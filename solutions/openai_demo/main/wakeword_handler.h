/* Wake Word Handler Header

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_capture_sink.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Wake word handler handle
     */
    typedef struct wakeword_handler_s wakeword_handler_t;

    /**
     * @brief Wake word configuration
     */
    typedef struct
    {
        uint32_t sample_rate;    /*!< Audio sample rate (typically 16000) */
        uint8_t channel;         /*!< Audio channel count (typically 1) */
        uint8_t bits_per_sample; /*!< Bits per sample (typically 16) */
    } wakeword_config_t;

    /**
     * @brief Wake word detection callback
     *
     * @param model_index  Wake word model index that was detected
     * @param word_index   Specific word index within the model
     * @param ctx          User context pointer
     */
    typedef void (*wakeword_callback_t)(int model_index, int word_index, void *ctx);

    /**
     * @brief Create wake word handler
     *
     * @param config  Wake word configuration
     * @return wakeword_handler_t* Wake word handler instance or NULL on failure
     */
    wakeword_handler_t *wakeword_handler_create(wakeword_config_t *config);

    /**
     * @brief Start wake word detection
     *
     * @param handler     Wake word handler instance
     * @param sink        Capture sink handle to read audio from
     * @return int        0 on success, negative on error
     */
    int wakeword_handler_start(wakeword_handler_t *handler, esp_capture_sink_handle_t sink);

    /**
     * @brief Stop wake word detection
     *
     * @param handler  Wake word handler instance
     * @return int     0 on success, negative on error
     */
    int wakeword_handler_stop(wakeword_handler_t *handler);

    /**
     * @brief Set wake word detection callback
     *
     * @param handler   Wake word handler instance
     * @param callback  Callback function to call when wake word is detected
     * @param ctx       User context pointer passed to callback
     * @return int      0 on success, negative on error
     */
    int wakeword_handler_set_callback(wakeword_handler_t *handler, wakeword_callback_t callback, void *ctx);

    /**
     * @brief Destroy wake word handler
     *
     * @param handler  Wake word handler instance
     */
    void wakeword_handler_destroy(wakeword_handler_t *handler);

#ifdef __cplusplus
}
#endif