#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>

#define OLED_SDA 32
#define OLED_SCL 33
// GPS 
#define GPS_RX 21
#define GPS_TX 22
TinyGPSPlus gps;
HardwareSerial GPS_Serial(2);
double latitude = 0.0;
double longitude = 0.0;
bool gpsValid = false;

#define DHTPIN 4
#define DHTTYPE DHT22
#define MQ135_PIN 34
#define LED_GREEN 2
#define LED_YELLOW 27
#define LED_RED 13
#define LORA_SS 5
#define LORA_RST 16
#define LORA_DIO0 17
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const float R4 = 10000, R5 = 20000, RL = 10000, VCC = 5.0;   // GIÁ TRỊ ĐÊ TÍNH MQ135 
const float R0 = 291281.84;      // ĐÃ CHẠY RIÊNG ĐỂ LẤY GIÁ TRỊ HIỆU CHUẨN 

// Khai báo mức ngưỡng để cảnh báo 
#define tem_MAX 35.0
#define hum_MAX 80.0
#define ppm_good 950.0      // ppm < 950 => tốt , 950 <= ppm <= 1500 => cảnh báo, >= 1500 => không khí xấu 
#define ppm_warn 1500.0

float temperature; 
float humidity;
float ppm;

const char* GROUP_ID = "nhom_ba";
int nodeID = 3;
String packet;

void initGPIO(){
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
 
}

void initOLED(){
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    Serial.println("Ket noi OLED that bai");
    while(1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("SYSTEM STARTING...");
  display.display();
  delay(3000);
  display.clearDisplay();
  display.display();
}

void initLoRa(){
  SPI.begin(18, 19, 23, 5);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if(!LoRa.begin(433E6)){
    Serial.println("Khoi dong lora node3 that bai");
    while(1);
  }
  Serial.println("LoRa node3 da khoi dong");
}

void readDHT(){
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if(isnan(temperature) || isnan(humidity)){ 
    Serial.println("Cam bien DHT loi");
    return;
  }
  Serial.print("Nhiet do: ");
  Serial.print(temperature);
  Serial.println(" C");
  Serial.print("Do am: ");
  Serial.print(humidity);
  Serial.println(" %");
}   

// ĐÃ THAY THẾ TOÀN BỘ CODE MQ135 CŨ BẰNG ĐOẠN NÀY
void readMQ135(){
  uint32_t s = 0;
  for (int i = 0; i < 32; i++) { s += analogReadMilliVolts(MQ135_PIN); delay(2); }
  float vao = s / 32000.0 * (R4 + R5) / R5;
  if (vao < 0.01 || vao >= VCC) return;
  float rs = RL * (VCC - vao) / vao;
  ppm = 116.6020682 * pow(rs / R0, -2.769034857);
  Serial.print("PPM: "); Serial.println(ppm, 1);
}

void updateAlarm(){
  bool temAlarm = (temperature > tem_MAX);
  bool humAlarm = (humidity > hum_MAX);

  if (ppm < ppm_good){
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_RED, LOW);
  }
  else if (ppm < ppm_warn){
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_YELLOW, HIGH);
    digitalWrite(LED_RED, LOW);
  }
  else {
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_RED, HIGH);
  }
  
  
}

void displayOLED(){
   display.clearDisplay();
   display.setTextSize(1);
   display.setTextColor(SSD1306_WHITE);
   
   display.setCursor(0, 0);
   display.print("Tem: ");
   display.print(temperature, 1);
   display.print(" C");
   
   display.setCursor(0, 15);
   display.print("Hum: ");
   display.print(humidity, 1);
   display.print(" %");
   
   display.setCursor(0, 30);
   display.print("PPM: ");
   display.print(ppm, 1);
   display.print(" ppm");
   
   display.display();
}

void createPacket() {
  packet = String(GROUP_ID) + "," +
           String(nodeID) + "," +
           String(temperature, 1) + "," +
           String(humidity, 1) + "," +
           String(ppm, 0) + "," +
           String(latitude, 6) + "," +
           String(longitude, 6) + "," +
           String(gpsValid ? 1 : 0);
}

void sendLoRa(){
  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();
}

void initGPS() {
  GPS_Serial.setRxBufferSize(1024);
  GPS_Serial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS3 da khoi dong");
}

void readGPS() {
  while (GPS_Serial.available() > 0) {
    char c = GPS_Serial.read();
    gps.encode(c);
  }

  static unsigned long lastReport = 0;
  if (millis() - lastReport >= 2000) {
    lastReport = millis();

    Serial.print("GPS bytes: ");
    Serial.println(gps.charsProcessed());

    Serial.print("GPS sentences with fix: ");
    Serial.println(gps.sentencesWithFix());

    Serial.print("GPS age (ms): ");
    Serial.println(gps.location.age());

    if (gps.location.isValid()) {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
      gpsValid = true;

      Serial.print("Latitude: ");
      Serial.println(latitude, 6);

      Serial.print("Longitude: ");
      Serial.println(longitude, 6);
    } else {
      gpsValid = false;
      Serial.println("GPS chua co vi tri hop le");
    }
  }
}

void setup(){
  Serial.begin(115200);
  Wire.begin(OLED_SDA, OLED_SCL);
  initGPIO();
  analogSetPinAttenuation(MQ135_PIN, ADC_0db);
  initOLED();
  dht.begin();
  initLoRa();
  initGPS();
  Serial.println("He thong san sang!");
}

void loop() {
  // Đọc GPS liên tục, không chờ delay
  readGPS();

  // Cứ mỗi 8 giây mới đọc cảm biến và gửi LoRa
  static unsigned long lastSend = 0, interval = nodeID * 1500;

  if (millis() - lastSend >= interval) {
    lastSend = millis();
    interval = 8000 + random(0, 1000);

    readDHT();
    readMQ135();
    updateAlarm();
    displayOLED();

    createPacket();
    sendLoRa();

    Serial.print("Packet LoRa: ");
    Serial.println(packet);
  }
}
