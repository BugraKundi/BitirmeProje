#include "esp_camera.h"
#include "camera_pins.h"        // Must include this after setting
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems
#include "esp_http_server.h"
#include <ESP32Servo.h>

#include "helpers.h"

unsigned long last_still=0;
bool last_region_g4 = true;
bool toggle_track = true;
unsigned long squirt_t;
bool tracking_on = false;

// Ağ bilgilerinizi buraya girin
const char* ssid = "TurkTelekom_ZTT3AK";
const char* password = "Kfa9ahaEk4x3";

#define PART_BOUNDARY "123456789000000000000987654321"

// Sadece Sağ-Sol (Pan) servosu için pin tanımı
#define SERVO_PAN    15  
#define DRIVER_PIN   12 // İleride buzzer/lazer için kullanılacak

#define SERVO_STEP   5

Servo panServo;
int panPos = 90; // Kameranın başlangıç açısı (Ortada)

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

// Web Arayüzü (Yukarı/Aşağı butonları kaldırıldı)
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <title>ESP32-CAM Kontrol Paneli</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
      :root {
        --bg-color: #0f172a;
        --card-bg: #1e293b;
        --text-color: #f8fafc;
        --primary: #3b82f6;
        --primary-hover: #2563eb;
        --success: #10b981;
        --success-hover: #059669;
        --danger: #ef4444;
        --danger-hover: #dc2626;
        --accent: #f59e0b;
      }
      
      body { 
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
        background-color: var(--bg-color); 
        color: var(--text-color);
        margin: 0; 
        padding: 20px;
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        min-height: 100vh;
      }

      .container {
        width: 100%;
        max-width: 500px;
        background: var(--card-bg);
        padding: 25px;
        border-radius: 16px;
        box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.3), 0 8px 10px -6px rgba(0, 0, 0, 0.3);
        text-align: center;
      }

      h1 { 
        font-size: 24px; 
        margin-top: 0;
        margin-bottom: 20px;
        font-weight: 600;
        letter-spacing: -0.5px;
        color: #e2e8f0;
      }

      .stream-container {
        position: relative;
        width: 100%;
        border-radius: 12px;
        overflow: hidden;
        background: #000;
        aspect-ratio: 4 / 3;
        margin-bottom: 20px;
        border: 2px solid #334155;
      }

      #photo {  
        width: 100%;
        height: 100%;
        object-fit: cover;
        display: block;
      }

      .control-group {
        display: flex;
        flex-direction: column;
        gap: 15px;
        margin-top: 10px;
      }

      .row {
        display: flex;
        gap: 10px;
        justify-content: center;
        width: 100%;
      }

      .button {
        flex: 1;
        border: none;
        color: white;
        padding: 14px 20px;
        text-align: center;
        font-weight: 600;
        font-size: 15px;
        border-radius: 10px;
        cursor: pointer;
        transition: all 0.2s ease;
        box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
        -webkit-touch-callout: none;
        user-select: none;
      }

      .button:active {
        transform: scale(0.97);
        box-shadow: 0 2px 4px rgba(0,0,0,0.2);
      }

      .btn-primary { background-color: var(--primary); }
      .btn-primary:hover { background-color: var(--primary-hover); }

      .btn-success { background-color: var(--success); }
      .btn-success:hover { background-color: var(--success-hover); }

      .btn-danger { background-color: var(--danger); }
      .btn-danger:hover { background-color: var(--danger-hover); }

      .btn-accent { background-color: var(--accent); }
      .btn-accent:hover { background-color: #d97706; }

      .status-bar {
        display: flex;
        justify-content: space-between;
        align-items: center;
        background: #0f172a;
        padding: 8px 15px;
        border-radius: 8px;
        font-size: 13px;
        color: #94a3b8;
        margin-bottom: 15px;
      }

      .status-dot {
        display: inline-block;
        width: 8px;
        height: 8px;
        background-color: var(--success);
        border-radius: 50%;
        margin-right: 5px;
        box-shadow: 0 0 8px var(--success);
      }
    </style>
  </head>
  <body>

    <div class="container">
      <h1>Otonom Takip Sistemi</h1>
      
      <div class="status-bar">
        <span><span class="status-dot"></span> Canli Yayin Aktif</span>
        <span id="mode-text">Takip: Beklemede</span>
      </div>

      <div class="stream-container">
        <img src="" id="photo">
      </div>
      
      <div class="control-group">
        <div class="row">
          <button class="button btn-primary" onmousedown="toggleCheckbox('left');" ontouchstart="toggleCheckbox('left');">Sol</button>
          <button class="button btn-primary" onmousedown="toggleCheckbox('right');" ontouchstart="toggleCheckbox('right');">Sag</button>
        </div>
        
        <div class="row">
          <button class="button btn-success" onmousedown="toggleCheckbox('track'); updateMode('Aktif');">Takip Baslat</button>
          <button class="button btn-danger" onmousedown="toggleCheckbox('track_off'); updateMode('Kapali');">Takip Durdur</button>
        </div>

        <div class="row">
          <button class="button btn-accent" onmousedown="toggleCheckbox('squirt');">Test Tetikleme (Lazer/Buzzer)</button>
        </div>
      </div>
    </div>
       
    <script>
      function toggleCheckbox(x) {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/action?go=" + x, true);
        xhr.send();
      }
      
      function updateMode(status) {
        document.getElementById("mode-text").innerText = "Takip: " + status;
      }

      window.onload = function() {
        document.getElementById("photo").src = window.location.href.slice(0, -1) + ":81/stream";
      }
    </script>
  </body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

void move_left(int degs){
  Serial.print("in move left ");
  Serial.println(panPos);
  Serial.println(degs);
  for(int i = 0; i<degs; i++){
    if(panPos <= 180-SERVO_STEP) {
      panPos += SERVO_STEP;
      panServo.write(panPos);
      delay(30);
    }
  }
}

void move_right(int degs){
  Serial.print("in move right ");
  Serial.println(degs);
  for(int i = 0; i<degs; i++){
    if(panPos >= SERVO_STEP) {
      panPos -= SERVO_STEP;
      panServo.write(panPos);
      delay(30);
    }
  }
}

static esp_err_t stream_handler(httpd_req_t *req){
  fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    return res;
  }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      // Motion tracking mantığı (Değiştirilmedi, helpers.h kullanılıyor)
      if(tracking_on && finished_tracking)
      {
        last_still = millis();
      if (!capture_still())
          {
              Serial.println("Failed capture");
          } else{
            cnt++;
            if(first_capture) {
              cnt = 0;
              update_frame();
              first_capture = false;
             }
            }
        
        if (motion_detect())
              {
                cnt = 0;
                do_tracking = true; 
                finished_tracking = false; 
                Serial.println("motion detected");
              } else if(cnt>1000){
                // 1000 kare boyunca hareket yoksa ortaya dön
                panServo.write(90);
                delay(100);
                capture_still();
                update_frame();
              }
        update_frame();
      
      } else {
        toggle_track = true;
      }
      
      if(fb->width > 100){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    
    if(res != ESP_OK){
      break;
    }
  }
  return res;
}

static esp_err_t cmd_handler(httpd_req_t *req){
  char* buf;
  size_t buf_len;
  char variable[32] = {0,};
  
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char*)malloc(buf_len);
    if(!buf){
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "go", variable, sizeof(variable)) == ESP_OK) {
      } else {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
      }
    } else {
      free(buf);
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  int res = 0;
  
  if(!strcmp(variable, "left")) {
    if(panPos <= 170) {
      panPos += SERVO_STEP;
      panServo.write(panPos);
    }
    Serial.println(panPos);
    Serial.println("Left");
  }
  else if(!strcmp(variable, "right")) {
    if(panPos >= 10) {
      panPos -= SERVO_STEP;
      panServo.write(panPos);
    }
    Serial.println(panPos);
    Serial.println("Right");
  }
  else if(!strcmp(variable, "squirt")) {
    // Web arayüzünden manuel tetikleme yapmak istersen diye açık bırakıldı
    digitalWrite(DRIVER_PIN,HIGH);
    squirt_t = millis();
    Serial.println("squirt test triggered");
  }
  else if(!strcmp(variable, "track")) {
    tracking_on = true;
    Serial.println("track ON");
  }
  else if(!strcmp(variable, "track_off")) {
    tracking_on = false;
    Serial.println("track OFF");
  }
  else {
    res = -1;
  }

  if(res){
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t cmd_uri = {
    .uri       = "/action",
    .method    = HTTP_GET,
    .handler   = cmd_handler,
    .user_ctx  = NULL
  };
  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
  }
  config.server_port += 1;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  
  squirt_t = millis();
  pinMode(DRIVER_PIN, OUTPUT);
  digitalWrite(DRIVER_PIN, LOW); // Motor/Röle başlangıçta kapalı
  
  panServo.setPeriodHertz(50);    // standard 50 hz servo
  panServo.attach(SERVO_PAN, 1000, 2000);
  panServo.write(panPos);
  
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE; 
  
  if(psramFound()){
    config.frame_size = MY_FRAMESIZE; 
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = MY_FRAMESIZE; 
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // Wi-Fi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.println(WiFi.localIP());
  
  // Start streaming web server
  startCameraServer();
}

void loop() {
  // Manuel squirt testi yapıldığında bir süre sonra kapatması için:
  if((millis()-squirt_t)>400){
    digitalWrite(DRIVER_PIN, LOW);
  }
  
  if(tracking_on && do_tracking){
    do_tracking = false;
    finished_tracking = false;
    Serial.println("Motion detected");
     
    // Hareket algılandığında sağa/sola dön
    if(region_of_interest<4){
      move_left(3*(4-region_of_interest));
      last_region_g4 = false;
    } else if(region_of_interest>4){
      move_right(3*(region_of_interest-4));
      last_region_g4 = true;
    }
    
    // NOT: Otomatik su tabancası (lazer/buzzer) ateşleme kısmı şu an kullanılmadığı için kapatıldı
    /*
    if(fire_waterpistol){ 
      // watergun_off_time değişkeni de kapatıldığı için burada kullanılmıyor.
      digitalWrite(DRIVER_PIN,HIGH);
      squirt_t = millis(); 
    }
    */
    
    delay(100);
    first_capture = true;
    finished_tracking = true;
    Serial.print("region_of_interest = ");
    Serial.println(region_of_interest);
  }
}