// ESP32 core 1.0.4 defines conflicting HTTP method enums in WebServer and
// esp_http_server. Compile streaming in its own translation unit.
#include <Arduino.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "freertos/semphr.h"
#include "StreamServer.h"
#include "Preview.h"
extern SemaphoreHandle_t frameMutex;
extern camera_fb_t *frame;
extern bool frameValid;

static esp_err_t streamHandler(httpd_req_t *req){
 httpd_resp_set_type(req,"multipart/x-mixed-replace;boundary=frame");
 for(;;){
  uint8_t *copy=nullptr;size_t n=0;
  uint16_t seq=0;previewCopy(copy,n,seq);
  if(copy){
   char header[90];snprintf(header,sizeof(header),"\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",(unsigned)n);
   esp_err_t e=httpd_resp_send_chunk(req,header,strlen(header));
   if(e==ESP_OK)e=httpd_resp_send_chunk(req,(const char*)copy,n);free(copy);
   if(e!=ESP_OK)return e;
  }
  vTaskDelay(pdMS_TO_TICKS(120));
 }
 return ESP_OK;
}
void startStream(){
 httpd_config_t c=HTTPD_DEFAULT_CONFIG();c.server_port=81;c.ctrl_port=32769;c.stack_size=4096;
 httpd_handle_t server=nullptr;if(httpd_start(&server,&c)==ESP_OK){
  httpd_uri_t u={};u.uri="/stream";u.method=HTTP_GET;u.handler=streamHandler;httpd_register_uri_handler(server,&u);
 }
}
