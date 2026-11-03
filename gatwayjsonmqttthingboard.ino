#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define LORA_SS   5
#define LORA_RST  16
#define LORA_DIO0 17



const char* WIFI_SSID = "phòng 301";
const char* WIFI_PASSWORD = "88888888";


const char* ClientID = "gatwayyy";
const char* MQTT_TOPIC = "datanhomba281207";

const char* MQTT_SERVER = "broker.hivemq.com";
const int MQTT_PORT = 1883;


const char* GROUP_ID = "nhom_ba";


WiFiClient espClient;
PubSubClient mqttClient(espClient);


// DATA

String receivedData = "";

int nodeID = 0;
float temperature = 0;
float humidity = 0;
float ppm = 0;

int rssi = 0;

double latitude = 0.0;
double longitude = 0.0;
int gpsValid = 0;

char groupID[20];


// LORA
void initLoRa()
{
  SPI.begin();

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6))
  {
    Serial.println("LoRa Gateway khoi dong THAT BAI!");
    while (1);
  }

  Serial.println("LoRa Gateway khoi dong THANH CONG!");
}


// =====================================================
// WIFI
// =====================================================
void cnWiFi()
{
  Serial.print("Dang ket noi WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi da ket noi!");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}


// =====================================================
// MQTT
// =====================================================
void initMQTT()
{
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.println("MQTT san sang!");
}


void cnMQTT()
{
  while (!mqttClient.connected())
  {
    Serial.println("Dang ket noi MQTT...");

    if (mqttClient.connect(ClientID))
    {
      Serial.println("MQTT ket noi thanh cong!");
    }
    else
    {
      Serial.print("MQTT loi, state = ");
      Serial.println(mqttClient.state());

      delay(2000);
    }
  }
}


// =====================================================
// NHAN DU LIEU LORA
// =====================================================
void receiveLoRa()
{
  int packetSize = LoRa.parsePacket();

  if (packetSize)
  {
    receivedData = "";

    while (LoRa.available())
    {
      receivedData += (char)LoRa.read();
    }

    // Luu RSSI cua packet vua nhan
    rssi = LoRa.packetRssi();

    Serial.println();
    Serial.println("==============================");

    Serial.print("Nhan LoRa: ");
    Serial.println(receivedData);

    Serial.print("RSSI: ");
    Serial.print(rssi);
    Serial.println(" dBm");

    Serial.println("==============================");
  }
}


// =====================================================
// PHAN TICH DU LIEU LORA
// =====================================================

bool parseData()
{
  // Thu tu:
  // GROUP_ID, nodeID, temp, humidity, ppm,
  // latitude, longitude, gpsValid

  int result = sscanf(
    receivedData.c_str(),
    "%19[^,],%d,%f,%f,%f,%lf,%lf,%d",
    groupID,
    &nodeID,
    &temperature,
    &humidity,
    &ppm,
    &latitude,
    &longitude,
    &gpsValid
  );

  // Chap nhan packet cu 5 truong hoac packet moi 8 truong
  if (result != 5 && result != 8)
  {
    Serial.println("Packet LoRa SAI DINH DANG!");
    return false;
  }

  // Neu packet cu chua co GPS
  if (result == 5)
  {
    latitude = 0.0;
    longitude = 0.0;
    gpsValid = 0;
  }

  if (strcmp(groupID, GROUP_ID) != 0)
  {
    Serial.println("SAI NHOM - BO QUA!");
    Serial.print("Group nhan duoc: ");
    Serial.println(groupID);
    return false;
  }

  Serial.println("NHOM HOP LE!");
  Serial.printf("Node: %d\n", nodeID);
  Serial.printf("Temperature: %.1f C\n", temperature);
  Serial.printf("Humidity: %.1f %%\n", humidity);
  Serial.printf("MQ135: %.0f\n", ppm);
  Serial.printf("RSSI: %d dBm\n", rssi);

  if (gpsValid == 1)
  {
    Serial.printf("Latitude: %.6f\n", latitude);
    Serial.printf("Longitude: %.6f\n", longitude);
  }
  else
  {
    Serial.println("GPS chua co vi tri hop le");
  }

  return true;
}


// =====================================================
// GUI MQTT
// =====================================================

void publishMQTT()
{
  String nodeName = "node" + String(nodeID);

  String data = "{";

  data += "\"nodeId\":\"" + nodeName + "\",";
  data += "\"temp\":" + String(temperature, 1) + ",";
  data += "\"humidity\":" + String(humidity, 1) + ",";
  data += "\"mq135\":" + String(ppm, 0) + ",";
  data += "\"latitude\":" + String(latitude, 6) + ",";
  data += "\"longitude\":" + String(longitude, 6) + ",";
  data += "\"gpsValid\":";
  data += (gpsValid == 1 ? "true" : "false");
  data += ",";
  data += "\"rssi\":" + String(rssi);
  data += "}";

  if (!mqttClient.connected())
  {
    Serial.println("MQTT chua ket noi - khong gui du lieu");
    return;
  }

  if (mqttClient.publish(MQTT_TOPIC, data.c_str(), true))
  {
    Serial.println("\n===== MQTT PUBLISH THANH CONG =====");
    Serial.print("Topic: ");
    Serial.println(MQTT_TOPIC);
    Serial.print("Payload: ");
    Serial.println(data);
    Serial.println("==================================");
  }
  else
  {
    Serial.println("MQTT GUI THAT BAI!");
  }
}

// =====================================================
// SETUP
// =====================================================
void setup()
{
  Serial.begin(115200);

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32 LORA MQTT GATEWAY");
  Serial.println("==============================");

  initLoRa();

  cnWiFi();

  initMQTT();

  cnMQTT();
}


// =====================================================
// LOOP
// =====================================================
void loop()
{
  // Kiem tra MQTT
  if (!mqttClient.connected())
  {
    cnMQTT();
  }

  mqttClient.loop();


  // Nhan LoRa
  receiveLoRa();


  // Neu co packet LoRa
  if (receivedData != "")
  {
    bool validData = parseData();

    // Chi gui MQTT neu dung GROUP
    if (validData)
    {
      publishMQTT();
    }

    // Xoa packet cu
    receivedData = "";
  }


  delay(100);
}