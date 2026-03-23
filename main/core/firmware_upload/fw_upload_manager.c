#include "fw_upload_manager.h"

#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <esp_log.h>

#include "core/firmware_upload/fw_upload_registry.h"
#include "core/firmware_upload/fw_upload_storage.h"
#include "core/firmware_upload/fw_upload_meta_defaults.h"
#include "core/firmware_upload/fw_update_worker.h"
#include "core/esp32_update/esp32_update.h"
#include "core/firmware_upload/fw_upload_state.h"
#include "base/sdmmc/sdmmc_init.h"
#include "config/storage_paths.h"

esp_err_t fw_upload_prepare_meta(fw_meta_t *meta, const char *name, const char *type, fw_format_t format) {
    if (!meta || !name || !type) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(meta, 0, sizeof(*meta));
    strncpy(meta->file_name, name, sizeof(meta->file_name) - 1);
    strncpy(meta->file_type, type, sizeof(meta->file_type) - 1);

    /* file_type 기반 fw_meta_t 기본값 먼저 적용 */
    fw_meta_init_defaults(meta);

    /* 호출자가 format 을 명시한 경우 그 값으로 덮어쓴다. */
    if (format != FW_FMT_UNKNOWN) {
        meta->format = format;
        if (meta->exec_format == FW_FMT_UNKNOWN) {
            meta->exec_format = meta->format;
        }
    }
    return ESP_OK;
}

esp_err_t fw_upload_save_buffer(const fw_meta_t *meta, const uint8_t *data, size_t len) {
    if (!meta || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    fw_storage_writer_t writer;
    fw_meta_t tmp = *meta;
    tmp.original_size = len;
    fw_state_set_step(FW_STEP_TRANSFER, "store buffer");
    if (fw_storage_writer_open(&writer, &tmp) != ESP_OK) {
        return ESP_FAIL;
    }
    if (fw_storage_writer_write(&writer, data, len) != ESP_OK) {
        fw_storage_writer_close(&writer);
        return ESP_FAIL;
    }
    if (fw_storage_writer_close(&writer) != ESP_OK) {
        return ESP_FAIL;
    }
    fw_state_set_step(FW_STEP_DONE, "store done");
    return ESP_OK;
}

esp_err_t fw_upload_session_begin(fw_upload_session_t *session, const fw_meta_t *meta, size_t total_size) {
    if (!session || !meta || total_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(session, 0, sizeof(*session));
    session->meta = *meta;
    session->meta.original_size = total_size;
    session->received = 0;
    fw_state_set_step(FW_STEP_TRANSFER, "store stream");
    fw_state_set_progress(0);
    return fw_storage_writer_open(&session->writer, &session->meta);
}

esp_err_t fw_upload_session_write(fw_upload_session_t *session, const uint8_t *data, size_t len) {
    if (!session || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = fw_storage_writer_write(&session->writer, data, len);
    if (ret == ESP_OK) {
        session->received += len;
        if (session->meta.original_size > 0) {
            uint32_t percent = (uint32_t)((session->received * 100U) / session->meta.original_size);
            if (percent > 100U) {
                percent = 100U;
            }
            fw_state_set_progress((uint8_t)percent);
        }
    }
    return ret;
}

esp_err_t fw_upload_session_finish(fw_upload_session_t *session) {
    if (!session) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = fw_storage_writer_close(&session->writer);
    if (ret == ESP_OK) {
        fw_state_set_step(FW_STEP_DONE, "store done");
        fw_state_set_progress(100);
    }
    return ret;
}

static void fw_get_filename_without_extension(const char *name, char *out, size_t out_size) {
    const char *dot = strrchr(name, '.');
    size_t len = dot ? (size_t)(dot - name) : strlen(name);
    if (len >= out_size) {
        len = out_size - 1;
    }
    memcpy(out, name, len);
    out[len] = '\0';
}

static const char *fw_get_filename_ext(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) {
        return "";
    }
    return dot + 1;
}

static void fw_trim_whitespace(char *s) {
    if (!s) {
        return;
    }
    char *start = s;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

static bool fw_split_name_type(const char *input_name, const char *input_type,
                               char *out_name, size_t out_name_size,
                               char *out_type, size_t out_type_size) {
    if (!input_name || !out_name || !out_type) {
        return false;
    }
    const char *dot = strrchr(input_name, '.');
    if (dot && dot[1] != '\0') {
        size_t name_len = (size_t)(dot - input_name);
        if (name_len >= out_name_size) {
            name_len = out_name_size - 1;
        }
        memcpy(out_name, input_name, name_len);
        out_name[name_len] = '\0';
        snprintf(out_type, out_type_size, "%s", dot + 1);
        return true;
    }
    snprintf(out_name, out_name_size, "%s", input_name);
    snprintf(out_type, out_type_size, "%s", input_type ? input_type : "");
    return true;
}

static bool fw_find_sdmmc_file(const char *name, const char *type,
                               char *out_name, size_t out_name_size,
                               char *out_type, size_t out_type_size) {
    DIR *dir = opendir(BASE_FILESYSTEM_PATH_SDMMC "/");
    if (!dir) {
        return false;
    }
    struct dirent *ent;
    char fallback_name[64] = {0};
    char fallback_type[8] = {0};
    while ((ent = readdir(dir)) != NULL) {
        const char *ext = fw_get_filename_ext(ent->d_name);
        if (strlen(ext) != 3) {
            continue;
        }
        char name_buf[64];
        fw_get_filename_without_extension(ent->d_name, name_buf, sizeof(name_buf));
        if (strcasecmp(name_buf, name) == 0 && strcasecmp(ext, type) == 0) {
            snprintf(out_name, out_name_size, "%s", name_buf);
            snprintf(out_type, out_type_size, "%s", ext);
            closedir(dir);
            return true;
        }
        if (fallback_name[0] == '\0' && strcasecmp(name_buf, name) == 0) {
            snprintf(fallback_name, sizeof(fallback_name), "%s", name_buf);
            snprintf(fallback_type, sizeof(fallback_type), "%s", ext);
        }
    }
    if (fallback_name[0] != '\0') {
        snprintf(out_name, out_name_size, "%s", fallback_name);
        snprintf(out_type, out_type_size, "%s", fallback_type);
        closedir(dir);
        return true;
    }
    closedir(dir);
    return false;
}

esp_err_t fw_upload_run_from_sdmmc(const char *name, const char *type, bool *out_started) {
    if (!name || !type) {
        fw_state_set_step(FW_STEP_ERROR, "invalid args");
        return ESP_ERR_INVALID_ARG;
    }
    char name_buf[64];
    char type_buf[8];
    fw_split_name_type(name, type, name_buf, sizeof(name_buf), type_buf, sizeof(type_buf));
    fw_trim_whitespace(name_buf);
    fw_trim_whitespace(type_buf);
    if (out_started) {
        *out_started = false;
    }


    fw_meta_t meta;
    fw_route_t route;
    bool have_route = fw_registry_lookup(type_buf, &route);
    bool has_meta = (fw_storage_meta_read(name_buf, type_buf, &meta) == ESP_OK);
    if (!has_meta) {
        memset(&meta, 0, sizeof(meta));
        strncpy(meta.file_name, name_buf, sizeof(meta.file_name) - 1);
        strncpy(meta.file_type, type_buf, sizeof(meta.file_type) - 1);
        if (have_route) {
            meta.format = route.format;
            meta.exec = route.exec;
        } else {
            meta.format = FW_FMT_UNKNOWN;
            meta.exec = FW_EXEC_UNKNOWN;
        }
        meta.encrypted = false;
        meta.original_size = 0;
        char path[384];
        if (fw_storage_build_path(path, sizeof(path), name, type)) {
            struct stat st;
            if (stat(path, &st) == 0) {
                meta.original_size = (size_t)st.st_size;
            }
        }
        fw_storage_meta_write(&meta);
    } else if (have_route) {
        bool changed = false;
        if (!meta.exec_override && meta.exec != route.exec) {
            meta.exec = route.exec;
            changed = true;
        }
        if (meta.format == FW_FMT_UNKNOWN && route.format != FW_FMT_UNKNOWN) {
            meta.format = route.format;
            changed = true;
        }
        if (changed) {
            fw_storage_meta_write(&meta);
        }
    }

    if (meta.exec != FW_EXEC_ESP32_OTA) {
        if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
            fw_state_set_step(FW_STEP_ERROR, "SDMMC not ready");
            return ESP_FAIL;
        }
        if (!fw_find_sdmmc_file(name_buf, type_buf, name_buf, sizeof(name_buf),
                                type_buf, sizeof(type_buf))) {
            fw_state_set_step(FW_STEP_ERROR, "file not found");
            return ESP_FAIL;
        }
    }

    if (fw_update_worker_start() != ESP_OK) {
        fw_state_set_step(FW_STEP_ERROR, "update task start fail");
        return ESP_FAIL;
    }
    esp_err_t ret = fw_update_enqueue_job(&meta, name_buf, type_buf);
    if (ret == ESP_OK && out_started) {
        *out_started = true;
    }
    return ret;
}
