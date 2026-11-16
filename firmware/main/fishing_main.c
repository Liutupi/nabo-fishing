#include <stdatomic.h>
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_random.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "fishing_ui.h"
#include "fishing_sound.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "nvs.h"

#define TAG "NABO-Fishing"
#define SAVE_KEY "progress_v5"
// Idle handling has two steps. After STANDBY_US the backlight goes dark but the
// game keeps running, so any key brings the screen back at once. Only after
// SLEEP_US does the badge enter deep sleep, which needs a firm key press and a
// reboot to leave. The aquarium is an ornament: it never dims or sleeps.
#define STANDBY_US INT64_C(120000000)
#define SLEEP_US INT64_C(1500000000)
#define BACKLIGHT 80

typedef struct { fish_button_t button; uint32_t ms; } input_t;
static QueueHandle_t inputs;
static atomic_bool overflow;
static fish_game_t game;
static bool dirty, save_error, standby, options_dirty;
static int64_t last_input, retry_at;

static void load_options(void)
{
    nvs_handle_t h;
    if (nvs_open("nabo_fish", NVS_READONLY, &h) == ESP_OK) {
        uint32_t packed;
        esp_err_t result = nvs_get_u32(h, "options_v1", &packed);
        nvs_close(h);
        if (result == ESP_OK && (packed & 255) >= 10 && (packed & 255) <= 100 &&
            ((packed >> 8) & 255) <= 100 && (packed >> 16) <= 6) {
            game.brightness = packed & 255;
            game.volume = (packed >> 8) & 255;
            game.sleep_index = packed >> 16;
            return;
        }
    }
    if (nvs_open("nabo_ui", NVS_READONLY, &h) != ESP_OK) return;
    int32_t value;
    uint8_t speaker;
    if (nvs_get_i32(h, "brightness", &value) == ESP_OK && value >= 10 && value <= 100)
        game.brightness = value;
    if (nvs_get_i32(h, "volume", &value) == ESP_OK && value >= 0 && value <= 100)
        game.volume = value;
    if (nvs_get_u8(h, "speaker", &speaker) == ESP_OK && !speaker) game.volume = 0;
    if (nvs_get_i32(h, "sleep", &value) == ESP_OK)
        for (unsigned i = 0; i < 7; i++)
            if (fish_sleep_seconds[i] == value) game.sleep_index = i;
    nvs_close(h);
}

static uint32_t now_ms(void) { return esp_timer_get_time() / 1000; }

static fish_save_t load_progress(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("nabo_fish", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        static const char *const keys[] = {SAVE_KEY, "progress_v4", "progress_v3", "progress_v2", "progress_v1"};
        static const unsigned counts[] = {FISH_RECORD_WORDS, FISH_RECORD_WORDS_V4, FISH_RECORD_WORDS_V3, 6, 3};
        for (unsigned i = 0; i < sizeof keys / sizeof keys[0]; i++) {
            uint32_t record[FISH_RECORD_WORDS];
            size_t size = sizeof record;
            err = nvs_get_blob(handle, keys[i], record, &size);
            if (err == ESP_ERR_NVS_NOT_FOUND) continue;
            fish_save_t save;
            if (err == ESP_OK && size == counts[i] * sizeof(uint32_t) &&
                fish_save_decode(&save, record, counts[i])) {
                nvs_close(handle);
                return save;
            }
            ESP_LOGW(TAG, "could not read %s", keys[i]);
        }
        nvs_close(handle);
    }
    return (fish_save_t){.rod = 1, .line = 1};
}

static void write_progress(int64_t now)
{
    uint32_t record[FISH_RECORD_WORDS];
    fish_save_encode(&game.save, record);
    nvs_handle_t handle;
    esp_err_t err = nvs_open("nabo_fish", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, SAVE_KEY, record, sizeof record);
        if (err == ESP_OK && options_dirty) {
            uint32_t packed = game.brightness | ((uint32_t)game.volume << 8) |
                              ((uint32_t)game.sleep_index << 16);
            err = nvs_set_u32(handle, "options_v1", packed);
        }
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    dirty = save_error = err != ESP_OK;
    if (!dirty) options_dirty = false;
    if (dirty) {
        retry_at = now + INT64_C(5000000);
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    }
}

static void button_event(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (event != BSP_BTN_PRESS) return;
    const fish_button_t map[] = {FISH_UP, FISH_DOWN, FISH_OK};
    if ((unsigned)button >= sizeof map / sizeof map[0]) return;
    input_t input = {.button = map[button], .ms = now_ms()};
    if (xQueueSend(inputs, &input, 0) != pdTRUE) atomic_store(&overflow, true);
}

static void announce_page(fish_page_t old_page)
{
    if (game.page == old_page) return;
    switch (game.page) {
        case FISH_WAIT: fishing_sound_play(SOUND_CAST); break;
        case FISH_BITE: fishing_sound_play(SOUND_BITE); break;
        case FISH_RESULT:
            fishing_sound_play(game.result == FISH_CAUGHT ? SOUND_LAND : SOUND_MISS);
            break;
        default: break;
    }
}

static void show_welcome(void)
{
    uint32_t started = now_ms();
    bool spoke = false;
    input_t skip;
    while (xQueueReceive(inputs, &skip, 0) != pdTRUE) {
        unsigned elapsed = now_ms() - started;
        if (elapsed >= 3400) break;
        if (!spoke && elapsed >= 640) {
            fishing_sound_play(SOUND_WELCOME);
            spoke = true;
        }
        if (bsp_lvgl_lock(20)) {
            fishing_ui_boot(elapsed, true);
            bsp_lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    fishing_sound_play(SOUND_STOP);
    if (bsp_lvgl_lock(1000)) {
        fishing_ui_boot(0, false);
        bsp_lvgl_unlock();
    }
}

static void sleep_now(void)
{
    if (bsp_button_read_mv() < BSP_BTN_MV_RELEASED_MIN) return;
    if (!bsp_lvgl_lock(20)) return;
    int level = 0;
    if (bsp_button_prepare_deep_sleep(&level) != ESP_OK || !level) esp_restart();
    if (esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown(
            1ULL << BSP_BTN_GPIO, ESP_GPIO_WAKEUP_GPIO_LOW) != ESP_OK) esp_restart();
    if (lvgl_port_stop() != ESP_OK) esp_restart();
    bsp_display_backlight(0);
    // The BSP panel command waits for queued SPI color transfers.
    if (bsp_display_prepare_deep_sleep() != ESP_OK) esp_restart();
    fishing_sound_sleep();
    esp_deep_sleep_start();
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    fish_init(&game, load_progress(), esp_random());
    load_options();
    inputs = xQueueCreate(24, sizeof(input_t));
    if (!inputs) { ESP_LOGE(TAG, "cannot allocate input queue"); return; }

    ESP_ERROR_CHECK(bsp_display_init());
    lv_display_t *display = bsp_lvgl_init();
    if (!display || !bsp_lvgl_lock(1000)) { ESP_LOGE(TAG, "cannot start display"); return; }
    fishing_ui_init();
    fishing_ui_draw(&game, now_ms(), false);
    bool wait_release = (esp_sleep_get_wakeup_causes() & (1u << ESP_SLEEP_WAKEUP_GPIO)) != 0;
    if (!wait_release) fishing_ui_boot(0, true);
    bsp_lvgl_unlock();
    bsp_display_backlight(game.brightness);
    ESP_ERROR_CHECK(bsp_button_init(button_event, NULL));
    if (!fishing_sound_init(game.volume)) ESP_LOGW(TAG, "sound unavailable");
    if (!wait_release) show_welcome();
    last_input = esp_timer_get_time();
    ESP_LOGI(TAG, "offline fishing ready; heap=%u", (unsigned)esp_get_free_heap_size());

    for (;;) {
        int64_t now = esp_timer_get_time();
        if (atomic_exchange(&overflow, false)) {
            xQueueReset(inputs);
            ESP_LOGW(TAG, "input queue overflow");
        }
        input_t input;
        if (wait_release) {
            xQueueReset(inputs);
            wait_release = bsp_button_read_mv() < BSP_BTN_MV_RELEASED_MIN;
        }
        while (xQueueReceive(inputs, &input, 0) == pdTRUE) {
            last_input = now;
            if (standby) {                      // this press only wakes the screen
                standby = false;
                bsp_display_backlight(game.brightness);
                continue;
            }
            fish_page_t old_page = game.page;
            uint8_t old_rod = game.save.rod, old_line = game.save.line;
            uint8_t brightness = game.brightness, volume = game.volume, sleep = game.sleep_index;
            if (fish_press(&game, input.button, input.ms)) {
                dirty = true;
                retry_at = now;
            }
            if (brightness != game.brightness) bsp_display_backlight(game.brightness);
            if (volume != game.volume) fishing_sound_volume(game.volume);
            if (brightness != game.brightness || volume != game.volume || sleep != game.sleep_index) {
                options_dirty = dirty = true;
                retry_at = now + 350000;
            }
            if (game.save.rod != old_rod || game.save.line != old_line) fishing_sound_play(SOUND_UPGRADE);
            announce_page(old_page);
        }
        fish_page_t old_page = game.page;
        fish_tick(&game, now_ms());
        announce_page(old_page);
        if (dirty && now >= retry_at) write_progress(now);
        if (fish_is_aquarium(game.page)) last_input = now;
        int64_t standby_us = (int64_t)fish_sleep_seconds[game.sleep_index] * 1000000;
        if (standby_us && !standby && now - last_input >= standby_us) {
            standby = true;
            bsp_display_backlight(0);
        }
        if (!standby && bsp_lvgl_lock(10)) {
            fishing_ui_draw(&game, now_ms(), save_error);
            bsp_lvgl_unlock();
        }
        if (standby_us && !dirty && now - last_input >= SLEEP_US) sleep_now();
        vTaskDelay(pdMS_TO_TICKS(standby ? 40 : 16));
    }
}
