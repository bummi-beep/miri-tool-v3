#include "http_server.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_timer.h>

#include "base/sdmmc/sdmmc_init.h"
#include "base/runtime_config/runtime_config.h"
#include "base/spiflash_fs/spiflash_fs.h"
#include "config/version.h"

#include "base/net/http/sub/rest_common_delete.h"
#include "base/net/http/sub/rest_common_format.h"
#include "base/net/http/sub/rest_fat_get_list.h"
#include "base/net/http/sub/rest_get_fdc.h"
#include "base/net/http/sub/rest_sdmmc_fw_get_list.h"
#include "base/net/http/sub/rest_sdmmc_fw_run.h"
#include "base/net/http/sub/rest_sdmmc_fw_upload_run_mobile.h"
#include "base/net/http/sub/rest_sdmmc_fw_upload_raw.h"
#include "base/net/http/sub/rest_sdmmc_fw_meta.h"
#include "base/net/http/sub/rest_fw_state.h"
#include "base/net/http/sub/rest_fs_browser.h"

static const char *TAG = "http_server";

static const char *http_method_name(httpd_req_t *req) {
    if (!req) {
        return "?";
    }
    switch (req->method) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        case HTTP_PUT: return "PUT";
        case HTTP_DELETE: return "DELETE";
        default: return "?";
    }
}

static void log_http_request(httpd_req_t *req, const char *handler_name) {
    if (!req) {
        return;
    }
    char content_type[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) != ESP_OK) {
        content_type[0] = '\0';
    }
    ESP_LOGI(TAG, "HTTP %s %s handler=%s len=%d type=%s",
             http_method_name(req),
             req->uri ? req->uri : "-",
             handler_name ? handler_name : "-",
             (int)req->content_len,
             content_type[0] ? content_type : "-");
}

static esp_err_t sdmmc_fw_upload_run_logged(httpd_req_t *req) {
    log_http_request(req, "/fw_upload_run");
    return sdmmc_fw_upload_run_post_handler(req);
}

static esp_err_t sdmmc_fw_upload_raw_logged(httpd_req_t *req) {
    log_http_request(req, "/fw_upload_raw");
    return sdmmc_fw_upload_raw_post_handler(req);
}

static esp_err_t sdmmc_fw_meta_logged(httpd_req_t *req) {
    log_http_request(req, "/fw_meta");
    return sdmmc_fw_meta_post_handler(req);
}

static esp_err_t sdmmc_fw_run_logged(httpd_req_t *req) {
    log_http_request(req, "/fw_run");
    return sdmmc_fw_run_post_handler(req);
}

static void send_kv_row(httpd_req_t *req, const char *key, const char *value) {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "<tr><td>%.*s</td><td>%.*s</td></tr>",
             64, key ? key : "-", 128, value ? value : "-");
    httpd_resp_sendstr_chunk(req, buf);
}

static const char *wifi_status_string(void) {
    static char status[64];
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_err_t ret = esp_wifi_get_mode(&mode);
    if (ret != ESP_OK) {
        snprintf(status, sizeof(status), "FAIL (%s)", esp_err_to_name(ret));
        return status;
    }
    if (mode & WIFI_MODE_AP) {
        snprintf(status, sizeof(status), "OK (SoftAP)");
        return status;
    }
    snprintf(status, sizeof(status), "FAIL (mode=%d)", (int)mode);
    return status;
}

static const char *ble_status_string(void) {
#if CONFIG_BT_NIMBLE_ENABLED
    return "OK (NimBLE enabled)";
#else
    return "FAIL (NimBLE disabled)";
#endif
}

static esp_err_t hellow_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req,
                             "<!DOCTYPE html><html><head><title>MIRI-TOOL</title>"
                             "<meta http-equiv=\"refresh\" content=\"1\">"
                             "<style>"
                             "body{font-family:Arial,Helvetica,sans-serif;margin:16px;}"
                             ".uptime{background:#00c853;color:#000;font-weight:bold;padding:8px 12px;"
                             "display:inline-block;border-radius:6px;}"
                             "table{border-collapse:collapse;margin-top:8px;}"
                             "td{border:1px solid #888;padding:6px 10px;}"
                             "</style></head><body>");
    httpd_resp_sendstr_chunk(req, "<h1>MIRI-TOOL Web Server</h1>");

    uint64_t uptime_ms = esp_timer_get_time() / 1000ULL;
    uint64_t uptime_sec = uptime_ms / 1000ULL;
    uint64_t h = uptime_sec / 3600ULL;
    uint64_t m = (uptime_sec % 3600ULL) / 60ULL;
    uint64_t s = uptime_sec % 60ULL;
    char uptime_buf[64];
    snprintf(uptime_buf, sizeof(uptime_buf), "Uptime %02llu:%02llu:%02llu",
             (unsigned long long)h,
             (unsigned long long)m,
             (unsigned long long)s);
    httpd_resp_sendstr_chunk(req, "<div class=\"uptime\">");
    httpd_resp_sendstr_chunk(req, uptime_buf);
    httpd_resp_sendstr_chunk(req, "</div>");

    httpd_resp_sendstr_chunk(req, "<h2>Device Info</h2><table border=\"1\">");
    send_kv_row(req, "Project", PROJECT_NAME);
    send_kv_row(req, "Version", MIRI_TOOL_VERSION_STR);
    send_kv_row(req, "Build", __DATE__ " " __TIME__);
    send_kv_row(req, "Device Name", runtime_config_get_device_name());
    httpd_resp_sendstr_chunk(req, "</table>");

    httpd_resp_sendstr_chunk(req, "<h2>Base Status</h2><table border=\"1\">");
    send_kv_row(req, "FATFS (/c)", spiflash_fs_is_ready() ? "OK" : "FAIL");
    send_kv_row(req, "SDMMC (/sdcard)", sdmmc_is_ready() ? "OK" : "FAIL");
    send_kv_row(req, "Wi-Fi", wifi_status_string());
    send_kv_row(req, "BLE", ble_status_string());
    send_kv_row(req, "HTTP Server", "OK");
    httpd_resp_sendstr_chunk(req, "</table>");

    httpd_resp_sendstr_chunk(req, "<h2>Endpoints</h2><ul>");
    httpd_resp_sendstr_chunk(req, "<li>GET /upload</li>");
    httpd_resp_sendstr_chunk(req, "<li>GET /fw_run_ui</li>");
    httpd_resp_sendstr_chunk(req, "<li>POST /fw_upload_run</li>");
    httpd_resp_sendstr_chunk(req, "<li>POST /fw_upload_raw</li>");
    httpd_resp_sendstr_chunk(req, "<li>POST /fw_meta</li>");
    httpd_resp_sendstr_chunk(req, "<li>POST /fw_run</li>");
    httpd_resp_sendstr_chunk(req, "<li>GET /fw_state</li>");
    httpd_resp_sendstr_chunk(req, "<li>GET /sdmmc_fw_get_list</li>");
    httpd_resp_sendstr_chunk(req, "<li>GET /fat_get_list</li>");
    httpd_resp_sendstr_chunk(req, "<li>GET /sdmmc_format</li>");
    httpd_resp_sendstr_chunk(req, "<li>GET /fat_format</li>");
    httpd_resp_sendstr_chunk(req, "<li>POST /sdmmc_file_delete</li>");
    httpd_resp_sendstr_chunk(req, "<li>POST /fat_file_delete</li>");
    httpd_resp_sendstr_chunk(req, "<li>GET /get_fdc</li>");
    httpd_resp_sendstr_chunk(req, "<li>GET /sdmmc</li>");
    httpd_resp_sendstr_chunk(req, "<li>GET /flash</li>");
    httpd_resp_sendstr_chunk(req, "<li>GET /fs_list?fs=sdmmc|flash</li>");
    httpd_resp_sendstr_chunk(req, "<li>POST /fs_upload?fs=sdmmc|flash&name=...</li>");
    httpd_resp_sendstr_chunk(req, "<li>POST /fs_delete?fs=sdmmc|flash</li>");
    httpd_resp_sendstr_chunk(req, "<li>GET /fs_download?fs=sdmmc|flash&name=...</li>");
    httpd_resp_sendstr_chunk(req, "</ul>");

    httpd_resp_sendstr_chunk(req, "</body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t upload_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req,
                             "<!DOCTYPE html><html><head><title>MIRI-TOOL Upload</title>"
                             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                             "<style>"
                             "body{font-family:Arial,Helvetica,sans-serif;margin:16px;}"
                             "label{display:block;margin-top:12px;}"
                             "input,select,button{font-size:16px;padding:6px 8px;margin-top:6px;}"
                             "#log{white-space:pre-wrap;background:#f4f4f4;padding:8px;border-radius:6px;}"
                             "</style></head><body>");
    httpd_resp_sendstr_chunk(req, "<h1>SDMMC Firmware Upload</h1>");
    httpd_resp_sendstr_chunk(req,
                             "<label>Firmware file (.bin/.hex/.coff/.elf)"
                             "<input type=\"file\" id=\"file\" accept=\".bin,.hex,.coff,.elf\"></label>");
    httpd_resp_sendstr_chunk(req,
                             "<label>File type"
                             "<select id=\"fileType\">"
                             "<option>S00</option><option>S01</option>"
                             "<option>A00</option><option>A01</option><option>A02</option><option>A03</option>"
                             "<option>A04</option><option>A05</option><option>A06</option><option>A07</option>"
                             "<option>A08</option><option>A09</option><option>A10</option><option>A11</option>"
                             "<option>A12</option><option>A13</option><option>A14</option><option>A15</option>"
                             "</select></label>");
    httpd_resp_sendstr_chunk(req,
                             "<label>File format"
                             "<select id=\"fileFormat\">"
                             "<option>BIN</option><option>IHEX</option><option>COFF</option><option>ELF</option>"
                             "</select></label>");
    httpd_resp_sendstr_chunk(req,
                             "<label>Upgrade interface"
                             "<select id=\"upgradeExec\">"
                             "<option value=\"AUTO\">AUTO</option>"
                             "<option value=\"ESP32_OTA\">ESP32_OTA</option>"
                             "<option value=\"CAN\">CAN</option>"
                             "<option value=\"UART_ISP\">UART_ISP</option>"
                             "<option value=\"SWD\">SWD</option>"
                             "<option value=\"JTAG\">JTAG</option>"
                             "</select></label>");
    httpd_resp_sendstr_chunk(req,
                             "<div id=\"paramCan\" style=\"display:none;\">"
                             "<label>CAN bitrate"
                             "<input type=\"text\" id=\"canBitrate\" placeholder=\"1000000\"></label>"
                             "</div>");
    httpd_resp_sendstr_chunk(req,
                             "<div id=\"paramUart\" style=\"display:none;\">"
                             "<label>UART baudrate"
                             "<input type=\"text\" id=\"uartBaud\" placeholder=\"115200\"></label>"
                             "</div>");
    httpd_resp_sendstr_chunk(req,
                             "<div id=\"paramSwd\" style=\"display:none;\">"
                             "<label>SWD clock (kHz)"
                             "<input type=\"text\" id=\"swdClock\" placeholder=\"1000\"></label>"
                             "</div>");
    httpd_resp_sendstr_chunk(req,
                             "<div id=\"paramJtag\" style=\"display:none;\">"
                             "<label>JTAG clock (kHz)"
                             "<input type=\"text\" id=\"jtagClock\" placeholder=\"1000\"></label>"
                             "</div>");
    httpd_resp_sendstr_chunk(req,
                             "<label>Meta JSON (optional)"
                             "<textarea id=\"metaJson\" rows=\"4\" placeholder='{\"can_id\":\"0x201\",\"boot_cmd\":\"ENTER_BOOT\"}'></textarea></label>");
    httpd_resp_sendstr_chunk(req,
                             "<label>File name (36 chars, optional)"
                             "<input type=\"text\" id=\"fileName\" placeholder=\"auto-generate\"></label>");
    httpd_resp_sendstr_chunk(req,
                             "<button id=\"uploadBtn\">Upload</button>"
                             "<p>Rule: file_name(36) + '.' + file_type(3) = total 40 chars</p>"
                             "<h3>Log</h3><div id=\"log\">-</div>");
    httpd_resp_sendstr_chunk(req,
                             "<script>"
                             "function log(msg){document.getElementById('log').textContent=msg;}"
                             "function updateParams(){"
                             "  const exec=document.getElementById('upgradeExec').value;"
                             "  document.getElementById('paramCan').style.display=(exec==='CAN')?'block':'none';"
                             "  document.getElementById('paramUart').style.display=(exec==='UART_ISP')?'block':'none';"
                             "  document.getElementById('paramSwd').style.display=(exec==='SWD')?'block':'none';"
                             "  document.getElementById('paramJtag').style.display=(exec==='JTAG')?'block':'none';"
                             "}"
                             "function genName(){"
                             "  const hex='0123456789ABCDEF';"
                             "  let out='';"
                             "  for(let i=0;i<36;i++){out+=hex[Math.floor(Math.random()*16)];}"
                             "  return out;"
                             "}"
                             "document.getElementById('file').addEventListener('change', ()=>{"
                             "  const f=document.getElementById('file').files[0];"
                             "  if(!f){return;}"
                             "  const name=f.name.toLowerCase();"
                             "  const fmtSel=document.getElementById('fileFormat');"
                             "  if(name.endsWith('.hex')) fmtSel.value='IHEX';"
                             "  else if(name.endsWith('.coff')) fmtSel.value='COFF';"
                             "  else if(name.endsWith('.elf')) fmtSel.value='ELF';"
                             "  else fmtSel.value='BIN';"
                             "});"
                             "document.getElementById('upgradeExec').addEventListener('change', updateParams);"
                             "updateParams();"
                             "document.getElementById('uploadBtn').onclick=async ()=>{"
                             "  const f=document.getElementById('file').files[0];"
                             "  if(!f){log('Select a file first.');return;}"
                             "  let name=document.getElementById('fileName').value.trim();"
                             "  if(!name){name=genName();document.getElementById('fileName').value=name;}"
                             "  if(name.length!==36){log('file_name must be 36 chars.');return;}"
                             "  const type=document.getElementById('fileType').value;"
                             "  const format=document.getElementById('fileFormat').value;"
                             "  const exec=document.getElementById('upgradeExec').value;"
                             "  const canBitrate=document.getElementById('canBitrate').value.trim();"
                             "  const uartBaud=document.getElementById('uartBaud').value.trim();"
                             "  const swdClock=document.getElementById('swdClock').value.trim();"
                             "  const jtagClock=document.getElementById('jtagClock').value.trim();"
                             "  const metaJson=document.getElementById('metaJson').value.trim();"
                             "  const body=f;"
                             "  log('Uploading...');"
                             "  try{"
                             "    const meta={file_name:name,file_type:type,file_format:format};"
                             "    if(exec!=='AUTO') meta.exec=exec;"
                             "    if(canBitrate) meta.can_bitrate=Number(canBitrate);"
                             "    if(uartBaud) meta.uart_baud=Number(uartBaud);"
                             "    if(swdClock) meta.swd_clock_khz=Number(swdClock);"
                             "    if(jtagClock) meta.jtag_clock_khz=Number(jtagClock);"
                             "    if(metaJson) meta.meta_json=metaJson;"
                             "    if(exec!=='AUTO' || canBitrate || uartBaud || swdClock || jtagClock || metaJson){"
                             "      const mres=await fetch('/fw_meta',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(meta)});"
                             "      if(!mres.ok){log('Meta upload failed.');return;}"
                             "    }"
                             "    const headers={"
                             "      'Content-Type':'application/octet-stream',"
                             "      'X-File-Name':name,"
                             "      'X-File-Type':type,"
                             "      'X-File-Format':format"
                             "    };"
                             "    const res=await fetch('/fw_upload_raw',{method:'POST',headers:headers,body});"
                             "    const text=await res.text();"
                             "    log('?�답: '+text);"
                             "  }catch(e){log('Upload failed: '+e);}"
                             "};"
                             "</script>");
    httpd_resp_sendstr_chunk(req, "</body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t fw_run_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req,
                             "<!DOCTYPE html><html><head><title>MIRI-TOOL Run</title>"
                             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                             "<style>"
                             "body{font-family:Arial,Helvetica,sans-serif;margin:16px;}"
                             "label{display:block;margin-top:12px;}"
                             "select,button{font-size:16px;padding:6px 8px;margin-top:6px;}"
                             "#log{white-space:pre-wrap;background:#f4f4f4;padding:8px;border-radius:6px;}"
                             "</style></head><body>");
    httpd_resp_sendstr_chunk(req, "<h1>SDMMC Firmware Upload + Run</h1>");
    httpd_resp_sendstr_chunk(req,
                             "<h2>Upload</h2>"
                             "<label>Firmware file (.bin/.hex/.coff/.elf)"
                             "<input type=\"file\" id=\"file\" accept=\".bin,.hex,.coff,.elf\"></label>"
                             "<label>File type"
                             "<select id=\"fileType\">"
                             "<option>S00</option><option>S01</option>"
                             "<option>A00</option><option>A01</option><option>A02</option><option>A03</option>"
                             "<option>A04</option><option>A05</option><option>A06</option><option>A07</option>"
                             "<option>A08</option><option>A09</option><option>A10</option><option>A11</option>"
                             "<option>A12</option><option>A13</option><option>A14</option><option>A15</option>"
                             "</select></label>"
                             "<label>File format"
                             "<select id=\"fileFormat\">"
                             "<option>BIN</option><option>IHEX</option><option>COFF</option><option>ELF</option>"
                             "</select></label>"
                             "<label>Upgrade interface"
                             "<select id=\"upgradeExec\">"
                             "<option value=\"AUTO\">AUTO</option>"
                             "<option value=\"ESP32_OTA\">ESP32_OTA</option>"
                             "<option value=\"CAN\">CAN</option>"
                             "<option value=\"UART_ISP\">UART_ISP</option>"
                             "<option value=\"SWD\">SWD</option>"
                             "<option value=\"JTAG\">JTAG</option>"
                             "</select></label>"
                             "<div id=\"paramCan\" style=\"display:none;\">"
                             "<label>CAN bitrate"
                             "<input type=\"text\" id=\"canBitrate\" placeholder=\"1000000\"></label>"
                             "</div>"
                             "<div id=\"paramUart\" style=\"display:none;\">"
                             "<label>UART baudrate"
                             "<input type=\"text\" id=\"uartBaud\" placeholder=\"115200\"></label>"
                             "</div>"
                             "<div id=\"paramSwd\" style=\"display:none;\">"
                             "<label>SWD clock (kHz)"
                             "<input type=\"text\" id=\"swdClock\" placeholder=\"1000\"></label>"
                             "</div>"
                             "<div id=\"paramJtag\" style=\"display:none;\">"
                             "<label>JTAG clock (kHz)"
                             "<input type=\"text\" id=\"jtagClock\" placeholder=\"1000\"></label>"
                             "</div>"
                             "<label>Meta JSON (optional)"
                             "<textarea id=\"metaJson\" rows=\"4\" placeholder='{\"can_id\":\"0x201\",\"boot_cmd\":\"ENTER_BOOT\"}'></textarea></label>"
                             "<label>File name (36 chars, optional)"
                             "<input type=\"text\" id=\"fileName\" placeholder=\"auto-generate\"></label>"
                             "<button id=\"uploadBtn\">Upload</button>"
                             "<p>Rule: file_name(36) + '.' + file_type(3) = total 40 chars</p>"
                             "<h3>Upload Log</h3><div id=\"uploadLog\">-</div>"
                             "<hr/>"
                             "<h2>Run</h2>"
                             "<label>Stored firmware"
                             "<select id=\"fileList\"></select></label>"
                             "<button id=\"refreshBtn\">Refresh</button>"
                             "<button id=\"runBtn\">Run</button>"
                             "<h3>Run Log</h3><div id=\"runLog\">-</div>"
                             "<h3>Status</h3><div id=\"status\">-</div>");
    httpd_resp_sendstr_chunk(req,
                             "<script>"
                             "function logUpload(msg){document.getElementById('uploadLog').textContent=msg;}"
                             "function logRun(msg){document.getElementById('runLog').textContent=msg;}"
                             "function setStatus(msg){document.getElementById('status').textContent=msg;}"
                             "function updateParams(){"
                             "  const exec=document.getElementById('upgradeExec').value;"
                             "  document.getElementById('paramCan').style.display=(exec==='CAN')?'block':'none';"
                             "  document.getElementById('paramUart').style.display=(exec==='UART_ISP')?'block':'none';"
                             "  document.getElementById('paramSwd').style.display=(exec==='SWD')?'block':'none';"
                             "  document.getElementById('paramJtag').style.display=(exec==='JTAG')?'block':'none';"
                             "}"
                             "function genName(){"
                             "  const hex='0123456789ABCDEF';"
                             "  let out='';"
                             "  for(let i=0;i<36;i++){out+=hex[Math.floor(Math.random()*16)];}"
                             "  return out;"
                             "}"
                             "document.getElementById('file').addEventListener('change', ()=>{"
                             "  const f=document.getElementById('file').files[0];"
                             "  if(!f){return;}"
                             "  const name=f.name.toLowerCase();"
                             "  const fmtSel=document.getElementById('fileFormat');"
                             "  if(name.endsWith('.hex')) fmtSel.value='IHEX';"
                             "  else if(name.endsWith('.coff')) fmtSel.value='COFF';"
                             "  else if(name.endsWith('.elf')) fmtSel.value='ELF';"
                             "  else fmtSel.value='BIN';"
                             "});"
                             "document.getElementById('upgradeExec').addEventListener('change', updateParams);"
                             "updateParams();"
                             "document.getElementById('uploadBtn').onclick=async ()=>{"
                             "  const f=document.getElementById('file').files[0];"
                             "  if(!f){logUpload('Select a file');return;}"
                             "  let name=document.getElementById('fileName').value.trim();"
                             "  if(!name){name=genName();document.getElementById('fileName').value=name;}"
                             "  if(name.length!==36){logUpload('file_name must be 36 chars');return;}"
                             "  const type=document.getElementById('fileType').value;"
                             "  const format=document.getElementById('fileFormat').value;"
                             "  const exec=document.getElementById('upgradeExec').value;"
                             "  const canBitrate=document.getElementById('canBitrate').value.trim();"
                             "  const uartBaud=document.getElementById('uartBaud').value.trim();"
                             "  const swdClock=document.getElementById('swdClock').value.trim();"
                             "  const jtagClock=document.getElementById('jtagClock').value.trim();"
                             "  const metaJson=document.getElementById('metaJson').value.trim();"
                             "  logUpload('Uploading...');"
                             "  try{"
                             "    const meta={file_name:name,file_type:type,file_format:format};"
                             "    if(exec!=='AUTO') meta.exec=exec;"
                             "    if(canBitrate) meta.can_bitrate=Number(canBitrate);"
                             "    if(uartBaud) meta.uart_baud=Number(uartBaud);"
                             "    if(swdClock) meta.swd_clock_khz=Number(swdClock);"
                             "    if(jtagClock) meta.jtag_clock_khz=Number(jtagClock);"
                             "    if(metaJson) meta.meta_json=metaJson;"
                             "    if(exec!=='AUTO' || canBitrate || uartBaud || swdClock || jtagClock || metaJson){"
                             "      const mres=await fetch('/fw_meta',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(meta)});"
                             "      if(!mres.ok){logUpload('Meta save failed');return;}"
                             "    }"
                             "    const res=await fetch('/fw_upload_raw',{method:'POST',headers:{"
                             "      'Content-Type':'application/octet-stream',"
                             "      'X-File-Name':name,"
                             "      'X-File-Type':type,"
                             "      'X-File-Format':format"
                             "    },body:f});"
                             "    const text=await res.text();"
                             "    logUpload('Response: '+text);"
                             "    refresh();"
                             "  }catch(e){logUpload('Upload failed: '+e);}"
                             "};"
                             "async function refresh(){"
                             "  const sel=document.getElementById('fileList');"
                             "  sel.innerHTML='';"
                             "  try{"
                             "    const res=await fetch('/sdmmc_fw_get_list');"
                             "    const data=await res.json();"
                             "    if(!data.fileList || data.fileList.length===0){"
                             "      logRun('No files found');"
                             "      return;"
                             "    }"
                             "    data.fileList.forEach((f)=>{"
                             "      const opt=document.createElement('option');"
                             "      opt.value=f.file_name+'|'+f.file_type;"
                             "      opt.text=f.file_name+'.'+f.file_type+' ('+f.file_date+')';"
                             "      sel.appendChild(opt);"
                             "    });"
                             "    logRun('List refreshed');"
                             "  }catch(e){logRun('List fetch failed: '+e);}"
                             "}"
                             "document.getElementById('refreshBtn').onclick=refresh;"
                             "document.getElementById('runBtn').onclick=async ()=>{"
                             "  const sel=document.getElementById('fileList');"
                             "  if(!sel.value){logRun('Select a file');return;}"
                             "  const parts=sel.value.split('|');"
                             "  const payload={file_name:parts[0],file_type:parts[1]};"
                             "  logRun('Starting...');"
                             "  try{"
                             "    const res=await fetch('/fw_run',{method:'POST',headers:{'Content-Type':'application/json'},"
                             "      body:JSON.stringify(payload)});"
                             "    const text=await res.text();"
                             "    logRun('Response: '+text);"
                             "  }catch(e){logRun('Run failed: '+e);}"
                             "};"
                             "async function pollStatus(){"
                             "  try{"
                             "    const res=await fetch('/fw_state');"
                             "    const data=await res.json();"
                             "    const msg=(data.message && data.message.length)?data.message:'';"
                             "    setStatus(data.step_name+' ('+data.progress+'%) '+msg);"
                             "  }catch(e){setStatus('Status error: '+e);}"
                             "}"
                             "setInterval(pollStatus, 1000);"
                             "pollStatus();"
                             "refresh();"
                             "</script>");
    httpd_resp_sendstr_chunk(req, "</body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

esp_err_t http_server_start(void) {
    static bool started = false;
    if (started) {
        ESP_LOGW(TAG, "HTTP server already started");
        return ESP_ERR_INVALID_STATE;
    }

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 16384;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 24;

    ESP_LOGI(TAG, "Starting HTTP Server on port: %d", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    /* /fw_upload_run, /fw_run: 레거시 폰 호환 시 메타는 1차 JSON → 2차 BLE → 3차 type 기본값 으로 구성 (각 핸들러 주석 참고) */
    httpd_uri_t sdmmc_fw_upload_run_handler = {
        .uri      = "/fw_upload_run",
        .method   = HTTP_POST,
        .handler  = sdmmc_fw_upload_run_logged,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &sdmmc_fw_upload_run_handler);

    httpd_uri_t sdmmc_fw_upload_raw_handler = {
        .uri      = "/fw_upload_raw",
        .method   = HTTP_POST,
        .handler  = sdmmc_fw_upload_raw_logged,
        .user_ctx = NULL,
    };
    httpd_uri_t sdmmc_fw_meta_handler = {
        .uri      = "/fw_meta",
        .method   = HTTP_POST,
        .handler  = sdmmc_fw_meta_logged,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &sdmmc_fw_upload_raw_handler);
    httpd_register_uri_handler(server, &sdmmc_fw_meta_handler);

    httpd_uri_t sdmmc_fw_run_handler = {
        .uri      = "/fw_run",
        .method   = HTTP_POST,
        .handler  = sdmmc_fw_run_logged,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &sdmmc_fw_run_handler);

    httpd_uri_t fw_state_handler = {
        .uri      = "/fw_state",
        .method   = HTTP_GET,
        .handler  = fw_state_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &fw_state_handler);

    httpd_uri_t fw_run_ui_handler = {
        .uri      = "/fw_run_ui",
        .method   = HTTP_GET,
        .handler  = fw_run_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &fw_run_ui_handler);

    httpd_uri_t sdmmc_fw_get_list_handler = {
        .uri      = "/sdmmc_fw_get_list",
        .method   = HTTP_GET,
        .handler  = sdmmc_fw_get_list_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &sdmmc_fw_get_list_handler);

    httpd_uri_t fat_get_list_handler = {
        .uri      = "/fat_get_list",
        .method   = HTTP_GET,
        .handler  = fat_get_list_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &fat_get_list_handler);

    httpd_uri_t sdmmc_format_handler = {
        .uri      = "/sdmmc_format",
        .method   = HTTP_GET,
        .handler  = format_sdmmc_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &sdmmc_format_handler);

    httpd_uri_t fat_format_handler = {
        .uri      = "/fat_format",
        .method   = HTTP_GET,
        .handler  = format_fat_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &fat_format_handler);

    httpd_uri_t sdmmc_file_delete_handler = {
        .uri      = "/sdmmc_file_delete",
        .method   = HTTP_POST,
        .handler  = delete_sdmmc_post_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &sdmmc_file_delete_handler);

    httpd_uri_t fat_file_delete_handler = {
        .uri      = "/fat_file_delete",
        .method   = HTTP_POST,
        .handler  = delete_fat_post_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &fat_file_delete_handler);

    httpd_uri_t get_fdc_handler = {
        .uri      = "/get_fdc",
        .method   = HTTP_GET,
        .handler  = get_fdc_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &get_fdc_handler);

    httpd_uri_t fs_list_handler = {
        .uri      = "/fs_list",
        .method   = HTTP_GET,
        .handler  = fs_list_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &fs_list_handler);

    httpd_uri_t fs_upload_handler = {
        .uri      = "/fs_upload",
        .method   = HTTP_POST,
        .handler  = fs_upload_post_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &fs_upload_handler);

    httpd_uri_t fs_delete_handler = {
        .uri      = "/fs_delete",
        .method   = HTTP_POST,
        .handler  = fs_delete_post_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &fs_delete_handler);

    httpd_uri_t fs_download_handler = {
        .uri      = "/fs_download",
        .method   = HTTP_GET,
        .handler  = fs_download_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &fs_download_handler);

    httpd_uri_t hellow = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = hellow_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &hellow);

    httpd_uri_t upload = {
        .uri      = "/upload",
        .method   = HTTP_GET,
        .handler  = upload_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &upload);

    httpd_uri_t sdmmc_ui = {
        .uri      = "/sdmmc",
        .method   = HTTP_GET,
        .handler  = fs_ui_get_handler,
        .user_ctx = (void *)"sdmmc",
    };
    httpd_register_uri_handler(server, &sdmmc_ui);

    httpd_uri_t flash_ui = {
        .uri      = "/flash",
        .method   = HTTP_GET,
        .handler  = fs_ui_get_handler,
        .user_ctx = (void *)"flash",
    };
    httpd_register_uri_handler(server, &flash_ui);

    started = true;
    return ESP_OK;
}




