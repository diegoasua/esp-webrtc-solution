/* Media system with Wake Word Integration

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "codec_init.h"
#include "codec_board.h"
#include "av_render.h"
#include "common.h"
#include "settings.h"
#include "media_lib_os.h"
#include "esp_timer.h"
#include "av_render_default.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_enc_default.h"
#include "esp_capture_defaults.h"
#include "esp_capture_sink.h"
#include "esp_log.h"
#include "wakeword_handler.h"

#define RET_ON_NULL(ptr, v)                                        \
    do                                                             \
    {                                                              \
        if (ptr == NULL)                                           \
        {                                                          \
            ESP_LOGE(TAG, "Memory allocate fail on %d", __LINE__); \
            return v;                                              \
        }                                                          \
    } while (0)

#define TAG "MEDIA_SYS"

typedef struct
{
    esp_capture_sink_handle_t capture_handle;
    esp_capture_audio_src_if_t *aud_src;
    esp_capture_sink_handle_t wakeword_sink;
    wakeword_handler_t *ww_handler;
} capture_system_t;

typedef struct
{
    audio_render_handle_t audio_render;
    av_render_handle_t player;
} player_system_t;

static capture_system_t capture_sys;
static player_system_t player_sys;

static int build_wakeword_system(void)
{
    // Setup wake word sink with raw PCM audio matching your capture system
    // Use same format as your capture system but for wake word processing
    esp_capture_sink_cfg_t ww_sink_cfg = {
        .audio_info = {
            .format_id = ESP_CAPTURE_FMT_ID_PCM, // Raw PCM for AFE processing
            .sample_rate = 16000,
            .channel = 4, // Match your ES7210 TDM configuration (4 channels)
            .bits_per_sample = 16,
        },
    };

    // Create wake word sink on a different stream index than WebRTC
    int ret = esp_capture_sink_setup(capture_sys.capture_handle, 2, &ww_sink_cfg, &capture_sys.wakeword_sink);
    if (ret != ESP_CAPTURE_ERR_OK)
    {
        ESP_LOGE(TAG, "Failed to setup wake word sink: %d", ret);
        return ret;
    }

    // Enable the wake word sink
    ret = esp_capture_sink_enable(capture_sys.wakeword_sink, ESP_CAPTURE_RUN_MODE_ALWAYS);
    if (ret != ESP_CAPTURE_ERR_OK)
    {
        ESP_LOGE(TAG, "Failed to enable wake word sink: %d", ret);
        return ret;
    }

    // Initialize wake word handler with proper configuration
    wakeword_config_t ww_config = {
        .sample_rate = 16000,
        .channel = 4, // Match the sink configuration
        .bits_per_sample = 16,
    };

    capture_sys.ww_handler = wakeword_handler_create(&ww_config);
    if (capture_sys.ww_handler == NULL)
    {
        ESP_LOGE(TAG, "Failed to create wake word handler");
        return -1;
    }

    ret = wakeword_handler_start(capture_sys.ww_handler, capture_sys.wakeword_sink);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "Failed to start wake word handler: %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "Wake word system initialized successfully");
    return 0;
}

static int build_capture_system(void)
{
    // For S3 when use ES7210 it use TDM mode second channel is reference data
    esp_capture_audio_aec_src_cfg_t codec_cfg = {
        .record_handle = get_record_handle(),
#if CONFIG_IDF_TARGET_ESP32S3
        .channel = 4,
        .channel_mask = 1 | 2,
#endif
    };
    // capture_sys.aud_src = esp_capture_new_audio_dev_src(&codec_cfg);
    capture_sys.aud_src = esp_capture_new_audio_aec_src(&codec_cfg);
    RET_ON_NULL(capture_sys.aud_src, -1);

    // Create capture system
    esp_capture_cfg_t cfg = {
        .sync_mode = ESP_CAPTURE_SYNC_MODE_AUDIO,
        .audio_src = capture_sys.aud_src,
    };
    int ret = esp_capture_open(&cfg, &capture_sys.capture_handle);
    if (ret != ESP_CAPTURE_ERR_OK)
    {
        ESP_LOGE(TAG, "Failed to open capture system: %d", ret);
        return ret;
    }

    esp_capture_start(capture_sys.capture_handle);
    ret = build_wakeword_system();
    if (ret != 0)
    {
        ESP_LOGW(TAG, "Wake word system initialization failed, continuing without it");
        // Don't fail the entire system if wake word fails
    }

    return 0;
}

static int build_player_system()
{
    i2s_render_cfg_t i2s_cfg = {
        .play_handle = get_playback_handle(),
    };
    player_sys.audio_render = av_render_alloc_i2s_render(&i2s_cfg);
    if (player_sys.audio_render == NULL)
    {
        ESP_LOGE(TAG, "Fail to create audio render");
        return -1;
    }
    esp_codec_dev_set_out_vol(i2s_cfg.play_handle, DEFAULT_PLAYBACK_VOL);
    av_render_cfg_t render_cfg = {
        .audio_render = player_sys.audio_render,
        .audio_raw_fifo_size = 8 * 4096,
        .audio_render_fifo_size = 100 * 1024,
        .allow_drop_data = false,
    };
    player_sys.player = av_render_open(&render_cfg);
    if (player_sys.player == NULL)
    {
        ESP_LOGE(TAG, "Fail to create player");
        return -1;
    }
    // When support AEC, reference data is from speaker right channel for ES8311 so must output 2 channel
    av_render_audio_frame_info_t aud_info = {
        .sample_rate = 16000,
        .channel = 2,
        .bits_per_sample = 16,
    };
    av_render_set_fixed_frame_info(player_sys.player, &aud_info);

    // Buffer 100ms data to avoid network not stable
    uint32_t audio_threshold = 0;
    audio_threshold *= aud_info.sample_rate * aud_info.channel * (aud_info.bits_per_sample >> 3) / 1000;
    av_render_set_audio_threshold(player_sys.player, audio_threshold);
    return 0;
}

int media_sys_buildup(void)
{
    // Register default audio encoder
    esp_audio_enc_register_default();
    // Register default audio decoder
    esp_audio_dec_register_default();
    // Build capture system
    build_capture_system();
    // Build player system
    build_player_system();
    return 0;
}

int media_sys_get_provider(esp_webrtc_media_provider_t *provide)
{
    provide->capture = capture_sys.capture_handle;
    provide->player = player_sys.player;
    return 0;
}

int media_sys_cleanup(void)
{
    if (capture_sys.ww_handler)
    {
        wakeword_handler_stop(capture_sys.ww_handler);
        wakeword_handler_destroy(capture_sys.ww_handler);
        capture_sys.ww_handler = NULL;
    }
    return 0;
}

int test_capture_to_player(void)
{
    esp_capture_sink_cfg_t sink_cfg = {
        .audio_info = {
            .format_id = ESP_CAPTURE_FMT_ID_OPUS,
            .sample_rate = 16000,
            .channel = 1,
            .bits_per_sample = 16,
        },
    };
    // Create capture
    esp_capture_sink_handle_t capture_path = NULL;
    esp_capture_sink_setup(capture_sys.capture_handle, 0, &sink_cfg, &capture_path);
    esp_capture_sink_enable(capture_path, ESP_CAPTURE_RUN_MODE_ALWAYS);
    // Create player
    av_render_audio_info_t render_aud_info = {
        .codec = AV_RENDER_AUDIO_CODEC_OPUS,
        .sample_rate = 16000,
        .channel = 1,
    };
    av_render_add_audio_stream(player_sys.player, &render_aud_info);

    uint32_t start_time = (uint32_t)(esp_timer_get_time() / 1000);
    esp_capture_start(capture_sys.capture_handle);
    while ((uint32_t)(esp_timer_get_time() / 1000) < start_time + 20000)
    {
        media_lib_thread_sleep(30);
        esp_capture_stream_frame_t frame = {
            .stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO,
        };
        while (esp_capture_sink_acquire_frame(capture_path, &frame, true) == ESP_CAPTURE_ERR_OK)
        {
            av_render_audio_data_t audio_data = {
                .data = frame.data,
                .size = frame.size,
                .pts = frame.pts,
            };
            av_render_add_audio_data(player_sys.player, &audio_data);
            esp_capture_sink_release_frame(capture_path, &frame);
        }
    }
    esp_capture_stop(capture_sys.capture_handle);
    av_render_reset(player_sys.player);
    return 0;
}