/* Wake Word Handler Implementation

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "wakeword_handler.h"
#include "esp_log.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "model_path.h"
#include "media_lib_os.h"
#include <string.h>
#include <stdlib.h>

#define TAG "WAKEWORD"
#define WAKEWORD_TASK_STACK_SIZE (8 * 1024)
#define WAKEWORD_TASK_PRIORITY 5
#define WAKEWORD_FRAME_INTERVAL 20 // ms

struct wakeword_handler_s
{
    wakeword_config_t config;
    esp_capture_sink_handle_t sink;
    esp_afe_sr_iface_t *afe_handle;
    esp_afe_sr_data_t *afe_data;
    srmodel_list_t *models;
    wakeword_callback_t callback;
    void *callback_ctx;
    media_lib_thread_handle_t task_handle;
    bool running;
    bool initialized;
};

static void default_wakeword_callback(int model_index, int word_index, void *ctx)
{
    ESP_LOGI(TAG, "=== WAKE WORD DETECTED ===");
    ESP_LOGI(TAG, "Model index: %d, Word index: %d", model_index, word_index);
    ESP_LOGI(TAG, "=========================");
}

static int initialize_afe_models(wakeword_handler_t *handler)
{
    // Initialize SR models (similar to your working example)
    handler->models = esp_srmodel_init("model");
    if (handler->models == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize SR models");
        return -1;
    }

    // Print available wake word models
    for (int i = 0; i < handler->models->num; i++)
    {
        if (strstr(handler->models->model_name[i], ESP_WN_PREFIX) != NULL)
        {
            ESP_LOGI(TAG, "Available wake word model: %s", handler->models->model_name[i]);
        }
    }

    // Create AFE config - using correct input format parameter
    // Common formats: "16000_16_1" (16kHz, 16-bit, mono) or "16000_32_2" (16kHz, 32-bit, stereo)
    const char *input_format = "16000_16_1"; // Adjust based on your audio input configuration

    afe_config_t *afe_config = afe_config_init(
        input_format, // Use string format instead of function call
        handler->models,
        AFE_TYPE_SR,
        AFE_MODE_LOW_COST);

    if (afe_config == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize AFE config");
        return -1;
    }

    // Print configured wake word models
    if (afe_config->wakenet_model_name)
    {
        ESP_LOGI(TAG, "Primary wake word model: %s", afe_config->wakenet_model_name);
    }
    if (afe_config->wakenet_model_name_2)
    {
        ESP_LOGI(TAG, "Secondary wake word model: %s", afe_config->wakenet_model_name_2);
    }

    // Create AFE handle and data structures
    handler->afe_handle = esp_afe_handle_from_config(afe_config);
    if (handler->afe_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create AFE handle");
        afe_config_free(afe_config);
        return -1;
    }

    handler->afe_data = handler->afe_handle->create_from_config(afe_config);
    if (handler->afe_data == NULL)
    {
        ESP_LOGE(TAG, "Failed to create AFE data");
        afe_config_free(afe_config);
        return -1;
    }

    // Clean up config (no longer needed)
    afe_config_free(afe_config);

    // Set wake word detection thresholds (optional tuning)
    handler->afe_handle->set_wakenet_threshold(handler->afe_data, 1, 0.6f);
    handler->afe_handle->set_wakenet_threshold(handler->afe_data, 2, 0.6f);

    ESP_LOGI(TAG, "AFE models initialized successfully");
    return 0;
}

static void wakeword_task(void *arg)
{
    wakeword_handler_t *handler = (wakeword_handler_t *)arg;

    ESP_LOGI(TAG, "Wake word detection task started");

    while (handler->running)
    {
        esp_capture_stream_frame_t frame = {
            .stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO,
        };

        // Acquire audio frame from the capture sink
        if (esp_capture_sink_acquire_frame(handler->sink, &frame, true) == ESP_CAPTURE_ERR_OK)
        {
            // Feed audio data to AFE for processing
            if (handler->afe_handle && handler->afe_data)
            {
                // AFE expects 16-bit samples
                int16_t *audio_samples = (int16_t *)frame.data;
                int sample_count = frame.size / sizeof(int16_t);

                // Feed audio in chunks if needed
                int chunk_size = handler->afe_handle->get_feed_chunksize(handler->afe_data);

                for (int i = 0; i < sample_count; i += chunk_size)
                {
                    int remaining = sample_count - i;
                    // Feed the audio chunk to AFE
                    handler->afe_handle->feed(handler->afe_data, &audio_samples[i]);

                    // Fetch detection results
                    afe_fetch_result_t *result = handler->afe_handle->fetch(handler->afe_data);
                    if (result && result->ret_value != ESP_FAIL)
                    {
                        if (result->wakeup_state == WAKENET_DETECTED)
                        {
                            ESP_LOGI(TAG, "Wake word detected!");

                            // Call the callback function
                            if (handler->callback)
                            {
                                handler->callback(result->wakenet_model_index,
                                                  result->wake_word_index,
                                                  handler->callback_ctx);
                            }
                        }
                    }
                }
            }

            // Release the frame back to the capture system
            esp_capture_sink_release_frame(handler->sink, &frame);
        }

        // Small delay to prevent excessive CPU usage
        media_lib_thread_sleep(WAKEWORD_FRAME_INTERVAL);
    }

    ESP_LOGI(TAG, "Wake word detection task stopped");
    media_lib_thread_destroy(NULL);
}

wakeword_handler_t *wakeword_handler_create(wakeword_config_t *config)
{
    if (config == NULL)
    {
        ESP_LOGE(TAG, "Invalid config parameter");
        return NULL;
    }

    wakeword_handler_t *handler = calloc(1, sizeof(wakeword_handler_t));
    if (handler == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate wake word handler");
        return NULL;
    }

    // Copy configuration
    handler->config = *config;
    handler->callback = default_wakeword_callback; // Set default callback
    handler->running = false;
    handler->initialized = false;

    // Initialize AFE models
    if (initialize_afe_models(handler) != 0)
    {
        ESP_LOGE(TAG, "Failed to initialize AFE models");
        free(handler);
        return NULL;
    }

    handler->initialized = true;
    ESP_LOGI(TAG, "Wake word handler created successfully");
    return handler;
}

int wakeword_handler_start(wakeword_handler_t *handler, esp_capture_sink_handle_t sink)
{
    if (handler == NULL || sink == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }

    if (!handler->initialized)
    {
        ESP_LOGE(TAG, "Wake word handler not properly initialized");
        return -1;
    }

    if (handler->running)
    {
        ESP_LOGW(TAG, "Wake word handler already running");
        return 0;
    }

    handler->sink = sink;
    handler->running = true;

    // Create the wake word detection task
    int ret = media_lib_thread_create_from_scheduler(
        &handler->task_handle,
        "wakeword_task",
        wakeword_task,
        handler);

    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to create wake word task: %d", ret);
        handler->running = false;
        return ret;
    }

    ESP_LOGI(TAG, "Wake word handler started");
    return 0;
}

int wakeword_handler_stop(wakeword_handler_t *handler)
{
    if (handler == NULL)
    {
        return -1;
    }

    if (!handler->running)
    {
        return 0; // Already stopped
    }

    handler->running = false;

    // Wait for task to finish (with timeout)
    if (handler->task_handle)
    {
        // Task will destroy itself
        handler->task_handle = NULL;
    }

    ESP_LOGI(TAG, "Wake word handler stopped");
    return 0;
}

int wakeword_handler_set_callback(wakeword_handler_t *handler, wakeword_callback_t callback, void *ctx)
{
    if (handler == NULL)
    {
        return -1;
    }

    handler->callback = callback ? callback : default_wakeword_callback;
    handler->callback_ctx = ctx;

    return 0;
}

void wakeword_handler_destroy(wakeword_handler_t *handler)
{
    if (handler == NULL)
    {
        return;
    }

    // Stop the handler first
    wakeword_handler_stop(handler);

    // Clean up AFE resources
    if (handler->afe_data && handler->afe_handle)
    {
        // Note: esp-sr doesn't provide explicit cleanup functions
        // The memory will be cleaned up when the process exits
        handler->afe_data = NULL;
    }

    handler->afe_handle = NULL;

    // Clean up models
    if (handler->models)
    {
        // Note: esp_srmodel_init doesn't provide explicit cleanup
        handler->models = NULL;
    }

    free(handler);
    ESP_LOGI(TAG, "Wake word handler destroyed");
}