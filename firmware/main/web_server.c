/*
 * web_server.c — Web 服务器（配网 + 管理 + REST API）
 */
#include "web_server.h"
#include "wol.h"
#include "wifi_manager.h"
#include "config_store.h"
#include <esp_http_server.h>
#include <esp_log.h>
#include <cJSON.h>
#include <string.h>

static const char *TAG = "Web";
static httpd_handle_t s_httpd = NULL;

// ===== 内嵌 HTML =====

static const char WIFI_SETUP_HTML[] = R"rawliteral(
<!DOCTYPE html><html lang="zh-CN"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>小希-WOL 配网</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#0f0c29;color:#e0e0e0;
  padding:1rem;max-width:480px;margin:auto;display:flex;flex-direction:column;
  min-height:100vh;justify-content:center}
.card{background:#1a1a3e;border-radius:12px;padding:1.5rem;margin-bottom:1rem;
  box-shadow:0 4px 20px rgba(0,0,0,0.3)}
h1{text-align:center;font-size:1.4rem;padding:.5rem 0;color:#667eea}
label{display:block;font-size:.85rem;color:#aaa;margin-top:.7rem}
input[type=text],input[type=password]{width:100%;padding:.6rem;border:1px solid #333;
  border-radius:8px;background:#16213e;color:#e0e0e0;font-size:1rem;margin-top:.2rem}
button{width:100%;padding:.8rem;margin-top:1rem;border:none;border-radius:10px;
  background:linear-gradient(135deg,#667eea,#764ba2);color:#fff;
  font-size:1.05rem;font-weight:700;cursor:pointer}
#msg{text-align:center;margin-top:.8rem;font-size:.9rem;color:#64ffda;display:none}
</style></head><body>
<div class="card">
  <h1>⚡ 小希-WOL 配网</h1>
  <label>WiFi名称</label><input id="ssid" type="text" placeholder="WiFi名称">
  <label>WiFi密码</label><input id="pwd" type="password" placeholder="WiFi密码">
  <button onclick="save()">保存并连接</button>
  <div id="msg"></div>
</div>
<script>
function save(){
  let s=document.getElementById('ssid').value.trim(),p=document.getElementById('pwd').value;
  if(!s){alert('请输入WiFi名称');return;}
  let m=document.getElementById('msg');m.style.display='block';m.textContent='正在连接...';
  fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ssid:s,password:p})
  }).then(r=>r.json()).then(j=>{m.textContent='✅ 已保存，设备将重启...';})
  .catch(e=>{m.textContent='❌ 失败: '+e;});
}
</script></body></html>)rawliteral";

static const char MAIN_HTML[] = R"rawliteral(
<!DOCTYPE html><html lang="zh-CN"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>小希-WOL</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#0f0c29;color:#e0e0e0;
  padding:1rem;max-width:480px;margin:auto}
h1{text-align:center;font-size:1.4rem;padding:1rem 0;color:#667eea}
.section{background:#1a1a3e;border-radius:12px;padding:1rem;margin-bottom:1rem}
.section h2{font-size:1rem;color:#667eea;margin-bottom:.6rem;border-bottom:1px solid #333;padding-bottom:.3rem}
.dev{display:flex;align-items:center;justify-content:space-between;
  background:#16213e;border-radius:8px;padding:.8rem;margin-bottom:.5rem}
.dev-info{flex:1}.dev-name{font-weight:700;font-size:1rem}.dev-mac{font-size:.75rem;color:#888}
.dev-btn{padding:.5rem 1.2rem;border:none;border-radius:8px;
  background:linear-gradient(135deg,#667eea,#764ba2);color:#fff;
  font-weight:700;cursor:pointer;font-size:.9rem}
.dev-btn:active{opacity:.7}
.dev-btn:disabled{background:#333;cursor:not-allowed}
label{display:block;font-size:.85rem;color:#aaa;margin-top:.5rem}
input[type=text]{width:100%;padding:.5rem;border:1px solid #333;
  border-radius:6px;background:#16213e;color:#e0e0e0;font-size:.9rem;margin-top:.2rem}
button.add{width:100%;padding:.6rem;margin-top:.8rem;border:none;border-radius:8px;
  background:#333;color:#fff;font-size:.9rem;cursor:pointer}
button.add:active{background:#444}
#toast{position:fixed;top:1rem;left:50%;transform:translateX(-50%);
  background:#4caf50;color:#fff;padding:.5rem 1.2rem;border-radius:8px;
  display:none;font-size:.9rem;z-index:999}
.status{text-align:center;font-size:.8rem;color:#888;margin-top:.5rem}
</style></head><body>
<h1>⚡ 小希-WOL</h1>
<div class="section">
  <h2>📡 设备状态</h2>
  <div id="status" class="status">加载中...</div>
</div>
<div class="section">
  <h2>💻 已保存设备</h2>
  <div id="dev-list"></div>
  <hr style="border-color:#333;margin:.8rem 0">
  <label>设备名称</label><input id="dname" type="text" placeholder="如：工坊电脑">
  <label>MAC地址</label><input id="dmac" type="text" placeholder="AA:BB:CC:DD:EE:FF">
  <button class="add" onclick="addDev()">➕ 添加设备</button>
</div>
<div class="section">
  <h2>📶 WiFi</h2>
  <div id="wifi-info" class="status">加载中...</div>
</div>
<div id="toast"></div>
<script>
function showToast(t){let e=document.getElementById('toast');e.textContent=t;
  e.style.display='block';setTimeout(()=>e.style.display='none',2000);}
function loadStatus(){fetch('/api/status').then(r=>r.json()).then(s=>{
  document.getElementById('status').innerHTML=
    'WiFi: <b>'+(s.wifi||'unknown')+'</b> | IP: <b>'+(s.ip||'0.0.0.0')+'</b>';
  document.getElementById('wifi-info').innerHTML=
    'SSID: <b>'+(s.ssid||'未配置')+'</b>'}).catch(()=>{});}
function loadDevs(){fetch('/api/devices').then(r=>r.json()).then(devs=>{
  let h='';(devs||[]).forEach((d,i)=>{
    h+='<div class="dev"><div class="dev-info"><div class="dev-name">'+
      d.name+'</div><div class="dev-mac">'+d.mac+'</div></div>'+
      '<button class="dev-btn" onclick="wol('+i+')">⚡ 开机</button></div>';
  });
  document.getElementById('dev-list').innerHTML=h||'<div class="status">暂无设备</div>';
}).catch(()=>{});}
function wol(i){fetch('/api/wol/'+i,{method:'POST'}).then(r=>r.json()).then(j=>{
  showToast(j.ok?'✅ 已发送开机指令':'❌ '+j.error);}).catch(e=>showToast('❌ '+e));}
function addDev(){
  let n=document.getElementById('dname').value.trim(),
      m=document.getElementById('dmac').value.trim();
  if(!n||!m){alert('请填写设备名称和MAC地址');return;}
  fetch('/api/devices',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({name:n,mac:m})
  }).then(r=>r.json()).then(j=>{if(j.ok){showToast('✅ 已添加');loadDevs();
    document.getElementById('dname').value='';document.getElementById('dmac').value='';}
    else showToast('❌ '+(j.error||'失败'));}).catch(e=>showToast('❌ '+e));}
loadStatus();loadDevs();setInterval(loadStatus,5000);
</script></body></html>)rawliteral";

// ===== DNS for captive portal =====
static volatile bool s_dns_running = false;
static TaskHandle_t s_dns_task = NULL;

static void dns_task(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { s_dns_running = false; vTaskDelete(NULL); return; }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock); s_dns_running = false; vTaskDelete(NULL); return;
    }

    ESP_LOGI(TAG, "DNS server started");
    uint8_t buf[512];
    while (s_dns_running) {
        struct sockaddr_in client = {};
        socklen_t len = sizeof(client);
        int n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&client, &len);
        if (n < 12) continue;

        uint8_t resp[512];
        memcpy(resp, buf, 2);
        resp[2] = 0x81; resp[3] = 0x80;
        resp[4] = 0; resp[5] = 1; resp[6] = 0; resp[7] = 1;
        memset(resp + 8, 0, 4);

        int qname_end = 12;
        while (qname_end < n && buf[qname_end]) qname_end += buf[qname_end] + 1;
        qname_end++;
        int q_len = qname_end - 12 + 4;
        memcpy(resp + 12, buf + 12, q_len);
        int rlen = 12 + q_len;

        resp[rlen++] = 0xC0; resp[rlen++] = 0x0C;
        resp[rlen++] = 0; resp[rlen++] = 1;
        resp[rlen++] = 0; resp[rlen++] = 1;
        resp[rlen++] = 0; resp[rlen++] = 0;
        resp[rlen++] = 0; resp[rlen++] = 0x3C;
        resp[rlen++] = 0; resp[rlen++] = 4;
        resp[rlen++] = 192; resp[rlen++] = 168;
        resp[rlen++] = 4;   resp[rlen++] = 1;

        sendto(sock, resp, rlen, 0, (struct sockaddr *)&client, len);
    }
    close(sock);
    vTaskDelete(NULL);
}

// ===== HTTP handlers =====

static esp_err_t handle_root(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    if (wifi_manager_get_status() == WIFI_STATUS_AP_MODE) {
        return httpd_resp_send(req, WIFI_SETUP_HTML, sizeof(WIFI_SETUP_HTML) - 1);
    }
    return httpd_resp_send(req, MAIN_HTML, sizeof(MAIN_HTML) - 1);
}

static esp_err_t handle_captive_redirect(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handle_get_status(httpd_req_t *req) {
    wol_config_t *cfg = config_get();
    cJSON *root = cJSON_CreateObject();
    const char *ws = "disconnected";
    switch (wifi_manager_get_status()) {
        case WIFI_STATUS_CONNECTED: ws = "connected"; break;
        case WIFI_STATUS_CONNECTING: ws = "connecting"; break;
        case WIFI_STATUS_AP_MODE: ws = "AP模式"; break;
        default: break;
    }
    cJSON_AddStringToObject(root, "wifi", ws);
    cJSON_AddStringToObject(root, "ip", wifi_manager_get_ip());
    cJSON_AddStringToObject(root, "ssid", cfg->wifi_ssid);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_sendstr(req, out);
    free(out);
    return ret;
}

static esp_err_t handle_get_devices(httpd_req_t *req) {
    wol_config_t *cfg = config_get();
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < cfg->device_count; i++) {
        wol_device_t *d = &cfg->devices[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "name", d->name);
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 d->mac[0], d->mac[1], d->mac[2], d->mac[3], d->mac[4], d->mac[5]);
        cJSON_AddStringToObject(obj, "mac", mac_str);
        cJSON_AddItemToArray(arr, obj);
    }
    char *out = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_sendstr(req, out);
    free(out);
    return ret;
}

static esp_err_t handle_post_device(httpd_req_t *req) {
    int total = req->content_len;
    if (total <= 0 || total > 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        return ESP_FAIL;
    }
    char *body = calloc(1, total + 1);
    httpd_req_recv(req, body, total);

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_FAIL;
    }

    wol_config_t *cfg = config_get();
    if (cfg->device_count >= WOL_MAX_DEVICES) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"设备已满\"}");
    }

    cJSON *jname = cJSON_GetObjectItem(root, "name");
    cJSON *jmac = cJSON_GetObjectItem(root, "mac");
    if (!jname || !jmac || !cJSON_IsString(jname) || !cJSON_IsString(jmac)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing name/mac");
        return ESP_FAIL;
    }

    wol_device_t *dev = &cfg->devices[cfg->device_count];
    strncpy(dev->name, jname->valuestring, sizeof(dev->name) - 1);

    // Parse MAC: "AA:BB:CC:DD:EE:FF"
    unsigned m[6];
    if (sscanf(jmac->valuestring, "%x:%x:%x:%x:%x:%x",
               &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
        for (int i = 0; i < 6; i++) dev->mac[i] = (uint8_t)m[i];
        dev->mac_valid = true;
    } else {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"MAC格式错误\"}");
    }

    dev->port = WOL_DEFAULT_PORT;
    cfg->device_count++;
    config_save();

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t handle_wol_device(httpd_req_t *req) {
    // URI: /api/wol/<index>
    const char *uri = req->uri;
    int idx = atoi(uri + strlen("/api/wol/"));

    wol_config_t *cfg = config_get();
    if (idx < 0 || idx >= cfg->device_count) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"设备不存在\"}");
    }

    led_set_state(LED_BLINK_FAST);
    esp_err_t err = wol_send_to_device(&cfg->devices[idx]);
    led_set_state(wifi_manager_get_status() == WIFI_STATUS_CONNECTED ? LED_ON : LED_BLINK_SLOW);

    httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK) {
        return httpd_resp_sendstr(req, "{\"ok\":true}");
    }
    return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"发送失败\"}");
}

static esp_err_t handle_post_wifi(httpd_req_t *req) {
    int total = req->content_len;
    if (total <= 0 || total > 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        return ESP_FAIL;
    }
    char *body = calloc(1, total + 1);
    httpd_req_recv(req, body, total);

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_FAIL;
    }

    wol_config_t *cfg = config_get();
    cJSON *j;
    if ((j = cJSON_GetObjectItem(root, "ssid")) && cJSON_IsString(j))
        strncpy(cfg->wifi_ssid, j->valuestring, sizeof(cfg->wifi_ssid) - 1);
    if ((j = cJSON_GetObjectItem(root, "password")) && cJSON_IsString(j))
        strncpy(cfg->wifi_password, j->valuestring, sizeof(cfg->wifi_password) - 1);

    cJSON_Delete(root);
    config_save();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

// ===== Public =====

esp_err_t web_server_init(void) {
    ESP_LOGI(TAG, "Web server init");
    return ESP_OK;
}

esp_err_t web_server_start(void) {
    if (s_httpd) return ESP_OK;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.stack_size = 8192;
    cfg.max_uri_handlers = 16;

    if (httpd_start(&s_httpd, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return ESP_FAIL;
    }

    static const httpd_uri_t uris[] = {
        { .uri = "/",              .method = HTTP_GET,  .handler = handle_root },
        { .uri = "/api/status",    .method = HTTP_GET,  .handler = handle_get_status },
        { .uri = "/api/devices",   .method = HTTP_GET,  .handler = handle_get_devices },
        { .uri = "/api/devices",   .method = HTTP_POST, .handler = handle_post_device },
        { .uri = "/api/wifi",      .method = HTTP_POST, .handler = handle_post_wifi },
        { .uri = "/generate_204",        .method = HTTP_GET, .handler = handle_captive_redirect },
        { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = handle_captive_redirect },
        { .uri = "/connecttest.txt",     .method = HTTP_GET, .handler = handle_captive_redirect },
        { .uri = "/ncsi.txt",            .method = HTTP_GET, .handler = handle_captive_redirect },
    };
    for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_httpd, &uris[i]);
    }
    // WOL 动态路由（/api/wol/0, /api/wol/1, ...）
    httpd_uri_t wol_uri = { .uri = "/api/wol/*", .method = HTTP_POST, .handler = handle_wol_device };
    httpd_register_uri_handler(s_httpd, &wol_uri);

    ESP_LOGI(TAG, "Web server started on :80");
    return ESP_OK;
}

void web_server_start_dns(void) {
    if (!s_dns_running) {
        s_dns_running = true;
        xTaskCreate(dns_task, "dns_srv", 4096, NULL, 5, &s_dns_task);
    }
}
