#include <stdint.h>
#include "bsp_audio.h"
#include "bsp_i2c.h"
#include "esp_log.h"
#include "fishing_sound.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#define PCM_CLIP(name) \
    extern const int16_t name##_start[] asm("_binary_" #name "_pcm_start"); \
    extern const int16_t name##_end[] asm("_binary_" #name "_pcm_end")
PCM_CLIP(welcome);
PCM_CLIP(cast);
PCM_CLIP(bite);
PCM_CLIP(land);
PCM_CLIP(miss);
PCM_CLIP(upgrade);
#define CLIP(name) {name##_start, name##_end}

typedef struct { const int16_t *start, *end; } clip_t;
static const clip_t clips[] = {
    {NULL, NULL}, CLIP(welcome), CLIP(cast), CLIP(bite),
    CLIP(land), CLIP(miss), CLIP(upgrade),
};
static QueueHandle_t requests;
static SemaphoreHandle_t codec_lock;
static bool ready, codec_initialized;

static void audio_task(void *arg)
{
    (void)arg;
    fishing_sound_t current = SOUND_STOP;
    const int16_t *cursor = NULL, *end = NULL;
    unsigned submitted = 0;
    for (;;) {
        fishing_sound_t next;
        if (xQueueReceive(requests, &next, 0) == pdTRUE) {
            if (current == SOUND_WELCOME && next == SOUND_STOP)
                ESP_LOGI("NABO-Sound", "welcome skipped after %u samples", submitted);
            current = next;
            cursor = clips[next].start;
            end = clips[next].end;
            submitted = 0;
        }

        // Match the badge's tested continuous DMA feed. The codec may apply
        // volume in place, including when the buffer contains only silence.
        int16_t buffer[320] = {0};
        unsigned samples = 0;
        if (current != SOUND_STOP) {
            samples = end - cursor;
            if (samples > 320) samples = 320;
            for (unsigned i = 0; i < samples; i++) buffer[i] = cursor[i];
        }
        xSemaphoreTake(codec_lock, portMAX_DELAY);
        esp_err_t result = bsp_audio_write(buffer, sizeof buffer);
        xSemaphoreGive(codec_lock);
        if (result != ESP_OK) {
            ESP_LOGE("NABO-Sound", "audio write failed: %s (cue %d, sent %u)",
                     esp_err_to_name(result), current, submitted);
            current = SOUND_STOP;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (current != SOUND_STOP) {
            cursor += samples;
            submitted += samples;
            if (cursor == end) {
                if (current == SOUND_WELCOME)
                    ESP_LOGI("NABO-Sound", "welcome sent to codec: %u samples", submitted);
                current = SOUND_STOP;
            }
        }
    }
}

bool fishing_sound_init(void)
{
    nvs_handle_t settings;
    int32_t volume = 82;
    uint8_t speaker = 1;
    if (nvs_open("nabo_ui", NVS_READONLY, &settings) == ESP_OK) {
        nvs_get_i32(settings, "volume", &volume);
        nvs_get_u8(settings, "speaker", &speaker);
        nvs_close(settings);
    }
    if (!speaker || volume <= 0) {
        ESP_LOGI("NABO-Sound", "saved speaker is muted");
        return true;
    }
    if (volume > 100) volume = 100;
    requests = xQueueCreate(1, sizeof(fishing_sound_t));
    codec_lock = xSemaphoreCreateMutex();
    if (!requests || !codec_lock) return false;
    codec_initialized = bsp_audio_init() == ESP_OK;
    if (!codec_initialized || bsp_audio_set_format(16000, 16, 1) != ESP_OK) return false;
    bsp_audio_set_volume((uint8_t)volume);
    ESP_LOGI("NABO-Sound", "speaker enabled, volume %ld", (long)volume);
    ready = xTaskCreate(audio_task, "fish_audio", 4096, NULL, 4, NULL) == pdPASS;
    return ready;
}

void fishing_sound_play(fishing_sound_t sound)
{
    if (ready && (unsigned)sound < sizeof clips / sizeof clips[0])
        xQueueOverwrite(requests, &sound);
}

void fishing_sound_sleep(void)
{
    if (!codec_initialized) return;
    if (ready) xSemaphoreTake(codec_lock, portMAX_DELAY);
    if (bsp_audio_sleep() != ESP_OK ||
        bsp_audio_prepare_deep_sleep() != ESP_OK ||
        bsp_i2c_prepare_deep_sleep() != ESP_OK)
        ESP_LOGW("NABO-Fishing", "audio sleep preparation did not complete");
    // The caller holds the codec lock until the chip enters deep sleep.
}
