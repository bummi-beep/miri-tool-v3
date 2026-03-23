#include "rest_fs_browser.h"

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#include <cJSON.h>
#include <esp_log.h>

#include "base/sdmmc/sdmmc_init.h"
#include "base/spiflash_fs/spiflash_fs.h"

static const char *TAG = "http_fs";

static bool is_valid_name(const char *name) {
    if (!name || name[0] == '\0') {
        return false;
    }
    if (strstr(name, "..") != NULL) {
        return false;
    }
    for (const char *p = name; *p; p++) {
        if (*p == '/' || *p == '\\') {
            return false;
        }
    }
    return true;
}

static bool get_query_value(httpd_req_t *req, const char *key, char *out, size_t out_len) {
    char query[128];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return false;
    }
    if (httpd_query_key_value(query, key, out, out_len) != ESP_OK) {
        return false;
    }
    return true;
}

static esp_err_t resolve_fs(httpd_req_t *req, const char **mount_out, const char **label_out) {
    char fs[16] = {0};
    if (!get_query_value(req, "fs", fs, sizeof(fs))) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(fs, "sdmmc") == 0) {
        if (sdmmc_init() != ESP_OK || !sdmmc_is_ready()) {
            return ESP_FAIL;
        }
        *mount_out = sdmmc_get_mount();
        *label_out = "sdmmc";
        return ESP_OK;
    }
    if (strcmp(fs, "flash") == 0 || strcmp(fs, "spiflash") == 0) {
        if (spiflash_fs_init() != ESP_OK || !spiflash_fs_is_ready()) {
            return ESP_FAIL;
        }
        *mount_out = spiflash_fs_get_mount();
        *label_out = "flash";
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t read_json_body(httpd_req_t *req, cJSON **out_root) {
    size_t json_data_size = req->content_len;
    char *json_data = calloc(1, json_data_size + 1);
    if (!json_data) {
        return ESP_ERR_NO_MEM;
    }
    size_t recv_size = 0;
    while (recv_size < req->content_len) {
        int ret = httpd_req_recv(req, json_data + recv_size, req->content_len - recv_size);
        if (ret <= 0) {
            free(json_data);
            return ESP_FAIL;
        }
        recv_size += ret;
    }
    cJSON *root = cJSON_Parse(json_data);
    free(json_data);
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_root = root;
    return ESP_OK;
}

esp_err_t fs_ui_get_handler(httpd_req_t *req) {
    const char *fs = (const char *)req->user_ctx;
    if (!fs) {
        fs = "sdmmc";
    }
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req,
                             "<!DOCTYPE html><html><head><title>File Browser</title>"
                             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                             "<style>"
                             "body{font-family:Arial,Helvetica,sans-serif;margin:16px;}"
                             "table{border-collapse:collapse;width:100%;}"
                             "th,td{border:1px solid #888;padding:6px 8px;text-align:left;}"
                             "button{padding:6px 10px;margin:4px;}"
                             "</style></head><body>");
    httpd_resp_sendstr_chunk(req, fs[0] == 's' ? "<h1>SDMMC Files</h1>" : "<h1>SPI Flash Files</h1>");
    httpd_resp_sendstr_chunk(req,
                             "<div>"
                             "<input type=\"file\" id=\"file\">"
                             "<button onclick=\"uploadFile()\">Upload</button>"
                             "<button onclick=\"refresh()\">Refresh</button>"
                             "</div>"
                             "<p id=\"msg\">-</p>"
                             "<table><thead><tr><th>Name</th><th>Size</th><th>Action</th></tr></thead>"
                             "<tbody id=\"list\"></tbody></table>");
    httpd_resp_sendstr_chunk(req,
                             "<script>"
                             "const FS='" );
    httpd_resp_sendstr_chunk(req, fs);
    httpd_resp_sendstr_chunk(req,
                             "';"
                             "const msg=(t)=>document.getElementById('msg').textContent=t;"
                             "async function refresh(){"
                             "  const res=await fetch('/fs_list?fs='+FS);"
                             "  const data=await res.json();"
                             "  const body=document.getElementById('list');"
                             "  body.innerHTML='';"
                             "  if(!data.files){msg('list failed');return;}"
                             "  data.files.forEach(f=>{"
                             "    const tr=document.createElement('tr');"
                             "    tr.innerHTML=`<td>${f.name}</td><td>${f.size}</td>"
                             "      <td><button onclick=\"downloadFile('${f.name}')\">Download</button>"
                             "      <button onclick=\"deleteFile('${f.name}')\">Delete</button></td>`;"
                             "    body.appendChild(tr);"
                             "  });"
                             "  msg('OK');"
                             "}"
                             "async function uploadFile(){"
                             "  const f=document.getElementById('file').files[0];"
                             "  if(!f){msg('Select a file first.');return;}"
                             "  const res=await fetch('/fs_upload?fs='+FS+'&name='+encodeURIComponent(f.name),{method:'POST',body:f});"
                             "  msg(await res.text());"
                             "  refresh();"
                             "}"
                             "async function deleteFile(name){"
                             "  if(!confirm('Delete '+name+'?')) return;"
                             "  const res=await fetch('/fs_delete?fs='+FS,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name})});"
                             "  msg(await res.text());"
                             "  refresh();"
                             "}"
                             "function downloadFile(name){"
                             "  window.location='/fs_download?fs='+FS+'&name='+encodeURIComponent(name);"
                             "}"
                             "refresh();"
                             "</script>");
    httpd_resp_sendstr_chunk(req, "</body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

esp_err_t fs_list_get_handler(httpd_req_t *req) {
    const char *mount = NULL;
    const char *label = NULL;
    esp_err_t ret = resolve_fs(req, &mount, &label);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid fs");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "fs", label);
    cJSON *files = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "files", files);

    DIR *dir = opendir(mount);
    if (!dir) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "open dir failed");
        return ESP_FAIL;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        char path[384];
        struct stat st;
        snprintf(path, sizeof(path), "%s/%s", mount, ent->d_name);
        if (stat(path, &st) != 0 || S_ISDIR(st.st_mode)) {
            continue;
        }
        cJSON *file = cJSON_CreateObject();
        cJSON_AddStringToObject(file, "name", ent->d_name);
        cJSON_AddNumberToObject(file, "size", st.st_size);
        cJSON_AddNumberToObject(file, "mtime", st.st_mtime);
        cJSON_AddItemToArray(files, file);
    }
    closedir(dir);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free(json);
    return ESP_OK;
}

esp_err_t fs_upload_post_handler(httpd_req_t *req) {
    const char *mount = NULL;
    const char *label = NULL;
    esp_err_t ret = resolve_fs(req, &mount, &label);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid fs");
        return ESP_FAIL;
    }
    char name[128] = {0};
    if (!get_query_value(req, "name", name, sizeof(name)) || !is_valid_name(name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid name");
        return ESP_FAIL;
    }
    char path[384];
    snprintf(path, sizeof(path), "%s/%s", mount, name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "open file failed");
        return ESP_FAIL;
    }

    char buf[1024];
    int remaining = req->content_len;
    while (remaining > 0) {
        int recv = httpd_req_recv(req, buf, remaining > (int)sizeof(buf) ? (int)sizeof(buf) : remaining);
        if (recv <= 0) {
            fclose(f);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
            return ESP_FAIL;
        }
        fwrite(buf, 1, (size_t)recv, f);
        remaining -= recv;
    }
    fclose(f);
    httpd_resp_sendstr(req, "Upload OK");
    ESP_LOGI(TAG, "upload %s to %s", name, label);
    return ESP_OK;
}

esp_err_t fs_delete_post_handler(httpd_req_t *req) {
    const char *mount = NULL;
    const char *label = NULL;
    esp_err_t ret = resolve_fs(req, &mount, &label);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid fs");
        return ESP_FAIL;
    }
    char name[128] = {0};
    cJSON *root = NULL;
    if (read_json_body(req, &root) == ESP_OK) {
        cJSON *item = cJSON_GetObjectItem(root, "name");
        if (cJSON_IsString(item) && item->valuestring) {
            snprintf(name, sizeof(name), "%s", item->valuestring);
        }
        cJSON_Delete(root);
    }
    if (name[0] == '\0') {
        get_query_value(req, "name", name, sizeof(name));
    }
    if (!is_valid_name(name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid name");
        return ESP_FAIL;
    }
    char path[384];
    snprintf(path, sizeof(path), "%s/%s", mount, name);
    if (remove(path) != 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "delete failed");
        return ESP_FAIL;
    }
    httpd_resp_sendstr(req, "Delete OK");
    ESP_LOGI(TAG, "delete %s from %s", name, label);
    return ESP_OK;
}

esp_err_t fs_download_get_handler(httpd_req_t *req) {
    const char *mount = NULL;
    const char *label = NULL;
    esp_err_t ret = resolve_fs(req, &mount, &label);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid fs");
        return ESP_FAIL;
    }
    char name[128] = {0};
    if (!get_query_value(req, "name", name, sizeof(name)) || !is_valid_name(name)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid name");
        return ESP_FAIL;
    }
    char path[384];
    snprintf(path, sizeof(path), "%s/%s", mount, name);
    FILE *f = fopen(path, "rb");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "file not found");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/octet-stream");
    char disp[160];
    snprintf(disp, sizeof(disp), "attachment; filename=\"%s\"", name);
    httpd_resp_set_hdr(req, "Content-Disposition", disp);
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) {
            fclose(f);
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_FAIL;
        }
    }
    fclose(f);
    httpd_resp_sendstr_chunk(req, NULL);
    ESP_LOGI(TAG, "download %s from %s", name, label);
    return ESP_OK;
}
