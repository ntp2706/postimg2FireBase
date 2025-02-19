#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#define SSID "NTP"
#define PASSWORD "qwert123"

const String FIREBASE_STORAGE_BUCKET = "**************************";

String Feedback=""; 
String Command="";
String cmd="";
String pointer="";
int flashValue = 0;

byte receiveState=0;
byte cmdState=1;
byte strState=1;
byte questionstate=0;
byte equalstate=0;
byte semicolonstate=0;

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WiFiServer server(80);
WiFiClient client;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 0;
    config.fb_count = 2;
  } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.jpeg_quality = 0;
      config.fb_count = 1;
    }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Khởi tạo camera lỗi 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  sensor_t * s = esp_camera_sensor_get();

  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, 0);
  }

  s->set_framesize(s, FRAMESIZE_QVGA);
  s->set_hmirror(s, 1); 

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Đã kết nối WiFi.");
  Serial.println(WiFi.localIP());
  server.begin();

  ledcAttachChannel(4, 5000, 8, 4);
  ledcWrite(4, flashValue);
}

static const char PROGMEM INDEX_HTML[] = R"rawliteral(

<!DOCTYPE HTML><html>
<head>
  <title>ESP32 CAM</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="preconnect" href="https://fonts.gstatic.com">
  <link href="https://fonts.googleapis.com/css2?family=Roboto&display=swap" rel="stylesheet">
<style>
    body {font-family: 'Roboto', sans-serif; background-color: #f4f4f4;}
    .flashContainer { display: flex; flex-direction: column; align-items: center; position: absolute; top: 10px; right: 10px; }
    .flashLabel img { width: 40px; height: 40px; cursor: pointer; border-radius: 10px; transition: transform 0.3s, box-shadow 0.3s; }
    .flashLabel img:hover { transform: scale(1.1); box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); }
    .barContainer { display: flex; margin-top: 0px; padding: 10px; background-color: #fff; border-radius: 10px; box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1); transition: opacity 0.3s, transform 0.3s; }
    .flashBar { width: 4px; height: 140px; -webkit-appearance: slider-vertical; }
    .input {display: flex; justify-content: center; width: 360px; margin: 40px auto auto auto; padding: 20px; box-shadow: 0px 0px 20px rgba(0,0,0,0.2); background-color: #fff; border-radius: 5px;}
    .inputCamera {width: 320px; height: 290px; display: flex; flex-direction: column; background-color: #ffffff;}
    .inputCamera > div {padding: 0px; margin: 0px auto;}
    button {background-color: #4CAF50; margin: 10px auto; padding: 10px; color: #fff; border: none; border-radius: 5px; cursor: pointer;}
    button:hover {background-color: #45a049; transform: scale(1.1); font-weight: bold;}
    img {width: 320px; height: 240px; background-color: #fff;}
    .hidden {display: none;}
  </style>
</head>
<body>
  <div class="flashContainer">
    <input type="checkbox" class="hidden" id="flashCheckbox" onchange="showBar(this)">
    <label class="flashLabel" for="flashCheckbox">
      <img src="https://firebasestorage.googleapis.com/v0/b/accesscontrolsystem-4f265.appspot.com/o/src%2Flight.jpg?alt=media">
    </label>
    <div class="barContainer hidden" id="barContainer">
      <input type="range" class="default-action flashBar" id="flash" min="0" max="255" value="0" orient="vertical">
    </div>
  </div>
  <div class="input" id="inputContainer">
    <div class="inputCamera">
      <img id="stream" src="" />
      <div class="feature">
        <button id="startStream">Stream</button>
        <button id="getStill">Still</button>
        <button id="saveImage">Save</button>
        <button id="stopStream">Stop</button>
      </div>
    </div>
  </div>
  <iframe class="hidden" id="ifr"></iframe>
</body>  

<script>

function showBar(checkbox) {
    const barContainer = document.getElementById('barContainer');
    const inputContainer = document.getElementById('inputContainer');
    if (checkbox.checked) {
      barContainer.classList.remove('hidden');
    } else {
        barContainer.classList.add('hidden');
      }
  }

document.addEventListener('DOMContentLoaded', function (event) {
  var baseHost = document.location.origin;

  const streamContainer = document.getElementById('stream');
  const stillBtn = document.getElementById('getStill');
  const streamBtn = document.getElementById('startStream');
  const stopBtn = document.getElementById('stopStream');
  const saveBtn = document.getElementById('saveImage');
  const ifr = document.getElementById('ifr');
  const flash = document.getElementById('flash');
     
  var streamState = false;

  streamBtn.onclick = function (event) {
    streamState = true;
    streamContainer.src = location.origin+'/?getstill='+Math.random();
  };

  streamContainer.onload = function (event) {
    if (!streamState) return;
    streamBtn.click();
  };

  function updateValue(flash) {
      let value;
      value = flash.value;
      var query = `${baseHost}/?flash=${value}`;
      fetch(query)
    }

  flash.onchange = () => updateValue(flash);
  
  stopBtn.onclick = function (event) {
    streamState=false;    
    window.stop();
  }   
   
  stillBtn.onclick = () => {
    stopBtn.click();
    streamContainer.src = `${baseHost}/?getstill=${Date.now()}`;
  };

  saveBtn.onclick = function (event) {
    ifr.src = baseHost + '?saveimage=' + Date.now();
  };

});

</script>
</body>
</html>)rawliteral";

void mainpage() {
  String Data="";

  if (cmd!="")
    Data = Feedback;
  else
    Data = String((const char *)INDEX_HTML);
  
  for (int Index = 0; Index < Data.length(); Index = Index+1024) {
    client.print(Data.substring(Index, Index+1024));
  } 
}

camera_fb_t * last_fb = NULL;

void getStill() {

  if (last_fb != NULL) {
    esp_camera_fb_return(last_fb);
  }
  last_fb = esp_camera_fb_get();
  
  if (!last_fb) {
    Serial.println("Lỗi fb");
    delay(1000);
    ESP.restart();
  }
  
  uint8_t *fbBuf = last_fb->buf;
  size_t fbLen = last_fb->len;
  
  for (size_t n = 0; n < fbLen; n += 1024) {
    if (n + 1024 < fbLen) {
      client.write(fbBuf, 1024);
      fbBuf += 1024;
    } else if (fbLen % 1024 > 0) {
      size_t remainder = fbLen % 1024;
      client.write(fbBuf, remainder);
    }
  }

  ledcAttachChannel(4, 5000, 8, 4);
  ledcWrite(4, flashValue);
}

void postImage(String filename) {
  String url = "https://firebasestorage.googleapis.com/v0/b/" + FIREBASE_STORAGE_BUCKET + "/o?name=" + pointer + ".jpg&uploadType=media";

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "image/jpeg");

  int httpResponseCode = http.POST(last_fb->buf, last_fb->len);

  delay(100);

  if (httpResponseCode > 0) {
    Serial.println("Tải lên thành công");
    String response = http.getString();
    Serial.println(response);
  } else {
      Serial.println("Tải lên thất bại");
      Serial.println(httpResponseCode);
      String response = http.getString();
      Serial.println(response);
    }

  http.end();

  ledcAttachChannel(4, 5000, 8, 4);
  ledcWrite(4, flashValue);  
}

void getCommand(char c) {
  if (c=='?') receiveState=1;
  if ((c==' ')||(c=='\r')||(c=='\n')) receiveState=0;
  
  if (receiveState==1)
  {
    Command=Command+String(c);
    
    if (c=='=') cmdState=0;
    if (c==';') strState++;
  
    if ((cmdState==1)&&((c!='?')||(questionstate==1))) cmd=cmd+String(c);
    if ((cmdState==0)&&(strState==1)&&((c!='=')||(equalstate==1))) pointer=pointer+String(c);
    
    if (c=='?') questionstate=1;
    if (c=='=') equalstate=1;
    if ((strState>=9)&&(c==';')) semicolonstate=1;
  }
}

void executeCommand() {
  if (cmd!="getstill") {
    Serial.println("cmd = " + cmd + ", pointer = " + pointer);
  }

  if (cmd=="saveimage") {
      postImage(pointer);
    } else if (cmd=="flash") {
        ledcAttachChannel(4, 5000, 8, 4);
        flashValue = pointer.toInt();
        ledcWrite(4, flashValue);  
          } else Feedback="Command is not defined.";
  if (Feedback=="") Feedback = Command;  
}

void listenConnection() {
  Feedback=""; Command=""; cmd=""; pointer="";
  receiveState=0, cmdState=1, strState=1, questionstate=0, equalstate=0, semicolonstate=0;
  
  client = server.available();

  if (client) { 
    String currentLine = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();             
        getCommand(c);
                
        if (c == '\n') {
          if (currentLine.length() == 0) {    
            if (cmd=="getstill") {
              getStill();      
              } else mainpage(); 
            Feedback="";
            break;
          } else {
              currentLine = "";
            }
        } else if (c != '\r') {
            currentLine += c;
          }

        if ((currentLine.indexOf("/?")!=-1)&&(currentLine.indexOf(" HTTP")!=-1)) {
          if (Command.indexOf("stop")!=-1) {
            client.println();
            client.stop();
          }
          currentLine="";
          Feedback="";
          executeCommand();
        }
      }
    }
    delay(1);
    client.stop();
  }
}

void loop() {  
  listenConnection();
}
