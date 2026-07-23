#include <Arduino.h> 
#include <WiFi.h> 
#include <WiFiClientSecure.h> 
#include <PubSubClient.h> 
#include <Wire.h> 
#include <LiquidCrystal_I2C.h> 
#include <ThreeWire.h> 
#include <RtcDS1302.h> 
#include <time.h> 
#include <Preferences.h>  
#include <DNSServer.h>   
#include <WebServer.h>     
#include <ArduinoJson.h>


#define RX2_PIN 16
#define TX2_PIN 17
#define BAUD_RATE 115200
const String PHONE_NUMBER = "0343544458";
void printSIMResponse();
void sendSMS(String message);
void makeCall();

LiquidCrystal_I2C lcd(0x27, 16, 2); // 0x27 định danh số Hex của chip chuyển đổi PCF 8574
unsigned long lastLCDUpdateTime = 0; // truyen khai lap trinh khong chan

ThreeWire myWire(23, 32, 12); // 23 data, 32, clk, 12, rst
RtcDS1302<ThreeWire> Rtc(myWire); //khai bao templace , tôi muôn dk con này 
Preferences preferences; 

// KHỞI TẠO SERVER CẤU HÌNH MẠNG LÚC GẠT CÔNG TẮC
DNSServer dnsServer;
WebServer webServer(80);
const byte DNS_PORT = 53;

struct WifiNetWork {
  const char* ssid; 
  const char* password; 
};
WifiNetWork knowNetworks [] = {   
  {"Huỳnh Thế Hiển", "30072006"}, 
  {"UTC2", ""}, 
  {"Minh Tien", "dededede"},
  {"My Linh", "7777773968"}
};

const char* mqtt_server = "27.71.25.224";
const int mqtt_port = 8883;

WiFiClientSecure espClient; 
PubSubClient client; 

// =================== CHỨNG CHỈ BẢO MẬT SSL =================== thiet lap lien ket ma hoa an toan
static const char ca_cert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDmTCCAoGgAwIBAgIUOBwsOvYWfB/1BPXcXJi1BJqsVKkwDQYJKoZIhvcNAQEL
BQAwXDELMAkGA1UEBhMCVk4xFTATBgNVBAcMDERlZmF1bHQgQ2l0eTEcMBoGA1UE
CgwTRGVmYXVsdCBDb21wYW55IEx0ZDEYMBYGA1UEAwwPMTI1LjIxMi4yNDAuMTAw
MB4XDTI2MDQzMDAzNTgxOVoXDTM2MDQyNzAzNTgxOVowXDELMAkGA1UEBhMCVk4x
FTATBgNVBAcMDERlZmF1bHQgQ2l0eTEcMBoGA1UECgwTRGVmYXVsdCBDb21wYW55
IEx0ZDEYMBYGA1UEAwwPMTI1LjIxMi4yNDAuMTAwMIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEA7UGRjeIysbTINuS1QadJ63gM/ylRdbjL6jMsOpzsIZ+Z
FJ3AoO+2bTj8zrW3sLOUcfoJf1us5O8gE6RnLqCig2+96BlR08f4ztPhYP4t76Y/
ZTRjl8YkBiJp9NAyDV1Ow8QnlassSgemFgzD/FYqdBu8WX+Z1sVPhIBPsPwhvALY
s8+JUrFlO9by+1kyA3/1JlACx6D7yr04nYtTbXRHJbcYVRq8WOondYprkgNl0b7t
J+X+WzBrtjqej7FEFOhRdwK5hv0vVCN+PaG0GYD2x/y5lq9MdjDJj4anBVaQiT2O
l6ewAqyhmBBVGOpFuIICWI94pOmpp0qnZZiIQHaPNQIDAQAB
-----END CERTIFICATE-----
)EOF";

bool isAlarmTriggered = false;  // co logic dung sai de quan li trnag thai bao dong, danh dau buoc vao chu ki bao dong chưa
bool currentAlarm = false; // bao hieu hien tai có vuot nguong hay khong
bool buzzerState = false; 
unsigned long lastSensorReadTime = 0; unsigned long lastBuzzerTime = 0; unsigned long lastNetworkCheck = 0; unsigned long offlineTime = 0;// quan li time tac vu
// co che chong nhieu phim bam
bool isManualOverride = false; 
float currentTemp = 0.0; float currentHum = 0.0;
bool lastBtnBomState = HIGH, bomStateStabilized = HIGH;
bool lastBtnDenState = HIGH, denStateStabilized = HIGH;

// Nút bấm điều khiển Chế độ Hệ thống
bool lastBtnModeState = HIGH, modeStateStabilized = HIGH; 
unsigned long lastDebounceMode = 0;

unsigned long lastDebounceBom = 0;
unsigned long lastDebounceDen = 0;
const int DEBOUNCE_DELAY = 50; 

// =================== BIẾN ĐIỀU KHIỂN CẢNH BÁO QUA SIM ===================
unsigned long alarmStartTime = 0;  // khoan khac bat dau vuot nguong
int smsSentCount = 0;          // bo dem tin nhan da gui
unsigned long lastSmsSentTime = 0;     // quan li khoan gian cach
bool callInitiated = false;            // co danh dau

// =================== CÔNG TẮC VẬT LÝ CHUYỂN CHẾ ĐỘ AP + STA ===================
const int wifi_switch_pin = 13; 
bool isApModeActive = false; 
bool lastSwitchState = HIGH;

String currentNetworkMode = "AUTO"; 

// =================== BỘ NHỚ LƯU TRỮ OFFLINE ===================
struct OfflineRecord {
  float temp;
  float hum;
  uint32_t timestamp; 
};
#define MAX_RECORDS 300 
OfflineRecord offlineData[MAX_RECORDS];
int offlineCount = 0;

const int led_dong_co_bom = 18, led_den_suoi_am = 5, analog_pin = 34; // output
const int relay_dongcobom = 25, relay_densuoiam = 26, relay_phunsuong = 27, relay_trong = 33; // output 

const int dieukhien_dcbom = 19, dieukhien_densuoi = 14, dieukhien_mode = 2, loa_canh_bao = 4; 
int nguong_nhiet_do_thap = 20, nguong_nhiet_do_cao = 25, thap_canh_bao = 15, cao_canh_bao = 30; 

void updateLCD() {
  lcd.clear(); 
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(currentTemp, 1);
  lcd.print((char)223); 
  lcd.print("C ");
  lcd.print("H:");
  lcd.print(currentHum, 1);
  lcd.print("%");
  
  lcd.setCursor(0, 1);
  if (isApModeActive) {
    lcd.print("WEB: CONFIG AP "); 
  } else if (WiFi.status() != WL_CONNECTED) { // lien luc kiem tr và tra ve trnag thai hien tai cua modem
    if(WiFi.getMode() == WIFI_AP_STA) { // che do hien tại cua modem esp
      lcd.print("AP: CONFIG...  ");
    } else {
      lcd.print("WIFI: DISCONN  ");
    }
  } else if (!client.connected()) { 
    lcd.print("MQTT: LOSS     ");
  } else {
    lcd.print("NET: " + currentNetworkMode + "  ");
  }

  lcd.setCursor(12, 1);
  if (isManualOverride) {
    lcd.print("[M]"); 
  } else {
    lcd.print("[A]"); 
  }
}

void syncTimeWithNTP() {
  if (WiFi.status() == WL_CONNECTED && WiFi.RSSI() > -70) {
    configTime(25200, 0, "pool.ntp.org"); 
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 3000)) { // lấy giờ hệ thông và bù trừ data
      RtcDateTime ntpTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec); // lam sach va dong bo cac he so
      Rtc.SetDateTime(ntpTime); // cai moc thgian chuan do làm ds1302
      Serial.println(" Thoi gian da duoc dong bo chuan tu Internet.");
    }
  } else {
    Serial.println(" Mạng yeu hoac mat ket noi, bo qua dong bo NTP.");
  }
}

// === GIAO DIỆN TRANG WEB NHẬP CẤU HÌNH WIFI ===
void handleRoot() { // tạo van ban chua html/css bắn về trình duyệt didenj thoại 
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"; // tỉ lệ vừa màn hình, khong to, khong nhỏ
  html += "<style>body{font-family:Arial;text-align:center;background:#f4f4f4;margin:40px;} .box{background:#fff;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);max-width:300px;margin:auto;} input{width:100%;padding:10px;margin:10px 0;box-sizing:border-box;} button{width:100%;padding:10px;background:#28a745;color:#fff;border:none;border-radius:4px;cursor:pointer;}</style>";
  html += "</head><body><div class='box'><h2>Cấu Hình Wi-Fi Nông Trại</h2>";
  html += "<form action='/save' method='POST'>";
  html += "Tên Wi-Fi (SSID):<input type='text' name='ssid' placeholder='Nhập tên WiFi'>";
  html += "Mật khẩu:<input type='password' name='pass' placeholder='Nhập mật khẩu'>";
  html += "<button type='submit'>Lưu và Kết Nối</button>";
  html += "</form></div></body></html>";
  webServer.send(200, "text/html", html);
}

void handleSave() { // xu li duong dan save từ handleRoot gửi len, hasArg() đóng vai trò là bước kiểm tra xem món hàng đó có nằm trong bưu kiện hay không.
  if (webServer.hasArg("ssid")) { //Kiểm tra an toàn xem trong gói tin POST gửi lên có chứa tham số nào tên là "ssid" hay không.
    String reqSsid = webServer.arg("ssid"); //arg() đóng vai trò là hành động mở hộp và lấy món hàng đó ra sử dụng.
    String reqPass = webServer.arg("pass");
    
    preferences.begin("wifi_store", false);
    preferences.putString("ssid", reqSsid);
    preferences.putString("pass", reqPass);
    preferences.end();
    
    String html = "<html><body style='font-family:Arial;text-align:center;margin-top:50px;'><h2>Đang lưu và tiến hành kết nối mạng: " + reqSsid + "</h2><p>Vui lòng gạt công tắc về vị trí STA cũ để kiểm tra kết nối mạng.</p></body></html>";
    webServer.send(200, "text/html", html);
    delay(1000);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(reqSsid.c_str(), reqPass.c_str());
  }
}

void startCaptivePortal() {
  Serial.println("\n Không kết nối được mạng cũ. Tự động kích hoạt Điểm phát Web cấu hình...");
  lcd.clear(); lcd.setCursor(0, 0); lcd.print("No Known WiFi!");
  lcd.setCursor(0, 1); lcd.print("AP: CAU_HINH_FARM");
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("CAU_HINH_FARM", "12345678"); // hành một cục Router thu nhỏ, tự phát ra một luồng sóng Wi-Fi độc lập
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
  webServer.onNotFound([]() { 
    webServer.send(200, "text/html", "<html><head><script>location.href='http://192.168.4.1/';</script></head></html>"); 
  });
  webServer.begin();
}

void scanWifiAndPublish() {
  Serial.println("Bắt đầu quét mạng WiFi theo yêu cầu từ Web...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
     client.publish("farm/wifi_results", "[]");
     return;
  }
  
  DynamicJsonDocument doc(1024); // đối tượng quản lí bộ nhớ động 
  JsonArray array = doc.to<JsonArray>(); 
  
  int maxScan = (n > 8) ? 8 : n; 
  for (int i = 0; i < maxScan; ++i) {
    JsonObject obj = array.createNestedObject();// chuyên dụng dùng để tạo và chèn thêm một Đối tượng JSON ({ }) vào bên trong một Mảng JSON ([ ]).
    obj["ssid"] = WiFi.SSID(i);
    obj["rssi"] = WiFi.RSSI(i);
  }
  
  String jsonOutput;
  serializeJson(doc, jsonOutput); // đóng gói
  client.publish("farm/wifi_results", jsonOutput.c_str());// chuyển đổi một chuỗi thuộc kiểu String thành một mảng ký tự kiểu C thuần túy.
  Serial.println("Đã gửi danh sách quét WiFi lên MQTT!");
}

void setup_wifi() {
  if (isApModeActive) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("CAU_HINH_FARM", "12345678"); 
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    
    webServer.on("/", handleRoot);
    webServer.on("/save", handleSave);
    webServer.onNotFound(handleRoot); 
    webServer.begin();
    
    Serial.println("\n[ACTIVE] Đang phát Wi-Fi: CAU_HINH_FARM (Mật khẩu: 12345678)");
    return;
  }

  dnsServer.stop();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("\n Đang quét các mạng WiFi xung quanh...");
  lcd.clear(); lcd.setCursor(0, 0); lcd.print("Scanning WiFi...");

  preferences.begin("wifi_store", true);
  String storedSsid = preferences.getString("ssid", "");
  String storedPass = preferences.getString("pass", "");
  preferences.end();

  if (storedSsid.length() > 0) {
    WiFi.begin(storedSsid.c_str(), storedPass.c_str());
    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 15) { delay(500); Serial.print("."); t++; }
    if (WiFi.status() == WL_CONNECTED) {
       syncTimeWithNTP();
       return;
    }
  }

  int n_networks = WiFi.scanNetworks();
  if (n_networks == 0) { startCaptivePortal(); return; }

  int bestNetworkIndex = -1;
  int highestRSSI = -100; 
  int totalKnownNetworks = sizeof(knowNetworks) / sizeof(knowNetworks[0]);

  for (int i = 0; i < n_networks; i++) {
    String currentSSID = WiFi.SSID(i);
    int currentRSSI = WiFi.RSSI(i);
    for (int j = 0; j < totalKnownNetworks; j++) {
      if (currentSSID == knowNetworks[j].ssid) {
        if (currentRSSI > highestRSSI) {
          highestRSSI = currentRSSI;
          bestNetworkIndex = j; 
        }
      }
    }
  }

  if (bestNetworkIndex != -1) {
    WiFi.begin(knowNetworks[bestNetworkIndex].ssid, knowNetworks[bestNetworkIndex].password);
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) { delay(500); Serial.print("."); timeout++; }
    if (WiFi.status() == WL_CONNECTED) {
      syncTimeWithNTP(); 
      return;
    }
  }
  startCaptivePortal();
}

void maintainNetwork() {
  if (isApModeActive) {
    dnsServer.processNextRequest();
    webServer.handleClient();
  } else {
    if (WiFi.status() != WL_CONNECTED) {
      setup_wifi();
    }
  }
} 

// =================== XỬ LÝ LỆNH TỪ MQTT (ĐÃ CHUẨN HÓA KHÔNG LO TRÙNG LỆNH) ===================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i]; // ASCII
  Serial.println("Nhận lệnh MQTT: " + msg);

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (error) return;

  // 1. Quét WiFi theo Action chuẩn từ Server
  if (doc.containsKey("action") && (doc["action"] == "scan_wifi" || doc["action"] == "request_wifi_scan")) { // kiem tra trong json tồn tại không 
     scanWifiAndPublish();
     return;
  }

  // 2. Nhận yêu cầu kết nối mạng Wifi mới từ Web
  if (doc.containsKey("action") && doc["action"] == "connect_wifi") {
     String reqSsid = doc["ssid"].as<String>(); // toán tử chuyển đổi 
     String reqPass = doc["pass"].as<String>();
     WiFi.disconnect();
     WiFi.begin(reqSsid.c_str(), reqPass.c_str());
     int checkConnect = 0;
     while (WiFi.status() != WL_CONNECTED && checkConnect < 20) { delay(500); checkConnect++; }
     if (WiFi.status() == WL_CONNECTED) {
        preferences.begin("wifi_store", false);
        preferences.putString("ssid", reqSsid);
        preferences.putString("pass", reqPass);
        preferences.end();
        currentNetworkMode = "WIFI";
     } else {
        setup_wifi();
     }
     return;
  }

  // 3. Xử lý đồng bộ Trạng thái mạng hiển thị
  if (doc.containsKey("network")) {
     currentNetworkMode = doc["network"].as<String>();
     updateLCD();
     return;
  }

  // 4. Đồng bộ tham số cài đặt ngưỡng từ giao diện
  if (doc.containsKey("type") && doc["type"] == "CONFIG") {
    float newTMin = doc["tMin"].as<float>();
    float newTMax = doc["tMax"].as<float>();
    thap_canh_bao = (int)newTMin; 
    cao_canh_bao = (int)newTMax;
    nguong_nhiet_do_thap = thap_canh_bao + 5; 
    nguong_nhiet_do_cao = cao_canh_bao - 5;
    return;
  } 

  // 5. Chuyển đổi chính xác chế độ AUTO / MANUAL
  if (doc.containsKey("mode")) {
    String targetMode = doc["mode"].as<String>();
    if (targetMode == "AUTO") isManualOverride = false;
    else if (targetMode == "MANUAL") isManualOverride = true;
    updateLCD(); 
    return; 
  }

  // 6. Điều khiển trực tiếp thiết bị đầu ra (Chế độ MANUAL)
  if (isManualOverride && doc.containsKey("device") && doc.containsKey("state")) {
    String device = doc["device"].as<String>();
    String state = doc["state"].as<String>();
    int pinState = (state == "ON") ? HIGH : LOW;

    if (device == "bom1") { 
      digitalWrite(relay_dongcobom, pinState); 
      digitalWrite(led_dong_co_bom, pinState); 
    }
    else if (device == "quat1") { 
      digitalWrite(relay_phunsuong, pinState); 
    }
    else if (device == "den1") { 
      digitalWrite(relay_densuoiam, pinState); 
      digitalWrite(led_den_suoi_am, pinState); 
    }
    else if (device == "khac") { 
      digitalWrite(relay_trong, pinState); 
    }
    updateLCD();
  }
}

void reconnectMQTT() {
  if (!client.connected() && !isApModeActive) { // ché độ AP
    if (client.connect("ESP32FarmClient")) { // ầu kết nối lên Broker với cái tên định danh (Client ID)
      client.subscribe("farm/control"); 
      client.subscribe("farm/wifi_scan_req");
      client.publish("farm/sync", "request_sync"); // đồng bộ
      String syncModePayload = "{\"mode\":\"" + String(isManualOverride ? "MANUAL" : "AUTO") + "\"}";
      client.publish("farm/sync_update", syncModePayload.c_str());
    }
  }
}

void printSIMResponse() {
  unsigned long timeout = millis();
  while (!Serial2.available() && millis() - timeout < 1000) delay(10); // Nếu cổng Serial2 vẫn đang TRỐNG RỖNG (chưa có chữ nào gửi về)"
  while (Serial2.available()) Serial.print(Serial2.readString());
}

void sendSMS(String message) {
  Serial2.println("AT+CMGF=1"); delay(500); printSIMResponse(); // chuyển sang chế độ văn bản thuân ftusy, xem sim ok không
  Serial2.println("AT+CMGS=\"" + PHONE_NUMBER + "\""); delay(500); // lệnh khởi tạo cuộc gửi tin nhắn
  Serial2.print(message); delay(100); Serial2.write(26); delay(3000); 
  printSIMResponse();
}

void makeCall() {
  Serial2.println("ATD" + PHONE_NUMBER + ";"); delay(500); printSIMResponse();
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(BAUD_RATE, SERIAL_8N1, RX2_PIN, TX2_PIN);

  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (!Rtc.IsDateTimeValid()) Rtc.SetDateTime(compiled); // kiểm thgian hop le không
  if (Rtc.GetIsWriteProtected()) Rtc.SetIsWriteProtected(false);
  if (!Rtc.GetIsRunning()) Rtc.SetIsRunning(true); // kiem tra thạch anh có dao động hay không, không thích kích 

  Wire.begin(21, 22); lcd.init(); lcd.backlight();
  lcd.print("ESP32 SmartFarm"); delay(1500);

  pinMode(wifi_switch_pin, INPUT_PULLUP);
  pinMode(dieukhien_dcbom, INPUT_PULLUP); pinMode(dieukhien_densuoi, INPUT_PULLUP);
  pinMode(dieukhien_mode, INPUT_PULLUP); 
  
  pinMode(led_dong_co_bom, OUTPUT); pinMode(led_den_suoi_am, OUTPUT); pinMode(loa_canh_bao, OUTPUT);
  pinMode(relay_densuoiam, OUTPUT); pinMode(relay_dongcobom, OUTPUT); pinMode(relay_phunsuong, OUTPUT); pinMode(relay_trong, OUTPUT);

  digitalWrite(led_den_suoi_am, LOW); digitalWrite(loa_canh_bao, LOW);
  digitalWrite(relay_densuoiam, LOW); digitalWrite(relay_dongcobom, LOW); digitalWrite(relay_phunsuong, LOW); digitalWrite(relay_trong, LOW);
  
  isApModeActive = (digitalRead(wifi_switch_pin) == LOW);
  lastSwitchState = digitalRead(wifi_switch_pin);

  setup_wifi();
  
  client.setBufferSize(1024); 
  espClient.setCACert(ca_cert); espClient.setInsecure(); 
  // khoi tao cau hinh mqtt
  client.setServer(mqtt_server, mqtt_port); client.setCallback(mqttCallback); client.setClient(espClient); 
  Serial2.println("AT"); delay(200); printSIMResponse();
}

void loop() {
  unsigned long currentMillis = millis();
  RtcDateTime nowRTC = Rtc.GetDateTime(); // doc thgian tu phan cung
  uint32_t currentEpoch = nowRTC.Epoch32Time();

  // =================== 1. XỬ LÝ NÚT NHẤN CHUYỂN CHẾ ĐỘ AUTO/MANUAL ===================
  bool currentBtnMode = digitalRead(dieukhien_mode);
  if (currentBtnMode != lastBtnModeState) lastDebounceMode = currentMillis;
  if ((currentMillis - lastDebounceMode) > DEBOUNCE_DELAY) {
    if (currentBtnMode != modeStateStabilized) {
      modeStateStabilized = currentBtnMode;
      if (modeStateStabilized == LOW) { 
        isManualOverride = !isManualOverride; 
        if (client.connected()) {
          String syncModePayload = "{\"mode\":\"" + String(isManualOverride ? "MANUAL" : "AUTO") + "\"}";
          client.publish("farm/sync_update", syncModePayload.c_str());
        }
        updateLCD();
      }
    }
  }
  lastBtnModeState = currentBtnMode;

  // =================== 2. LOGIC ĐIỀU KHIỂN NÚT BẤM BƠM/ĐÈN CŨ ===================
  if (isManualOverride) {
    bool currentBtnBom = digitalRead(dieukhien_dcbom);
    if (currentBtnBom != lastBtnBomState) lastDebounceBom = currentMillis;
    if ((currentMillis - lastDebounceBom) > DEBOUNCE_DELAY) {
      if (currentBtnBom != bomStateStabilized) {
        bomStateStabilized = currentBtnBom;
        if (bomStateStabilized == LOW) { 
          bool isNowOn = !digitalRead(relay_dongcobom); 
          digitalWrite(relay_dongcobom, isNowOn); digitalWrite(led_dong_co_bom, isNowOn);
          if (client.connected()) {
            String payload = "{\"device\":\"bom1\",\"state\":\"" + String(isNowOn ? "ON" : "OFF") + "\"}";
            client.publish("farm/sync_update", payload.c_str());
          }
          updateLCD();
        }
      }
    }
    lastBtnBomState = currentBtnBom;

    bool currentBtnDen = digitalRead(dieukhien_densuoi);
    if (currentBtnDen != lastBtnDenState) lastDebounceDen = currentMillis;
    if ((currentMillis - lastDebounceDen) > DEBOUNCE_DELAY) {
      if (currentBtnDen != denStateStabilized) {
        denStateStabilized = currentBtnDen;
        if (denStateStabilized == LOW) {
          bool isNowOn = !digitalRead(relay_densuoiam);
          digitalWrite(relay_densuoiam, isNowOn); digitalWrite(led_den_suoi_am, isNowOn);
          if (client.connected()) {
            String payload = "{\"device\":\"den1\",\"state\":\"" + String(isNowOn ? "ON" : "OFF") + "\"}";
            client.publish("farm/sync_update", payload.c_str());
          }
          updateLCD();
        }
      }
    }
    lastBtnDenState = currentBtnDen;
  } else {
    lastBtnBomState = digitalRead(dieukhien_dcbom);
    lastBtnDenState = digitalRead(dieukhien_densuoi);
  }

  // =================== 3. LOGIC CÔNG TẮC VẬT LÝ CHUYỂN CHẾ ĐỘ MẠNG (ĐÃ TỐI ƯU CHỐNG NHIỄU CHÂN 13) ===================
  bool currentSwitchState = digitalRead(wifi_switch_pin);
  if (currentSwitchState != lastSwitchState) {
    delay(100); // Thêm thời gian trễ lọc nhiễu dội phím vật lý
    currentSwitchState = digitalRead(wifi_switch_pin); 
    if (currentSwitchState == LOW) { 
      if (!isApModeActive) {
        isApModeActive = true; 
        setup_wifi(); 
        updateLCD(); 
      }
    } 
    else if (currentSwitchState == HIGH) { 
      if (isApModeActive) {
        isApModeActive = false; 
        setup_wifi(); 
        lastNetworkCheck = 0; 
        updateLCD(); 
      }
    }
    lastSwitchState = currentSwitchState;
  }

  if (WiFi.status() != WL_CONNECTED && !isApModeActive) {
    if (offlineTime == 0) offlineTime = currentMillis;
    if (currentMillis - offlineTime > 180000) ESP.restart(); 
  } else { offlineTime = 0; }
  
  if (currentMillis - lastNetworkCheck >= 5000) {
    lastNetworkCheck = currentMillis;
    maintainNetwork(); 
    if (WiFi.status() == WL_CONNECTED && !isApModeActive) reconnectMQTT();
    updateLCD();
  }

  if (isApModeActive) {
    dnsServer.processNextRequest();
    webServer.handleClient();
  }

  if (client.connected() && !isApModeActive) client.loop(); 

  // =================== 4. ĐỌC SENSOR VÀ CHẠY ĐIỀU KHIỂN TỰ ĐỘNG CŨ ===================
  if (currentMillis - lastSensorReadTime >= 2000) {
    lastSensorReadTime = currentMillis;
    float read_analog = analogRead(analog_pin);
    currentTemp = 10.0 + ((float)read_analog / 4095.0) * (45.0 - 10.0); 
    currentHum = 35.0 + ((float)read_analog / 4095.0) * (95.0 - 35.0);

    float lat = 10.762622; float lon = 106.660172;

    if (client.connected() && !isApModeActive) {
      if (offlineCount > 0) {
        for(int i = 0; i < offlineCount; i++) {
          OfflineRecord rec = offlineData[i];
          String oldPayload = "{\"temp\":" + String(rec.temp) + ",\"hum\":" + String(rec.hum) + ",\"lat\":" + String(lat,6) + ",\"lon\":" + String(lon,6) + ",\"net\":\"" + currentNetworkMode + "\",\"epoch\":" + String(rec.timestamp) + "}";
          client.publish("farm/sensor", oldPayload.c_str());
          delay(60); 
        }
        offlineCount = 0; 
      }
      String payload = "{\"temp\":" + String(currentTemp) + ",\"hum\":" + String(currentHum) + ",\"lat\":" + String(lat,6) + ",\"lon\":" + String(lon,6) + ",\"net\":\"" + currentNetworkMode + "\",\"epoch\":" + String(currentEpoch) + "}";
      client.publish("farm/sensor", payload.c_str());
    } else {
      if (offlineCount < MAX_RECORDS) {
        offlineData[offlineCount] = {currentTemp, currentHum, currentEpoch};
        offlineCount++;
      }
    }
    
    currentAlarm = false;
    if (!isManualOverride) { 
      if ( currentTemp >= cao_canh_bao ){
        digitalWrite( relay_dongcobom, HIGH ); digitalWrite(led_dong_co_bom, HIGH); 
        digitalWrite( relay_densuoiam, LOW ); digitalWrite(led_den_suoi_am, LOW); currentAlarm = true ;
      } else if ( currentTemp >= nguong_nhiet_do_cao ){
        digitalWrite( relay_dongcobom, HIGH ); digitalWrite(led_dong_co_bom, HIGH); 
        digitalWrite( relay_densuoiam, LOW ); digitalWrite(led_den_suoi_am, LOW); 
      } else if ( currentTemp >= nguong_nhiet_do_thap ){
        digitalWrite( relay_dongcobom, LOW ); digitalWrite(led_dong_co_bom, LOW); 
        digitalWrite( relay_densuoiam, LOW ); digitalWrite(led_den_suoi_am, LOW); 
      } else if ( currentTemp >= thap_canh_bao ){
        digitalWrite( relay_dongcobom, LOW ); digitalWrite(led_dong_co_bom, LOW);
        digitalWrite( relay_densuoiam, HIGH ); digitalWrite(led_den_suoi_am, HIGH); 
      } else { 
        digitalWrite( relay_dongcobom, LOW ); digitalWrite(led_dong_co_bom, LOW);
        digitalWrite( relay_densuoiam, HIGH ); digitalWrite(led_den_suoi_am, HIGH); currentAlarm = true ;
      }
    } else {
      if (currentTemp >= cao_canh_bao || currentTemp < thap_canh_bao) currentAlarm = true; 
    }

    if (currentAlarm) {
      if (!isAlarmTriggered) {
        isAlarmTriggered = true; alarmStartTime = millis(); smsSentCount = 0; callInitiated = false;
      }
      unsigned long timeElapsed = millis() - alarmStartTime;
      if (timeElapsed >= 3000 && smsSentCount < 3) {
        if (smsSentCount == 0 || (millis() - lastSmsSentTime >= 10000)) {
          smsSentCount++; lastSmsSentTime = millis();
          String smsMsg = "CANH BAO FARM: Nhiet do hien tai la " + String(currentTemp, 1) + "C, dang o muc NGUY HIEM!";
          sendSMS(smsMsg);
        }
      } else if (smsSentCount >= 3 && !callInitiated) {
        callInitiated = true; makeCall();
      }
    } else { 
      if (isAlarmTriggered) { isAlarmTriggered = false; smsSentCount = 0; callInitiated = false; Serial2.println("ATH"); }
    }
    updateLCD();
  }

  // =================== 5. ĐIỀU KHIỂN LOA CẢNH BÁO CŨ ===================
  if (currentAlarm) {
    if (currentMillis - lastBuzzerTime >= 500) {
      lastBuzzerTime = currentMillis; buzzerState = !buzzerState; digitalWrite(loa_canh_bao, buzzerState);
    }
  } else {
    digitalWrite(loa_canh_bao, LOW); buzzerState = false;
  }
}