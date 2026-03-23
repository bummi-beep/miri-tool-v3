#ifndef REST_COMMON_DELETE_H
#define REST_COMMON_DELETE_H

#include <esp_err.h>
#include <esp_http_server.h>

esp_err_t delete_sdmmc_post_handler(httpd_req_t *req);
esp_err_t delete_fat_post_handler(httpd_req_t *req);

#endif
