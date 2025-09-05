/* OpenAI signaling

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "https_client.h"
#include "common.h"
#include "esp_log.h"
#include <cJSON.h>

#define TAG                   "OPENAI_SIGNALING"

// Realtime model + endpoints (calls API). Adjust if using different host/model.
#define OPENAI_REALTIME_MODEL "gpt-realtime"
#define OPENAI_REALTIME_URL   "https://api.openai.com/v1/realtime/calls?model=" OPENAI_REALTIME_MODEL
// Ephemeral token endpoint; default to OpenAI Realtime Sessions
#ifndef OPENAI_EPHEMERAL_URL
#define OPENAI_EPHEMERAL_URL  "https://api.openai.com/v1/realtime/sessions"
#endif

#define SAFE_FREE(p) if (p) {   \
    free(p);                    \
    p = NULL;                   \
}

#define GET_KEY_END(str, key) get_key_end(str, key, sizeof(key) - 1)

typedef struct {
    esp_peer_signaling_cfg_t cfg;
    uint8_t                 *remote_sdp;
    int                      remote_sdp_size;
    char                    *ephemeral_token;
} openai_signaling_t;

static char *get_key_end(char *str, char *key, int len)
{
    char *p = strstr(str, key);
    if (p == NULL) {
        return NULL;
    }
    return p + len;
}

static void session_answer(http_resp_t *resp, void *ctx)
{
    openai_signaling_t *sig = (openai_signaling_t *)ctx;
    if (resp == NULL || resp->data == NULL || resp->size <= 0) {
        return;
    }
    // Ensure null-terminated buffer
    char *buf = malloc(resp->size + 1);
    if (!buf) return;
    memcpy(buf, resp->data, resp->size);
    buf[resp->size] = '\0';

    // Trim leading/trailing whitespace
    char *s = buf;
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\n' || end[-1] == '\r' || end[-1] == '\t')) {
        *--end = '\0';
    }

    char *token_out = NULL;
    if (*s == '{') {
        // JSON: support both { client_secret: { value } } and { value }
        cJSON *root = cJSON_Parse(s);
        if (root) {
            cJSON *client_secret = cJSON_GetObjectItemCaseSensitive(root, "client_secret");
            if (client_secret && cJSON_IsObject(client_secret)) {
                cJSON *value = cJSON_GetObjectItemCaseSensitive(client_secret, "value");
                if (value && cJSON_IsString(value)) {
                    token_out = strdup(value->valuestring);
                }
            }
            if (!token_out) {
                cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "value");
                if (value && cJSON_IsString(value)) {
                    token_out = strdup(value->valuestring);
                }
            }
            if (!token_out) {
                // Sometimes the token may be in "secret" or "token"
                cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "secret");
                if (value && cJSON_IsString(value)) {
                    token_out = strdup(value->valuestring);
                }
            }
            cJSON_Delete(root);
        }
    } else {
        // Plain token string, possibly quoted
        if (*s == '"') {
            s++;
            char *q = strchr(s, '"');
            if (q) *q = '\0';
        }
        token_out = strdup(s);
    }

    if (token_out && token_out[0] != '\0') {
        sig->ephemeral_token = token_out;
    } else {
        int show = 200;
        if ((int)strlen(s) < show) show = strlen(s);
        ESP_LOGE(TAG, "Failed to parse ephemeral token. Body preview: %.*s", show, s);
        free(token_out);
    }
    free(buf);
}

static void get_ephemeral_token(openai_signaling_t *sig, char *token, char *voice, char *instructions)
{
    char content_type[]    = "Content-Type: application/json";
    int len = strlen("Authorization: Bearer ") + (token ? strlen(token) : 0) + 1;
    char auth[256] = {0};
    if (token && *token) {
        snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token);
    }
    char accept[]          = "Accept: text/plain, application/json";
    char accept_encoding[] = "Accept-Encoding: identity";
    char connection[]      = "Connection: close";
    char user_agent[]      = "User-Agent: esp32-esp-webrtc";
    char beta[]            = "OpenAI-Beta: realtime=v1";
    char *header[8];
    int h = 0;
    header[h++] = content_type;
    if (auth[0]) header[h++] = auth;
    header[h++] = accept;
    header[h++] = accept_encoding;
    header[h++] = connection;
    header[h++] = user_agent;
    header[h++] = beta;
    header[h++] = NULL;
    // Build body for ephemeral creation. Use legacy shape for /realtime/sessions,
    // and {"session":{...}} for custom endpoints.
    cJSON *root = cJSON_CreateObject();
    if (strstr(OPENAI_EPHEMERAL_URL, "/realtime/sessions")) {
        cJSON_AddStringToObject(root, "model", OPENAI_REALTIME_MODEL);
        cJSON *modalities = cJSON_CreateArray();
        cJSON_AddItemToArray(modalities, cJSON_CreateString("text"));
        cJSON_AddItemToArray(modalities, cJSON_CreateString("audio"));
        cJSON_AddItemToObject(root, "modalities", modalities);
        cJSON_AddStringToObject(root, "voice", voice);
        if (instructions && *instructions) {
            cJSON_AddStringToObject(root, "instructions", instructions);
        }
    } else {
        cJSON *session = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "session", session);
        cJSON_AddStringToObject(session, "model", OPENAI_REALTIME_MODEL);
        // Add audio.output.voice
        cJSON *audio = cJSON_CreateObject();
        cJSON *output = cJSON_CreateObject();
        cJSON_AddItemToObject(audio, "output", output);
        cJSON_AddStringToObject(output, "voice", voice);
        cJSON_AddItemToObject(session, "audio", audio);
        if (instructions && *instructions) {
            cJSON_AddStringToObject(session, "instructions", instructions);
        }
    }
    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string) {
        https_post(OPENAI_EPHEMERAL_URL, header, json_string, NULL, session_answer, sig);
        free(json_string);
    }
    cJSON_Delete(root);
}

static int openai_signaling_start(esp_peer_signaling_cfg_t *cfg, esp_peer_signaling_handle_t *h)
{
    openai_signaling_t *sig = (openai_signaling_t *)calloc(1, sizeof(openai_signaling_t));
    if (sig == NULL) {
        return ESP_PEER_ERR_NO_MEM;
    }
    openai_signaling_cfg_t *openai_cfg = (openai_signaling_cfg_t *)cfg->extra_cfg;
    sig->cfg = *cfg;
    // alloy, ash, ballad, coral, echo sage, shimmer and verse
    get_ephemeral_token(sig,
                        openai_cfg->token,
                        openai_cfg->voice ? openai_cfg->voice : "marin",
                        openai_cfg->instructions);
    if (sig->ephemeral_token == NULL) {
        free(sig);
        return ESP_PEER_ERR_NOT_SUPPORT;
    }
    *h = sig;
    esp_peer_signaling_ice_info_t ice_info = {
        .is_initiator = true,
    };
    sig->cfg.on_ice_info(&ice_info, sig->cfg.ctx);
    sig->cfg.on_connected(sig->cfg.ctx);
    return ESP_PEER_ERR_NONE;
}

static void openai_sdp_answer(http_resp_t *resp, void *ctx)
{
    openai_signaling_t *sig = (openai_signaling_t *)ctx;
    printf("Get remote SDP %s\n", (char *)resp->data);
    SAFE_FREE(sig->remote_sdp);
    sig->remote_sdp = (uint8_t *)malloc(resp->size);
    if (sig->remote_sdp == NULL) {
        ESP_LOGE(TAG, "No enough memory for remote sdp");
        return;
    }
    memcpy(sig->remote_sdp, resp->data, resp->size);
    sig->remote_sdp_size = resp->size;
}

static int openai_signaling_send_msg(esp_peer_signaling_handle_t h, esp_peer_signaling_msg_t *msg)
{
    openai_signaling_t *sig = (openai_signaling_t *)h;
    if (msg->type == ESP_PEER_SIGNALING_MSG_BYE) {

    } else if (msg->type == ESP_PEER_SIGNALING_MSG_SDP) {
        printf("Receive local SDP\n");
        char content_type[32] = "Content-Type: application/sdp";
        char *token = sig->ephemeral_token;
        int len = strlen("Authorization: Bearer ") + strlen(token) + 1;
        char auth[len];
        snprintf(auth, len, "Authorization: Bearer %s", token);
        char accept_sdp[]      = "Accept: application/sdp";
        char accept_encoding[] = "Accept-Encoding: identity";
        char connection[]      = "Connection: close";
        char user_agent[]      = "User-Agent: esp32-esp-webrtc";
        char beta[]            = "OpenAI-Beta: realtime=v1";
        char *header[] = {
            content_type,
            auth,
            accept_sdp,
            accept_encoding,
            connection,
            user_agent,
            beta,
            NULL,
        };
        int ret = https_post(OPENAI_REALTIME_URL, header, (char *)msg->data, NULL, openai_sdp_answer, h);
        if (ret != 0 || sig->remote_sdp == NULL) {
            ESP_LOGE(TAG, "Fail to post data to %s", OPENAI_REALTIME_URL);
            return -1;
        }
        esp_peer_signaling_msg_t sdp_msg = {
            .type = ESP_PEER_SIGNALING_MSG_SDP,
            .data = sig->remote_sdp,
            .size = sig->remote_sdp_size,
        };
        sig->cfg.on_msg(&sdp_msg, sig->cfg.ctx);
    }
    return 0;
}

static int openai_signaling_stop(esp_peer_signaling_handle_t h)
{
    openai_signaling_t *sig = (openai_signaling_t *)h;
    sig->cfg.on_close(sig->cfg.ctx);
    SAFE_FREE(sig->remote_sdp);
    SAFE_FREE(sig->ephemeral_token);
    SAFE_FREE(sig);
    return 0;
}

const esp_peer_signaling_impl_t *esp_signaling_get_openai_signaling(void)
{
    static const esp_peer_signaling_impl_t impl = {
        .start = openai_signaling_start,
        .send_msg = openai_signaling_send_msg,
        .stop = openai_signaling_stop,
    };
    return &impl;
}
