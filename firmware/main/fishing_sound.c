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
    for (;;) {
        fishing_sound_t next;
        if (xQueueReceive(requests, &next, current == SOUND_STOP ? portMAX_DELAY : 0) == pdTRUE) {
            current = next;
            cursor = clips[next].start;
            end = clips[next].end;
        }
        xSemaphoreTake(codec_lock, portMAX_DELAY);
        if (current == SOUND_STOP || cursor == end) {
            // Clear the tail of the DMA stream when skipping or ending a clip.
            static const int16_t silence[320] = {0};
            bsp_audio_write(silence, sizeof silence);
            current = SOUND_STOP;
        } else {
            int16_t buffer[320] = {0};
            size_t samples = end - cursor;
            if (samples > 320) samples = 320;
            for (size_t i = 0; i < samples; i++) buffer[i] = cursor[i];
            if (bsp_audio_write(buffer, sizeof buffer) != ESP_OK) current = SOUND_STOP;
            cursor += samples;
        }
        xSemaphoreGive(codec_lock);
    }
}

bool fishing_sound_init(void)
{
    nvs_handle_t settings;
    int32_t volume = 55;
    uint8_t speaker = 1;
    if (nvs_open("nabo_ui", NVS_READONLY, &settings) == ESP_OK) {
        nvs_get_i32(settings, "volume", &volume);
        nvs_get_u8(settings, "speaker", &speaker);
        nvs_close(settings);
    }
    if (!speaker || volume <= 0) return true;
    if (volume > 100) volume = 100;
    requests = xQueueCreate(1, sizeof(fishing_sound_t));
    codec_lock = xSemaphoreCreateMutex();
    if (!requests || !codec_lock) return false;
    codec_initialized = bsp_audio_init() == ESP_OK;
    if (!codec_initialized || bsp_audio_set_format(16000, 16, 1) != ESP_OK) return false;
    bsp_audio_set_volume((uint8_t)volume);
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
