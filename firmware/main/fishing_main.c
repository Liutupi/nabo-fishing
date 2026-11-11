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
#define SAVE_KEY "progress_v2"
#define IDLE_US INT64_C(300000000)

typedef struct { fish_button_t button; uint32_t ms; } input_t;
static QueueHandle_t inputs;
static atomic_bool overflow;
static fish_game_t game;
static bool dirty, save_error;
static int64_t last_input, retry_at;

static uint32_t now_ms(void) { return esp_timer_get_time() / 1000; }

static fish_save_t load_progress(void)
{
    uint32_t record[2 + FISH_SAVE_WORDS] = {0};
    size_t size = sizeof record;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("nabo_fish", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_blob(handle, SAVE_KEY, record, &size);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            size = sizeof record;
            err = nvs_get_blob(handle, "progress_v1", record, &size);
            if (err == ESP_OK && (size != 3 * sizeof(uint32_t) || record[2] > 63))
                err = ESP_ERR_INVALID_SIZE;
        } else if (err == ESP_OK && size != sizeof record) {
            err = ESP_ERR_INVALID_SIZE;
        }
        nvs_close(handle);
        if (err == ESP_OK && record[1] >= 1 && record[1] <= FISH_MAX_ROD && record[5] <= 15) {
            return (fish_save_t){.coins = record[0], .rod = record[1],
                .seen = {record[2], record[3], record[4], record[5]}};
        }
    }
    if (err != ESP_ERR_NVS_NOT_FOUND) ESP_LOGW(TAG, "save unavailable: %s", esp_err_to_name(err));
    return (fish_save_t){.rod = 1};
}

static void write_progress(int64_t now)
{
    const uint32_t record[] = {game.save.coins, game.save.rod,
        game.save.seen[0], game.save.seen[1], game.save.seen[2], game.save.seen[3]};
    nvs_handle_t handle;
    esp_err_t err = nvs_open("nabo_fish", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, SAVE_KEY, record, sizeof record);
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    dirty = save_error = err != ESP_OK;
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
        if (elapsed >= 2750) break;
        if (!spoke && elapsed >= 380) {
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
    bsp_display_backlight(80);
    ESP_ERROR_CHECK(bsp_button_init(button_event, NULL));
    if (!fishing_sound_init()) ESP_LOGW(TAG, "sound unavailable");
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
            fish_page_t old_page = game.page;
            uint8_t old_rod = game.save.rod;
            if (fish_press(&game, input.button, input.ms)) {
                dirty = true;
                retry_at = now;
            }
            if (game.save.rod != old_rod) fishing_sound_play(SOUND_UPGRADE);
            announce_page(old_page);
        }
        fish_page_t old_page = game.page;
        fish_tick(&game, now_ms());
        announce_page(old_page);
        if (dirty && now >= retry_at) write_progress(now);
        if (bsp_lvgl_lock(10)) {
            fishing_ui_draw(&game, now_ms(), save_error);
            bsp_lvgl_unlock();
        }
        if (!dirty && now - last_input >= IDLE_US) sleep_now();
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}
