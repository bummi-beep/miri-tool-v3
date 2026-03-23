#ifndef REST_FS_BROWSER_H
#define REST_FS_BROWSER_H

#include <esp_err.h>
#include <esp_http_server.h>

esp_err_t fs_ui_get_handler(httpd_req_t *req);
esp_err_t fs_list_get_handler(httpd_req_t *req);
esp_err_t fs_upload_post_handler(httpd_req_t *req);
esp_err_t fs_delete_post_handler(httpd_req_t *req);
esp_err_t fs_download_get_handler(httpd_req_t *req);

#endif
