#define TESTE 123v1

#include <Abellion.h>
#include <AWS_IOT.h>
#include "ping.h"
#include "UDHttp.h"
// #include "vfs_api.cpp"

#include <WiFiClientSecure.h>
// #include <UniversalTelegramBot.h>

#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "sdios.h"
#include "FreeStack.h"

#include <SPIFFS.h>
// #include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <NTPClient.h>

#include <TimeLib.h>

#include <SPIFFSEditor.h>

#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <RH_RF95.h>
#include <RHMesh.h>
#include <pthread.h>
#include <PubSubClient.h>
#include <HTTPClient.h>

#include <Update.h>

// #include <SD_MMC.h>

//ESP core especific libraries
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_attr.h"
#include <driver\uart.h>
#include <rom/rtc.h>
// #include "inet.h"

#include <WiFi.h>
#include <Update.h>

#include "EEPROM.h"

#include "FS.h"
#include "SD.h"
#include "SPI.h"

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

// Lora configuration driver
#include <RH_RF95.h>
#include <RHMesh.h>
// #include <RHReliableDatagram.h>

#include "lwip/dns.h"

#include "SdFat.h"
#include "sdios.h"
#include "FreeStack.h"
// extern SdFat SD;

// Set PRE_ALLOCATE true to pre-allocate file clusters.
const bool PRE_ALLOCATE = true;

// Set SKIP_FIRST_LATENCY true if the first read/write to the SD can1
// be avoid by writing a file header or reading the first record.
const bool SKIP_FIRST_LATENCY = true;

// Size of read/write.
const size_t BUF_SIZE = 1024;

// File size in MB where MB = 1,000,000 bytes.
const uint32_t FILE_SIZE_MB = 10;

// Write pass count.
const uint8_t WRITE_COUNT = 2;

// Read pass count.
const uint8_t READ_COUNT = 2;

const uint32_t FILE_SIZE = 1000000UL * FILE_SIZE_MB;

uint32_t buf32[(BUF_SIZE + 3) / 4];
uint8_t *buf = (uint8_t *)buf32;
SdFs sd;
File32 file_sd;

ArduinoOutStream cout(Serial);

#define error(s) sd.errorHalt(&Serial, F(s))

void cidDmp()
{
  cid_t cid;
  if (!sd.card()->readCID(&cid))
  {

    error("readCID failed");
  }
  cout << F("\nManufacturer ID: ");
  cout << hex << int(cid.mid) << dec << endl;
  cout << F("OEM ID: ") << cid.oid[0] << cid.oid[1] << endl;
  cout << F("Product: ");
  for (uint8_t i = 0; i < 5; i++)
  {
    cout << cid.pnm[i];
  }
  cout << F("\nVersion: ");
  cout << int(cid.prv_n) << '.' << int(cid.prv_m) << endl;
  cout << F("Serial number: ") << hex << cid.psn << dec << endl;
  cout << F("Manufacturing date: ");
  cout << int(cid.mdt_month) << '/';
  cout << (2000 + cid.mdt_year_low + 10 * cid.mdt_year_high) << endl;
  cout << endl;
  vTaskDelay(1000);
}

String getMacAddress();

uint32_t epochTime;
int offset = -3;

#define GPSPort Serial1

TaskHandle_t TaskHandle_1;
TaskHandle_t TaskHandle_2;
TaskHandle_t TaskHandle_3;
TaskHandle_t TaskHandle_4;
TaskHandle_t TaskHandle_5;
TaskHandle_t TaskHandle_6;

//Serial.println("request file open...");

// AWS_IOT awsClient;

// char HOST_ADDRESS[] = "a37xdfod1g833h.iot.us-east-1.amazonaws.com";
// char CLIENT_ID[] = "ESP-Gateway_data";
// char TOPIC_NAME[] = "Gateway_1/status";

// https://s3-us-west-2.amazonaws.com/otabuckettester
int port = 80;
// Validação da resposta da S3
int contentLength = 0;
bool isValidContentType = false;

//Caminho do arquivo para realização do OTA
const char *bin = "/ESP32_gateway_Async.ino.bin";
const char *version = "0.1";
const char *host = "otabuckettester.s3-us-west-2.amazonaws.com";

//HardwareSerial GPSPort(1);

// const uint8_t AnimationChannels = 1;

// NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> strip(1, 25);

// NeoPixelAnimator animations(AnimationChannels);

// struct MyAnimationState
// {
//   RgbwColor StartingColor;
//   RgbwColor EndingColor;
// };

// MyAnimationState animationState[AnimationChannels];

int colorSaturation = 100; // saturation of color constants

WiFiClient espClient,
    Client,
    S3Client;
PubSubClient mqtt_client(espClient);

Abellion GW(Client);

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

float Battery_Calibration = 0;
float Incomming_Battery_Calibration = 0;
bool calibration_cycle;

WebSocketsServer webSocket = WebSocketsServer(81);

String lastUpdate, update;

IPAddress adr_google = IPAddress(8, 8, 8, 8);
IPAddress adr_mqtt = IPAddress(34, 228, 111, 174);

float ping_mqtt;
float ping_google;

long limit_ping = 10000;

String devname = "dev" + getMacAddress();

char s_devname[25];

const char *ntp_server = "3.br.pool.ntp.org";
String externalIP = "null";

char sub_topics[20][50];
char pub_topics[20][50];

int max_reconnect = 50;
int number_of_reconnects;

char mqtt_server[50];

char *psdRamBuffer;
char *msg;
// char msg[10000];

int total_stations;
int total_old_data;

TinyGPSPlus gps;

float flat, flon, alt;
float readed_gps_flat, readed_gps_flon;
long last_fix;
unsigned long age = 0;
const int max_stations = 50;
const int max_remote_sensors = 10;

// Estation estationdata[max_stations];
Estation *(estationdata[max_stations]);

remoteSensors remoteSensors[max_remote_sensors], incommingSensor;

struct Connections_to_verify
{
  uint8_t station_address;
  bool valid = false;
};

Connections_to_verify stations_to_check[max_stations];

bool has_stations_to_check = false;

TransmissionData Incomming;
TransmissionDataSmall IncommingSmall;

String data_in;
int connected_stations = 0;

double Internal_battery_voltage = 0;
bool External_power_connected = false;

bool external_power_event = false, external_power_last = false;
bool power_changed_state = false, first_state = true;

int address_gateway = 100;
bool radio_busy = false;

char wifi_config_file[] = "/wifi.json";
bool connecting_on_wifi = false;
bool connected_to_cloud = false;
bool mqtt_connected = false;

//Status Variables
bool position_changed = false;
volatile bool upload_file = false;
volatile bool download_file = false;

float received_size;
float actual_received_size = 0;
uint32_t previous_time = 0;
float previous_size = 0;
bool downloading_file = false;
float actual_value = 0;
bool mqtt_connection_status = false;

bool cancel_sending_file = false;

DynamicJsonDocument EXT_RAM_ATTR filelistbuffer(4096);
JsonArray jarray = filelistbuffer.to<JsonArray>();

DynamicJsonDocument EXT_RAM_ATTR folderlistbuffer(4096);

int status_LED = WIFI_OFFLINE, status_LED_now = 0x00;

cid_t m_cid;
csd_t m_csd;
uint32_t m_eraseSize;
uint32_t m_ocr;

/////////////////////////////////////////////////////////////////////////////////
// RHHardwareSPI vspi(0, 0, 0);

RH_RF95 rf95(5, 4);
RHMesh manager(rf95, address_gateway);

int LoraMode = 2;
int ActualLoraMode = 0;

unsigned long timeout_for_connection = 1000;
long delta_interval = 0;

/////////////////////////////////////////////////////////////////////////////////
// SKETCH BEGIN
AsyncWebServer server(80);
AsyncWebSocket websocket("/websocket");
AsyncEventSource events("/events");

void print_reset_reason(RESET_REASON reason)
{
  switch (reason)
  {
  case 1:
    Serial.println("POWERON_RESET");
    break; /**<1, Vbat power on reset*/
  case 3:
    Serial.println("SW_RESET");
    break; /**<3, Software reset digital core*/
  case 4:
    Serial.println("OWDT_RESET");
    break; /**<4, Legacy watch dog reset digital core*/
  case 5:
    Serial.println("DEEPSLEEP_RESET");
    break; /**<5, Deep Sleep reset digital core*/
  case 6:
    Serial.println("SDIO_RESET");
    break; /**<6, Reset by SLC module, reset digital core*/
  case 7:
    Serial.println("TG0WDT_SYS_RESET");
    break; /**<7, Timer Group0 Watch dog reset digital core*/
  case 8:
    Serial.println("TG1WDT_SYS_RESET");
    break; /**<8, Timer Group1 Watch dog reset digital core*/
  case 9:
    Serial.println("RTCWDT_SYS_RESET");
    break; /**<9, RTC Watch dog Reset digital core*/
  case 10:
    Serial.println("INTRUSION_RESET");
    break; /**<10, Instrusion tested to reset CPU*/
  case 11:
    Serial.println("TGWDT_CPU_RESET");
    break; /**<11, Time Group reset CPU*/
  case 12:
    Serial.println("SW_CPU_RESET");
    break; /**<12, Software reset CPU*/
  case 13:
    Serial.println("RTCWDT_CPU_RESET");
    break; /**<13, RTC Watch dog Reset CPU*/
  case 14:
    Serial.println("EXT_CPU_RESET");
    break; /**<14, for APP CPU, reseted by PRO CPU*/
  case 15:
    Serial.println("RTCWDT_BROWN_OUT_RESET");
    break; /**<15, Reset when the vdd voltage is not stable*/
  case 16:
    Serial.println("RTCWDT_RTC_RESET");
    break; /**<16, RTC Watch dog reset digital core and rtc module*/
  default:
    Serial.println("NO_MEAN");
  }
}
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    Serial.printf("websocket[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    Serial.printf("websocket[%s][%u] disconnect: %u\n", server->url(), client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    Serial.printf("websocket[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_PONG)
  {
    Serial.printf("websocket[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
  }
  else if (type == WS_EVT_DATA)
  {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    String msg = "";
    if (info->final && info->index == 0 && info->len == len)
    {
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("websocket[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

      if (info->opcode == WS_TEXT)
      {
        for (size_t i = 0; i < info->len; i++)
        {
          msg += (char)data[i];
        }
      }
      else
      {
        char buff[3];
        for (size_t i = 0; i < info->len; i++)
        {
          sprintf(buff, "%02x ", (uint8_t)data[i]);
          msg += buff;
        }
      }
      Serial.printf("%s\n", msg.c_str());

      if (info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    }
    else
    {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0)
      {
        if (info->num == 0)
          Serial.printf("websocket[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        Serial.printf("websocket[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("websocket[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

      if (info->opcode == WS_TEXT)
      {
        for (size_t i = 0; i < info->len; i++)
        {
          msg += (char)data[i];
        }
      }
      else
      {
        char buff[3];
        for (size_t i = 0; i < info->len; i++)
        {
          sprintf(buff, "%02x ", (uint8_t)data[i]);
          msg += buff;
        }
      }
      Serial.printf("%s\n", msg.c_str());

      if ((info->index + len) == info->len)
      {
        Serial.printf("websocket[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final)
        {
          Serial.printf("websocket[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          if (info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
}

bool LoraConnected = false;
uint8_t Status = WIFI_OFFLINE;
uint8_t old_Status;

void taskOne(void *pvParameters)
{
  Serial.println("<booting> Task one ------------------------------");
  for (;;)
  {

    if (connected_stations < 1)
    {
      checkNewDevices();
      vTaskDelay(10000);
    }
    else
    {
      checkNewDevices();
      vTaskDelay(60000);
    }
  }
}
void taskGPS(void *pvParameters)
{
  Serial.println("<booting> Task GPS ------------------------------");
  int bits_count = 0;
  unsigned long start = millis();
  unsigned long age = 1000;
  for (;;)
  {
    while (GPSPort.available())
    {
      char in = GPSPort.read();

      gps.encode(in);
      //bits_count++; /stripchart /computer:3.br.pool.ntp.org /dataonly /samples:10
      // Serial.print(in);
    }

    if (millis() > start + 5000 && gps.charsProcessed() < 10)
      Serial.println(F("No GPS data received: check wiring"));
    adjustInternalTime();
    // gps.f_get_position(&readed_gps_flat, &readed_gps_flon, &age);

    if (gps.location.isValid())
    {
      // Serial.print(gps.location.lat(), 6);
      // Serial.print(F(","));
      // Serial.print(gps.location.lng(), 6);
      // Serial.print(" alt: ");
      // Serial.println(gps.altitude.meters(), 6);

      // if (GW.gateway_std_data.stored_lon != gps.location.lng() && GW.gateway_std_data.stored_lat != gps.location.lat())
      if (TinyGPSPlus::distanceBetween(GW.gateway_std_data.stored_lat, GW.gateway_std_data.stored_lon, gps.location.lat(), gps.location.lng()) > 5)
      {
        // Serial.print(gps.location.lat(), 6);
        // Serial.print(",");
        // Serial.print(gps.location.lng(), 6);
        // Serial.print(" diference distance: ");
        // Serial.print(TinyGPSPlus::distanceBetween(GW.gateway_std_data.stored_lat, GW.gateway_std_data.stored_lon, gps.location.lat(), gps.location.lng()), 2);
        // Serial.print(" alt: ");
        // Serial.println(gps.altitude.meters(), 6);

        GW.gateway_std_data.stored_lat = gps.location.lat();
        GW.gateway_std_data.stored_lon = gps.location.lng();
        GW.gateway_std_data.stored_alt = gps.altitude.meters();
        GW.gateway_std_data.last_fix = now();

        flat = GW.gateway_std_data.stored_lat;
        flon = GW.gateway_std_data.stored_lon;
        alt = GW.gateway_std_data.stored_alt;
        position_changed = true;

        // adjustInternalTime();
        saveConfig();
        // GPSPort.print("$PMTK225,0*2B\r\n");
        // GPSPort.print("$PMTK223,1,25,180000,60000*38\r\n");
        // GPSPort.print("$PMTK225,2,3000,12000,18000,72000*15\r\n");
        // vTaskDelay(10000);
      }
    }
    else
    {
      if (GW.gateway_std_data.stored_lon != flon && GW.gateway_std_data.stored_lat != flat)
      {
        Serial.println(F("Position restored from flash..."));
        loadConfig();
        flat = GW.gateway_std_data.stored_lat;
        flon = GW.gateway_std_data.stored_lon;
        alt = GW.gateway_std_data.stored_alt;
        Serial.print("flat: ");
        Serial.print(flat, 6);
        Serial.print(" flat: ");
        Serial.print(flon, 6);
        Serial.print(" alt: ");
        Serial.println(alt, 6);
      }
    }
    vTaskDelay(10);
  }
}
void taskLED(void *pvParameters)
{
  GW.LEDTask(status_LED);
}

void printLocalTime()
{

  char sz[50];

  sprintf(sz, "%02d/%02d/%02d  - %02d:%02d:%02d", day(now()), month(now()), year(now()), hour(now()), minute(now()), second(now()));
  Serial.print(sz);
}

void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  status_LED = MQTT_CONNECTING;
  mqttClient.connect();
}
void WiFiEvent(WiFiEvent_t event)
{
  // Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event)
  {
  case SYSTEM_EVENT_STA_GOT_IP:
    ip_addr_t dnsserver;

    Serial.println("---------------------------------------------------------------------------------WiFi connected");

    IP_ADDR4(&dnsserver, 1, 1, 1, 1);
    dns_setserver(0, &dnsserver);
    IP_ADDR4(&dnsserver, 8, 8, 8, 8);
    dns_setserver(1, &dnsserver);
    IP_ADDR4(&dnsserver, 9, 9, 9, 9);
    dns_setserver(2, &dnsserver);

    status_LED = WIFI_CONNECTED;
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    externalIP = GetExternalIP();
    // delay(100);
    connectToMqtt();

    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    Serial.println("WiFi lost connection");
    status_LED = WIFI_OFFLINE;
    xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    xTimerStart(wifiReconnectTimer, 0);
    break;
  }
}
void onMqttConnect(bool sessionPresent)
{
  status_LED = MQTT_CONNECT;
  Serial.println("MQTTConected...");
  char startingup[256];

  mqtt_connected = true;

  DynamicJsonDocument jsonBuffer(1024);
  JsonObject config = jsonBuffer.to<JsonObject>();

  config["Latitude"] = GW.gateway_std_data.stored_lat;
  config["Longitude"] = GW.gateway_std_data.stored_lon;
  //GW.gateway_std_data.stored_alt = gps.f_altitude();
  config["lastconnection"] = now();
  String date = String(day(now()));
  String hour_s = String(hour(now()));
  config["lastconnection_date"] = String(date + "/" + month(now()) + "/" + year(now()));
  config["lastconnection_time"] = String(hour_s + ":" + minute(now()) + ":" + second(now()));

  config["LocalIP"] = String(WiFi.localIP().toString());
  config["externalIP"] = externalIP;

  serializeJson(config, startingup);

  mqttClient.publish(pub_topics[0], 2, true, startingup);

  for (int i = 0; i < 20; i++)
  {
    if (strlen(sub_topics[i]) > 0)
    {
      mqttClient.subscribe(sub_topics[i], 1);
      Serial.printf("-  topic subscribed: %s -- index %d \r\n", sub_topics[i], i);
    }
  }
  vTaskDelay(1000);
  status_LED = MQTT_CONNECT;
}
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  Serial.println("Disconnected from MQTT.");
  mqtt_connected = false;

  if (WiFi.isConnected())
  {
    xTimerStart(mqttReconnectTimer, 0);
  }

  char filename[64];

  createDir(SD, "/readed_data");

  sprintf(filename, "/ConnLost/LC-%ld.json", now());

  File ConnLostFile = SD.open(filename, FILE_WRITE);

  if (ConnLostFile)
  {
    DynamicJsonDocument jsonBuffer(1024);
    JsonObject eventinfo = jsonBuffer.to<JsonObject>();

    String date = String(day(now()));
    String hour_s = String(hour(now()));
    eventinfo["lastconnection_date"] = String(date + "/" + month(now()) + "/" + year(now()));
    eventinfo["lastconnection_time"] = String(hour_s + ":" + minute(now()) + ":" + second(now()));

    eventinfo["start_of_connection_loss_epoch"] = now();
    eventinfo["Connected_stations"] = connected_stations;

    serializeJson(eventinfo, ConnLostFile);
  }
  else
  {
    Serial.println("unable to open file");
  }
  ConnLostFile.close();
  status_LED = MQTT_CONNECTING;
}

String folder_name;
String file_name;

bool inside_folder = false;
bool previous_file_was_folder = false;

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  if (External_power_connected == true)
  {
    status_LED = MQTT_CONNECT;
  }

  if (strcmp(topic, sub_topics[2]) == 0)
  {
    Serial.println("<action>Awensoring report\r\n");

    DynamicJsonDocument jsonBuffer(1024);
    JsonObject config = jsonBuffer.to<JsonObject>();

    config["Latitude"] = GW.gateway_std_data.stored_lat;
    config["Longitude"] = GW.gateway_std_data.stored_lon;
    config["altitude"] = GW.gateway_std_data.stored_alt;
    config["lastconnection"] = now();
    String date = String(day(now()));
    String hour_s = String(hour(now()));
    config["lastconnection_date"] = String(date + "/" + month(now()) + "/" + year(now()));
    config["Connected_stations"] = connected_stations;

    config["Free_Memory"] = ((float)ESP.getFreeHeap() / 1000);
    config["Memory_size_Memory"] = ((float)ESP.getHeapSize() / 1000);
    config["External_Free_Memory"] = ((float)ESP.getFreePsram() / 1000);
    config["External_size_Memory"] = ((float)ESP.getPsramSize() / 1000);
    config["SSID"] = WiFi.SSID();
    config["Wifi_Power"] = WiFi.RSSI();

    float cardSize = 0;
    float used = 0;
    // float cardSize = SD.card()->cardSize() * 0.000512;
    // float used = cardSize - SD.vol()->freeClusterCount() * SD.vol()->blocksPerCluster() * 0.000512;

    config["Disk_size"] = ((float)cardSize);
    config["Disk_used_space"] = (used);

    if (!first_state)
    {
      config["Ping_to_Cloud"] = ping_mqtt;
      config["Ping_to_Google"] = ping_google;

      config["Battery_Voltage"] = String(Internal_battery_voltage, 3);
      config["External_Power_Connected"] = String(External_power_connected);
      config["External_Power_Value"] = String(analogRead(32));
    }

    config["LocalIP"] = String(WiFi.localIP().toString());
    config["ExternalIP"] = externalIP;

    serializeJson(config, msg, 10000);

    mqttClient.publish(pub_topics[2], 0, false, msg);
  }

  // Message request to send to some station a file stored on the gateway internal SD card
  else if (strcmp(topic, sub_topics[3]) == 0)
  {
    Serial.println("<action>TESTING OTA FUNCTION!\r\n");
    DynamicJsonDocument config(1024);
    // JsonObject config = jsonBuffer.parseObject((char *)payload);
    // JsonObject config = jsonBuffer.to<JsonObject>();

    auto error = deserializeJson(config, (char *)payload);
    if (error)
    {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
      return;
    }
    else
    {
      host = config["Host_address"];
      bin = config["File"];
      version = config["Firmware_version"];
      serializeJsonPretty(config, Serial);
      // config.prettyPrintTo(Serial);
    }

    Serial.println("<action>Starting OTA\r\n");
    snprintf(msg, 75, "{\"Time\":\"%lu\"}", now());
    Serial.println(msg);
    mqtt_client.publish(pub_topics[1], msg);

    upload_file = true;
  }

  // Message request to cancel a message been sent
  // TOPIC is /config/OTA_firmware_CANCELL

  else if (strcmp(topic, sub_topics[8]) == 0)
  {
    Serial.println("<action>ABORTING TRANSMISSION REQUEST\r\n");
    if (upload_file)
      cancel_sending_file = true;
  }
  else if (strcmp(topic, sub_topics[9]) == 0)
  {
    Serial.println("<action>REQUEST TO SHUTDOWN\r\n");
    vTaskDelete(TaskHandle_6);
    // strip.SetPixelColor(0, RgbwColor(0, 0, 0, 0));
    // strip.Show();
    // vTaskDelay(10000);
    //shuting down LED

    vTaskDelay(100);
    //shuting down LORA radio
    rf95.sleep();
    digitalWrite(2, LOW);
    //shuting down GPS
    GPSPort.print("$PMTK161,0*28\r\n");
    vTaskDelay(1000);

    esp_sleep_enable_ext0_wakeup(GPIO_NUM_32, 1);
    esp_deep_sleep_start();
  }
  else if (strcmp(topic, sub_topics[1]) == 0)
  {

    Serial.println("<action>RESETTING\r\n");
    snprintf(msg, 75, "{\"Time\":\"%lu\"}", now());
    Serial.println(msg);
    mqttClient.publish(pub_topics[1], 0, false, msg);
    vTaskDelay(1000);
    ESP.restart();
  }
  else if (strcmp(topic, sub_topics[5]) == 0)
  {

    // filelistbuffer.clear();

    listDir(SD, "/", 1);
    Serial.println("----------------------------------------------------------------------------------------------------\r\n");

    // jarray.prettyPrintTo(Serial);
    serializeJsonPretty(jarray, Serial);
    Serial.println("<action>Sending file list\r\n");
    // root.prettyPrintTo(Serial);
    serializeJson(jarray, psdRamBuffer, 10000);

    // jarray.printTo(psdRamBuffer, 10000);

    mqttClient.publish(pub_topics[5], 0, false, psdRamBuffer, strlen(psdRamBuffer));
    // delay(100);
    filelistbuffer.clear();
    JsonArray jarray = filelistbuffer.to<JsonArray>();
    folderlistbuffer.clear();
  }
  else if (strcmp(topic, sub_topics[10]) == 0)
  {

    Serial.println("----------------------------------------------------------------------------------------------------\r\n");

    // jarray.prettyPrintTo(Serial);
    serializeJsonPretty(jarray, Serial);
    Serial.println("<action>Calibrating ADC\r\n");
    // root.prettyPrintTo(Serial);
    // Battery_Calibration;

    DynamicJsonDocument jsonBuffer(1024);
    JsonObject config = jsonBuffer.to<JsonObject>();

    auto error = deserializeJson(jsonBuffer, (char *)payload);
    if (error)
    {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
      return;
    }
    else
    {
      Incomming_Battery_Calibration = config["Battery_Calibration"];
      calibration_cycle = true;

      // config.prettyPrintTo(Serial);
      serializeJsonPretty(config, Serial);
    }
  }
  else if (strcmp(topic, sub_topics[6]) == 0)
  {

    Serial.println("<action>TESTING OTA FUNCTION!\r\n");
    DynamicJsonDocument jsonBuffer(1024);
    JsonObject config = jsonBuffer.to<JsonObject>();

    auto error = deserializeJson(jsonBuffer, (char *)payload);
    if (error)
    {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
      return;
    }
    else
    {
      host = config["Host_address"];
      bin = config["File"];
      version = config["Firmware_version"];

      serializeJsonPretty(config, Serial);
      // config.prettyPrintTo(Serial);
    }

    Serial.println("<action>Starting File Download\r\n");
    snprintf(msg, 75, "{\"Time\":\"%lu\"}", now());
    Serial.println(msg);
    mqttClient.publish(pub_topics[1], 0, false, msg, strlen(msg));

    upload_file = true;
  }
}

Update_status update_status;

const int loopTimeCtl = 0;
hw_timer_t *timer = NULL;
void IRAM_ATTR resetModule()
{
  // ets_printf("reboot\n");
  ESP.restart();
}

// void testFileIO(const char *path, uint32_t buffSize, uint32_t numMB)
// {
//   //File file = fs.open(path, FILE_WRITE);
//   File file;
//   file.open(path, FILE_WRITE);
//   size_t i;
//   auto start = millis();
//   uint8_t *buff = new uint8_t[buffSize];
//   auto numToWrite = (numMB * 1024 * 1024) / buffSize;
//   timerWrite(timer, 0);
//   for (i = 0; i < numToWrite; i++)
//   {
//     // timerWrite(timer, 0);
//     //file.write(buf, buffSize);
//     file.write(buff, buffSize);
//     // vTaskDelay(1);
//   }
//   auto end = millis() - start;
//   double seconds = end / 1000;
//   if (seconds < 0)
//   {
//     seconds = 1;
//   }
//   double MBS = numMB / seconds;
//   Serial.printf("%d: MB per Second: %6g :", end, MBS);
//   Serial.printf("Seconds per MB: %6g: numMB %d buffSize %d\n", 1 / MBS, numMB, buffSize);
//   file.close();
//   delete[] buff;
// // }
#define SPI_CLOCK SD_SCK_MHZ(26)

#define ESP32_CONFIG SdSpiConfig(15, DEDICATED_SPI, SPI_CLOCK)

void setup()
{

  SPI_H.begin(14, 12, 13, 15);
  SPI.begin(18, 19, 23, 5);

  vTaskDelay(1000);
  Serial.begin(250000);

  if (psramInit())
  {
    // Serial.println("PSRAM was found and loaded");
  }
  psdRamBuffer = (char *)ps_malloc(10000);
  msg = (char *)ps_malloc(10000);
  Estation dummy_station;
  // estationdata = (Estation *)ps_malloc(20 * sizeof(Estation));

  for (int i = 0; i < max_stations; i++)
  {
    estationdata[i] = (Estation *)ps_malloc(sizeof(Estation));

    estationdata[i]->lasttimeseen = 0;
    estationdata[i]->config.read_interval = 30000;
    estationdata[i]->active = false;

    memset(estationdata[i]->config.name, 0x00, 10);
  }

  timer = timerBegin(0, 80, true); //timer 0, div 80
  // timerAttachInterrupt(timer, &resetModule, true);
  // timerAlarmWrite(timer, 10000000, false); //set time in us
  // timerAlarmEnable(timer);                 //enable interrupt

  pinMode(25, OUTPUT); //LED
  adcAttachPin(32);
  adcAttachPin(33);

  if (!SPIFFS.begin(true))
  {
    Serial.println("Failed to mount file system");
  }

  xTaskCreatePinnedToCore(
      taskLED,   /* Task function. */
      "taskLED", /* String with name of task. */
      2048,      /* Stack size in words. */
      NULL,      /* Parameter passed as input of the task */
      1,         /* Priority of the task. */
      &TaskHandle_6,
      1); /* Task handle. */

  analogReadResolution(12); //12 bits
  analogSetWidth(12);
  analogSetAttenuation(ADC_11db); //For all pins
  // analogSetCycles(255);

  // analogSetSamples(10000);

  // analogSetClockDiv(255); // 1338mS

  pinMode(34, OUTPUT);
  // pinMode(33, OUTPUT);
  pinMode(2, OUTPUT);
  // pinMode(4, OUTPUT);
  // gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT);
  pinMode(13, INPUT_PULLUP);
  // SPI.begin();
  SPI.setFrequency(26000000);
  SPI_H.setFrequency(26000000);
  // digitalWrite(2, HIGH);
  // digitalWrite(4, HIGH);
  // auto hspi = SPIClass(2);
  // vspi = SPIClass(VSPI);
  // vspi.begin();

  // hspi.begin();
  // hspi.setFrequency(20000000);

  // strip.Begin();
  // strip.SetBrightness(125);
  // strip.Show();
  Serial.printf("Started, firmware date: %s\r\n", __DATE__);

  GW.init();
  GW.atual_update_status.update_status_value = 0;

  Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
  // //Internal RAM
  // //SPI RAM
  Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());

  Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());

  Serial.printf(" Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());

  // Serial.println("PSRAM init PASS");
  devname = "dev" + getMacAddress();
  devname.toCharArray(s_devname, 20);
  memcpy(GW.gateway_std_data.name, s_devname, strlen(s_devname));
  Serial.println("PSRAM was found and loaded");
  timerWrite(timer, 0);

  Serial.println(s_devname);
  // delay(100);
  // mySPI2.begin();
  // mySPI2.setFrequency(1000000);

  // if (!SD.begin(15, mySPI2, 20000000))
  // // if (!SD.begin(ESP32_CONFIG))
  // {
  //   Serial.println("NOT INIT");
  //   delay(100);
  //   ESP.restart();
  // }

  // pinMode(15, OUTPUT);
  // pinMode(4, OUTPUT);

  if (!sd.begin(ESP32_CONFIG))
  {
    sd.initErrorHalt(&Serial);
  }
  Serial.println("SD started ok!");

  if (!sd.exists("configuration"))
  {
    if (!sd.mkdir("configuration"))
    {
      error("Create configuration failed");
    }
    cout << F("Created configuration\n");
  }
  if (!sd.exists("images"))
  {
    if (!sd.mkdir("images"))
    {
      error("Create images failed");
    }
    cout << F("Created images\n");
  }

  // saveConfig();
  // loadConfig();

  // while (1)
  // {
  //   float s;
  //   uint32_t t;
  //   uint32_t maxLatency;
  //   uint32_t minLatency;
  //   uint32_t totalLatency;
  //   bool skipLatency;
  //   // digitalW

  //   // delay(10000);

  //   // Serial.println(SPI2._freq);

  //   if (sd.fatType() == FAT_TYPE_EXFAT)
  //   {
  //     cout << F("Type is exFAT") << endl;
  //   }
  //   else
  //   {
  //     cout << F("Type is FAT") << int(sd.fatType()) << endl;
  //   }

  //   cout << F("Card size: ") << sd.card()->sectorCount() * 512E-9;
  //   cout << F(" GB (GB = 1E9 bytes)") << endl;

  //   cidDmp();

  //   // open or create file - truncate existing file.
  //   if (!file_sd.open("bench2.dat", O_RDWR_SD | O_CREAT_SD | O_TRUNC_SD))
  //   {
  //     error("error!");
  //     // break;
  //   }

  //   // fill buf with known data
  //   if (BUF_SIZE > 1)
  //   {
  //     for (size_t i = 0; i < (BUF_SIZE - 2); i++)
  //     {
  //       buf[i] = 'A' + (i % 26);
  //     }
  //     buf[BUF_SIZE - 2] = '\r';
  //   }
  //   buf[BUF_SIZE - 1] = '\n';

  //   cout << F("FILE_SIZE_MB = ") << FILE_SIZE_MB << endl;
  //   cout << F("BUF_SIZE = ") << BUF_SIZE << F(" bytes\n");
  //   cout << F("Starting write test, please wait.") << endl
  //        << endl;

  //   // do write test
  //   uint32_t n = FILE_SIZE / BUF_SIZE;
  //   cout << F("write speed and latency") << endl;
  //   cout << F("speed,max,min,avg") << endl;
  //   cout << F("KB/Sec,usec,usec,usec") << endl;
  //   for (uint8_t nTest = 0; nTest < WRITE_COUNT; nTest++)
  //   {
  //     file_sd.truncate(0);
  //     if (PRE_ALLOCATE)
  //     {
  //       if (!file_sd.preAllocate(FILE_SIZE))
  //       {
  //         error("preAllocate failed");
  //       }
  //     }
  //     maxLatency = 0;
  //     minLatency = 9999999;
  //     totalLatency = 0;
  //     skipLatency = SKIP_FIRST_LATENCY;
  //     t = millis();
  //     for (uint32_t i = 0; i < n; i++)
  //     {
  //       uint32_t m = micros();
  //       file_sd.size();
  //       if (file_sd.write(buf, BUF_SIZE) != BUF_SIZE)
  //       {
  //         error("write failed");
  //       }
  //       m = micros() - m;
  //       totalLatency += m;
  //       if (skipLatency)
  //       {
  //         // Wait until first write to SD, not just a copy to the cache.
  //         skipLatency = file_sd.curPosition() < 512;
  //       }
  //       else
  //       {
  //         if (maxLatency < m)
  //         {
  //           maxLatency = m;
  //         }
  //         if (minLatency > m)
  //         {
  //           minLatency = m;
  //         }
  //       }
  //     }
  //     file_sd.sync();
  //     t = millis() - t;
  //     s = file_sd.size();
  //     cout << s / t << ',' << maxLatency << ',' << minLatency;
  //     cout << ',' << totalLatency / n << endl;
  //   }
  //   cout << endl
  //        << F("Starting read test, please wait.") << endl;
  //   cout << endl
  //        << F("read speed and latency") << endl;
  //   cout << F("speed,max,min,avg") << endl;
  //   cout << F("KB/Sec,usec,usec,usec") << endl;

  //   // do read test
  //   for (uint8_t nTest = 0; nTest < READ_COUNT; nTest++)
  //   {
  //     file_sd.rewind();
  //     maxLatency = 0;
  //     minLatency = 9999999;
  //     totalLatency = 0;
  //     skipLatency = SKIP_FIRST_LATENCY;
  //     t = millis();
  //     for (uint32_t i = 0; i < n; i++)
  //     {
  //       buf[BUF_SIZE - 1] = 0;
  //       uint32_t m = micros();
  //       int32_t nr = file_sd.read(buf, BUF_SIZE);
  //       if (nr != BUF_SIZE)
  //       {
  //         error("read failed");
  //       }
  //       m = micros() - m;
  //       totalLatency += m;
  //       if (buf[BUF_SIZE - 1] != '\n')
  //       {

  //         error("data check error");
  //       }
  //       if (skipLatency)
  //       {
  //         skipLatency = false;
  //       }
  //       else
  //       {
  //         if (maxLatency < m)
  //         {
  //           maxLatency = m;
  //         }
  //         if (minLatency > m)
  //         {
  //           minLatency = m;
  //         }
  //       }
  //     }
  //     s = file_sd.size();
  //     t = millis() - t;
  //     cout << s / t << ',' << maxLatency << ',' << minLatency;
  //     cout << ',' << totalLatency / n << endl;
  //   }
  //   cout << endl
  //        << F("Done") << endl;
  //   file_sd.close();
  //   delay(10000);

  //   //   word len = ether.packetReceive(); // go receive new packets
  //   //   word pos = ether.packetLoop(len); // respond to incoming pings

  //   //   // report whenever a reply to our outgoing ping comes back
  //   //   if (len > 0 && ether.packetLoopIcmpCheckReply(ether.hisip))
  //   //   {
  //   //     Serial.print("  ");
  //   //     Serial.print((micros() - timer) * 0.001, 3);
  //   //     Serial.println(" ms");
  //   //   }

  //   //   // ping a remote server once every few seconds
  //   //   // if (micros() - timer >= 5000000)
  //   //   // {
  //   //   //   ether.printIp("Pinging: ", ether.hisip);
  //   //   //   timer = micros();
  //   //   //   ether.clientIcmpRequest(ether.hisip);
  //   //   // }
  // }
  if (!sd.exists("configuration/config.json"))
    saveConfig();

  loadConfig();

  timerWrite(timer, 0);
  Serial.printf("mqtt server addr: %s\r\n", mqtt_server);

#ifdef DEBUG
  GPSPort.begin();
  GPSPort.begin(4800);
#else
  GPSPort.begin(9600, SERIAL_8N1, 27, 26, false, 20000UL);
  GPSPort.print("$PMTK225,0*2B\r\n");
#endif

  sprintf(sub_topics[0], "%s/config", s_devname);
  sprintf(sub_topics[1], "%s/config/reset", s_devname);
  sprintf(sub_topics[2], "%s/config/status_report", s_devname);
  sprintf(sub_topics[3], "%s/config/OTA_firmware", s_devname);
  sprintf(sub_topics[4], "%s/config/lora_config", s_devname);
  sprintf(sub_topics[5], "%s/config/list_files", s_devname);
  sprintf(sub_topics[6], "%s/config/downloadFile", s_devname);
  sprintf(sub_topics[7], "%s/config/downloadNewFirmware", s_devname);
  sprintf(sub_topics[8], "%s/config/OTA_firmware_CANCELL", s_devname);
  sprintf(sub_topics[9], "%s/power/standby", s_devname);
  sprintf(sub_topics[10], "%s/config/calibration", s_devname);

  sprintf(pub_topics[0], "%s/status/startingup", s_devname);
  sprintf(pub_topics[1], "%s/status/reset", s_devname);
  sprintf(pub_topics[2], "%s/status/report", s_devname);
  sprintf(pub_topics[3], "%s/status/coldstart", s_devname);
  sprintf(pub_topics[4], "%s/status/statusupdate", s_devname);
  sprintf(pub_topics[5], "%s/status/list_files", s_devname);
  sprintf(pub_topics[6], "%s/status/file_LORA_upload", s_devname);
  sprintf(pub_topics[7], "%s/status/downloadFile", s_devname);

  // Serial.printf(" - Total Message size on mesh: %d\r\n - Total Message size on RD: %d\r\n - Total payload on RD: %d ",
  //               RH_MESH_MAX_MESSAGE_LEN,
  //               RH_RF95_MAX_MESSAGE_LEN,
  //               RH_RF95_MAX_PAYLOAD_LEN);

  // sprintf(pub_topics[8], "%s/status/downloadFile", s_devname);

  // for (int i = 0; i < 7; i++)
  // {
  //   Serial.printf("-  topic subscribed: %s\r\n", sub_topics[i]);
  // }
  status_LED = WIFI_CONNECTING;
  status_LED_now = status_LED;

  // Serial.println("CPU0 reset reason: ");
  // print_reset_reason(rtc_get_reset_reason(0));

  // Serial.println("CPU1 reset reason: ");
  // print_reset_reason(rtc_get_reset_reason(1));

  xTaskCreatePinnedToCore(
      taskGPS,   /* Task function. */
      "taskGPS", /* String with name of task. */
      8192,      /* Stack size in words. */
      NULL,      /* Parameter passed as input of the task */
      1,         /* Priority of the task. */
      &TaskHandle_1,
      1); /* Task handle. */

  WiFi.onEvent(WiFiEvent);
  timerWrite(timer, 0);

  mqttReconnectTimer = xTimerCreate("mqttTimer",
                                    pdMS_TO_TICKS(5000),
                                    pdFALSE, (void *)0,
                                    reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));

  Serial.printf("mqtt_server: %s\r\n", mqtt_server);
  const char *mqtt_server_address = "mqtt.comandosolutions.com";
  // const char *mqtt_server_address = "34.228.111.174";
  mqttClient.setServer(mqtt_server_address, 1883);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);

  wifiReconnectTimer = xTimerCreate("wifiTimer",
                                    pdMS_TO_TICKS(5000),
                                    pdFALSE, (void *)0,
                                    reinterpret_cast<TimerCallbackFunction_t>(savewificonfig()));
  // esp_wifi_set_max_tx_power(44);
  printLocalTime();

  timerWrite(timer, 0);

  // status_LED = WIFI_OFFLINE;

  if (!MDNS.begin("gatewayconfig"))
  {
    // Serial.println("Error setting up MDNS responder!");
    while (1)
    {
      delay(10);
    }
  }

  setupLora();

  xTaskCreatePinnedToCore(
      taskOne,   /* Task function. */
      "TaskOne", /* String with name of task. */
      8192,      /* Stack size in words. */
      NULL,      /* Parameter passed as input of the task */
      1,         /* Priority of the task. */
      &TaskHandle_3,
      1); /* Task handle. */

  xTaskCreatePinnedToCore(
      handle_radio,   /* Task function. */
      "handle_radio", /* String with name of task. */
      8192,           /* Stack size in words. */
      NULL,           /* Parameter passed as input of the task */
      1,              /* Priority of the task. */
      &TaskHandle_4,
      0); /* Task handle. */

  // Serial.println("UP MDNS responder!");
  // status_LED = status_LED_now;
  xTaskCreatePinnedToCore(
      taskRunning,   /* Task function. */
      "taskRunning", /* String with name of task. */
      16384,         /* Stack size in words. */
      NULL,          /* Parameter passed as input of the task */
      1,             /* Priority of the task. */
      &TaskHandle_5,
      1); /* Task handle. */

  xTaskCreatePinnedToCore(
      taskLORAupdate,   /* Task function. */
      "taskLORAupdate", /* String with name of task. */
      8192,             /* Stack size in words. */
      NULL,             /* Parameter passed as input of the task */
      0,                /* Priority of the task. */
      NULL,
      1); /* Task handle. */

  xTaskCreatePinnedToCore(
      taskUpdate,   /* Task function. */
      "taskUpdate", /* String with name of task. */
      16384,        /* Stack size in words. */
      NULL,         /* Parameter passed as input of the task */
      1,            /* Priority of the task. */
      NULL,
      1); /* Task handle. */

  Serial.print("Rede: ");
  Serial.println(WiFi.SSID());
  Serial.print("Sinal: ");
  Serial.println(WiFi.RSSI());
  Serial.print("Passwd: ");
  Serial.println(WiFi.psk());
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  timerWrite(timer, 0);
  timerEnd(timer);

  // int8_t j;

  // esp_wifi_get_max_tx_power(&j);;

  websocket.onEvent(onWsEvent);
  server.addHandler(&websocket);

  // webSocket.begin();
  MDNS.addService("http", "tcp", 80);
  // webSocket.onEvent(webSocketEvent);

  events.onConnect([](AsyncEventSourceClient *client) {
    client->send("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
  });
  server.addHandler(&events);

  // server.addHandler(new SPIFFSEditor(SD));

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  server.on("/data.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.printf("Get \"/data.json\" request\r\n");

    AsyncResponseStream *response = request->beginResponseStream("application /json");
    // Exemplo de dados para coleta.
    response->addHeader("Access-Control-Allow-Origin", "*"); // to allow usage from local web page for test & development
    //response->addHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    //AsyncResponseStream *response = request->beginResponseStream("/data.json");
    DynamicJsonDocument jsonBuffer(1024);
    JsonObject root = jsonBuffer.to<JsonObject>();

    if (flat == 1000 && flon == 1000)
    {
      root["flat"] = -27;
      root["flon"] = -47;
    }
    else
    {
      root["flat"] = String(flat, 6);

      root["flon"] = String(flon, 6);
    }
    root["Connected_stations"] = connected_stations;

    root["heap"] = String((float)ESP.getFreeHeap() / 1000, 2);
    root["SSID"] = WiFi.SSID();
    root["wfpw"] = WiFi.RSSI();
    root["ping"] = String(ping_google, 1);

    root["Dev_name"] = s_devname;

    // File estationFile = SD.open("/SD-01042019-STFF3605D9.json", O_RDONLY);

    // if (estationFile)
    // {
    //   request->send(SD, "/SD-01042019-STFF3605D9.json", "text/json");
    // }
    //   Serial.println("File openned! Token: ");
    //   // Serial.println(myDrop.getToken("vNa1OielDlAAAAAAAAAEM3Q6GsJ6FcVwOmZ5KKofVNc"));

    //   myDrop.begin("vNa1OielDlAAAAAAAAAENHZfp-TlffMOidezZFhTZc79BYnKgI3UkyUAI7VsLWpUvNa1OielDlAAAAAAAAAENHZfp-TlffMOidezZFhTZc79BYnKgI3UkyUAI7VsLWpU");
    //   if (myDrop.fileUpload("/SD-01042019-STFF3605D9.json", "/SD-01042019-STFF3605D9.json", 1))
    //   { //Sending a test.txt file to /math/test.txt
    //     Serial.println("File Sent!");
    //   }
    //   else
    //   {
    //     Serial.println("ERRO!");
    //   }
    // }
    // estationFile.close();

    // root.prettyPrintTo(Serial);
    serializeJson(root, *response);
    // root.printTo(*response);
    request->send(response);
    //server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    //root.printTo(response, sizeof(response));
  });
  server.on("/gatewayinfo.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Get \"/gatewayinfo.json\" request received");
    AsyncResponseStream *response = request->beginResponseStream("application /json");
    response->addHeader("Access-Control-Allow-Origin", "*");

    // File configfile = SD.open("/config.json", "r");
    // if (configfile)
    // {
    //   size_t size = configfile.size();
    //   std::unique_ptr<char[]> buf(new char[size]);
    //   configfile.readBytes(buf.get(), size);

    //   DynamicJsonDocument jsonBuffer;

    //   JsonObject config = jsonBuffer.parseObject(buf.get());
    //   config.printTo(*response);
    // }

    DynamicJsonDocument jsonBuffer(1024);
    JsonObject root = jsonBuffer.to<JsonObject>();

    if (flat == 1000 && flon == 1000)
    {
      //flat = -27;
      //flon = -47;
      root["flat"] = -27;
      root["flon"] = -47;
    }
    else
    {
      root["flat"] = flat;
      root["flon"] = flon;
    }
    root["Name"] = s_devname;
    root["Memory"] = (float)ESP.getFreeHeap() / 1000;
    root["SSID"] = WiFi.SSID();
    root["WifiPower"] = WiFi.RSSI();
    root["Connected_stations"] = connected_stations;

    root["server_address"] = mqtt_server;
    root["timeup"] = millis() / 1000;

    serializeJsonPretty(root, *response);
    // root.prettyPrintTo(*response);

    request->send(response);
  });
  server.on("/lora_parameters.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Get \"/lora_parameters.json\" request received");
    AsyncResponseStream *response = request->beginResponseStream("application /json");
    // Exemplo de dados para coleta.
    response->addHeader("Access-Control-Allow-Origin", "*");

    DynamicJsonDocument jsonBuffer(1024);
    JsonObject root = jsonBuffer.to<JsonObject>();
    root["Power"] = "Gateway01";
    root["Power"] = "Gateway01";
    root["Power"] = "Gateway01";
    root["Power"] = "Gateway01";
    root["Power"] = "Gateway01";
    root["Power"] = "Gateway01";

    // root.printTo(*response);
    serializeJson(root, *response);

    request->send(response);
  });
  server.serveStatic("/", SD, "/static").setDefaultFile("index.htm");
  server.onNotFound([](AsyncWebServerRequest *request) {
    Serial.printf("NOT_FOUND: ");
    if (request->method() == HTTP_GET)
      Serial.printf("GET");
    else if (request->method() == HTTP_POST)
      Serial.printf("POST");
    else if (request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if (request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if (request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if (request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if (request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if (request->contentLength())
    {
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for (i = 0; i < headers; i++)
    {
      AsyncWebHeader *h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for (i = 0; i < params; i++)
    {
      AsyncWebParameter *p = request->getParam(i);
      if (p->isFile())
      {
        Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      }
      else if (p->isPost())
      {
        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
      else
      {
        Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });
  server.onFileUpload([](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index)
      Serial.printf("UploadStart: %s\n", filename.c_str());
    Serial.printf("%s", (const char *)data);
    if (final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index + len);
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!index)
      Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char *)data);
    if (index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });

  //Serial.println(mqtt_client.connect("arduino123a"));
  //Serial.println(mqtt_client.publish("mm/p", "Message to MQTT"));

  // if (!mqtt_client.connected())
  // {
  //   status_LED = NETWORK_CONNECTING;
  //   reconnect();
  //   // delay(1000);
  // }

  // MDNS.addService("_http", "_tcp", 80);
  server.begin();

  // mqtt_client.setServer(mqtt_server_address, 1883);
  // mqtt_client.setCallback(callback);
  // reconnect();
}

uint8_t bufLora[200];
int rssi = 0;
uint8_t len = sizeof(bufLora);
uint8_t from;
uint8_t to;
unsigned long timed_loop;
unsigned long timed_send;
bool first_cycle = false;

void loop()
{

  ping_resp returnPing;
  for (;;)
  {
    connected_stations = renewConnection();
    returnPing = ping_start(adr_mqtt, 4, 0, 0, 1);
    ping_mqtt = returnPing.total_time;
    returnPing = ping_start(adr_google, 4, 0, 0, 1);
    ping_google = returnPing.total_time;

    vTaskDelay(100);
  }
}

void taskRunning(void *pvParameters)
{
  for (;;)
  {

    DynamicJsonDocument httprequestjson(1024);
    JsonObject stored_file_info = httprequestjson.to<JsonObject>();

    if (millis() - timed_send > 120000)
    {
      Serial.println("Lost LORA");
      LoraConnected = false;
      setupLora();
    }
    // vTaskDelay(10000);

    // post("/static/favicon.png", "34.238.249.191", 1880, "/fileuploder");

    // //chack if the position has changed
    if (position_changed)
    {
      position_changed = false;
      DynamicJsonDocument jsonBuffer(1024);
      JsonObject config = jsonBuffer.to<JsonObject>();

      config["Latitude"] = GW.gateway_std_data.stored_lat;
      config["Longitude"] = GW.gateway_std_data.stored_lon;
      //GW.gateway_std_data.stored_alt = gps.f_altitude();
      String date = String(day(now()));
      String hour_s = String(hour(now()));
      config["Internal_Date"] = String(date + "/" + month(now()) + "/" + year(now()));
      config["Internal_Time"] = String(hour_s + ":" + minute(now()) + ":" + second(now()));
      config["WifiPower"] = WiFi.RSSI();
      config["Connected_stations"] = connected_stations;

      config["Timeup"] = millis() / 1000;
      config["LocalIP"] = String(WiFi.localIP().toString());

      serializeJson(config, msg, 10000);

      mqtt_client.publish(pub_topics[5], msg, true);
    }
    // double battery_volts = analogRead(33);
    // double external_connection_value = analogRead(32);
    double battery_volts = 0;
    double external_connection_value = 0;
    for (int i = 0; i < 1000; i++)
    {
      battery_volts += analogRead(33);
      external_connection_value += analogRead(32);
      delay(10);
    }

    battery_volts /= 1000;
    external_connection_value /= 1000;

    if (calibration_cycle)
    {
      Battery_Calibration = Incomming_Battery_Calibration / (battery_volts * 6.6 / 4096);
      saveConfig();

      Serial.print("Calibrating - Battery calibration: ");
      Serial.println(Battery_Calibration);

      calibration_cycle = false;
    }

    Internal_battery_voltage = (battery_volts * 6.6 / 4096) * Battery_Calibration;

    // if (power_changed_state && !first_state)
    // {
    //   power_changed_state = false;

    //   DynamicJsonDocument jsonBuffer;
    //   JsonObject config = jsonBuffer.to<JsonObject>();

    //   config["Latitude"] = GW.gateway_std_data.stored_lat;
    //   config["Longitude"] = GW.gateway_std_data.stored_lon;
    //   config["altitude"] = GW.gateway_std_data.stored_alt;
    //   config["lastconnection"] = now();
    //   String date = String(day(now()));
    //   String hour_s = String(hour(now()));
    //   config["lastconnection_date"] = String(date + "/" + month(now()) + "/" + year(now()));
    //   config["lastconnection_time"] = String(hour_s + ":" + minute(now()) + ":" + second(now()));

    //   config["LocalIP"] = String(WiFi.localIP().toString());
    //   config["externalIP"] = externalIP;
    //   config["Battery_Voltage"] = String(Internal_battery_voltage, 3);
    //   config["External_Power_Connected"] = String(External_power_connected);
    //   config["External_Power_Value"] = String(external_connection_value);

    //   // config.prettyPrintTo(Serial);
    //   // if (mqtt_client.connected())
    //   //   mqtt_client.publish(pub_topics[0], msg, true);
    // }

    if (((external_connection_value < 350 && external_connection_value > -1) || external_connection_value > 3000) && !external_power_last) // Retornou a energia no dispositivo
    {
      if (first_state)
      {
        External_power_connected = true;
        // status_LED = status_LED_now;
        power_changed_state = true;
      }
    }
    else if (!((external_connection_value < 350 && external_connection_value > -1) || external_connection_value > 3000) && external_power_last) // Dispositivo desconectou da energia elétrica
    {

      status_LED_now = status_LED;
      // status_LED = LOW_POWER_CONNECTED;
      power_changed_state = true;
    }

    if (!((external_connection_value < 350 && external_connection_value > -1) || external_connection_value > 3000))
    {
      External_power_connected = false;
      // status_LED = LOW_POWER_CONNECTED;
      external_power_last = true;
    }

    else
    {

      External_power_connected = true;
      // status_LED = status_LED_now;
      external_power_last = false;
    }

    // Serial.print("External power: ");
    // Serial.println(External_power_connected);
    // Serial.print("Battery value: ");
    // Serial.println(Internal_battery_voltage, 5);
    // Serial.print("External value : ");
    // Serial.println(external_connection_value);

    int delta = actual_received_size - previous_size;
    float trasnsmit_interval = (millis() - previous_time) / 1000;

    if (upload_file || download_file)
      Serial.printf("  --- file received total %f %\r\n  --- speed %f BPS\r\n",
                    mapDouble(actual_received_size, 0, received_size, 0, 100),
                    float(delta / trasnsmit_interval));

    if (GW.atual_update_status.uploading == true && actual_value != GW.atual_update_status.update_status_value)
    {

      Serial.print(" present programming percentage: ");
      actual_value = mapDouble(GW.atual_update_status.update_status_value, GW.atual_update_status.total_file_size, 0, 0, 100),

      Serial.println(actual_value);
      actual_value = GW.atual_update_status.update_status_value;
    }

    previous_size = actual_received_size;
    previous_time = millis();

    first_state = false;

    vTaskDelay(100);
  }
}
void taskLORAupdate(void *pvParameters)
{
  for (;;)
  {

    if (upload_file)
    {
      Serial.println("------------------------Sending File---------------------");
      send_file_over_LORA("/static/index.htm", 10, 0);
      upload_file = false;
    }
    vTaskDelay(10);
  }
}
bool send_file_over_LORA(const char *path, uint8_t destination, int type_of_file)
{
  File estationFile = SD.open(path, O_RDONLY);
  if (estationFile)
  {

    uint8_t data[RH_MESH_MAX_MESSAGE_LEN - 4];
    uint32_t file_transfer_init_time = millis();
    uint32_t start_time = 0;
    uint32_t end_time = 0;
    char filename[64];
    strcpy(filename, path);
    memset(&data, 0x00, sizeof(data));
    // memset(&filename, 0x00, sizeof(filename));

    int size = estationFile.size();
    char present_byte;
    uint32_t total_bytes = 0;
    int present_page_bytes = 0;
    bool start_sendind_ok = false;

    int pages = size / (RH_MESH_MAX_MESSAGE_LEN - 4); //TAMANHO MAXIMO DO FRAME POR MENSAGEM
    int actual_page = 0;
    if (size % (RH_MESH_MAX_MESSAGE_LEN - 4) > 0)
      // pages = pages + 1;

      Serial.printf("Preparing file for transfer, size: %d - n° of parts : %d, the max size is : %d\r\n", size, pages, RH_MESH_MAX_MESSAGE_LEN);

    data[0] = 0xA0;
    memcpy(&data[1], &filename, strlen(filename));

    memcpy(&data[2 + strlen(filename)], &size, sizeof(size));
    memcpy(&data[2 + strlen(filename) + sizeof(size)], &type_of_file, sizeof(type_of_file));

    Serial.print("Type of file to send: ");
    Serial.println(type_of_file, HEX);
    Serial.print("Size of send message : ");
    Serial.println((1 + strlen(filename) + sizeof(size) + sizeof(type_of_file)));
    Serial.print("Max size message : ");
    Serial.println(RH_MESH_MAX_MESSAGE_LEN - 4);

    //manager.waitPacketSent();
    if (manager.sendtoWait(data, (1 + strlen(filename) + sizeof(size) + sizeof(type_of_file)), destination))
    {
      Serial.println("MESSAGE NOT DELIVERED!");
      start_sendind_ok = false;
    }
    else
    {
      start_sendind_ok = true;
    }

    Serial.printf("Header sended %s , size %d \r\n", (char *)&data[1], strlen(filename));

    if (start_sendind_ok)
    {
      while (estationFile.available())
      {
        if (cancel_sending_file)
        {
          Serial.println("  ABORTING TRANSMISSION  ");
          break;
        }

        data[0] = 0xAA;
        int page_traking = actual_page;
        uint8_t low_page = page_traking & 0xff;
        uint8_t high_page = (page_traking >> 8);
        bool last_message = false;

        data[1] = low_page;
        data[2] = high_page;

        for (int i = 0; i < RH_MESH_MAX_MESSAGE_LEN - 8; i++)
        {

          if (total_bytes < size)
          {
            data[i + 3] = estationFile.read();
          }
          else
          {
            Serial.println("nothing more to read from file, getting out! ");
            Serial.print("Last part size: ");
            Serial.println(i);
            last_message = true;
            present_page_bytes = i;

            break;
          }
          total_bytes++;
          // vTaskDelay(1);
          // Serial.print("Part size: ");
          // Serial.println(i);
          // present_page_bytes = i;
        }
        DynamicJsonDocument jsonBuffer(1024);
        JsonObject config = jsonBuffer.to<JsonObject>();

        if (last_message)
        {
          start_time = millis();
          int response = manager.sendtoWait(data, present_page_bytes + 4, destination);
          if (response != 0)
          {
            Serial.println(response);
            Serial.println("File send ERROR - not delivered!");
            break;
          }
          end_time = millis();
        }
        else
        {
          start_time = millis();
          int response = manager.sendtoWait(data, sizeof(data), destination);
          if (response != 0)
          {
            Serial.println(response);
            Serial.println("File send ERROR - not delivered!");
            break;
          }
          end_time = millis();
        }

        timed_send = millis();
        vTaskDelay(1);
        // Serial.println("<action>Awensoring report\r\n");

        config["FileUploaded"] = String(filename);
        config["Status"] = mapDouble(actual_page, 0, pages + 1, 0, 100);
        config["KBPS"] = (RH_MESH_MAX_MESSAGE_LEN - 4) / ((float)(end_time - start_time) / 1000);
        config["SNR"] = rf95.lastSNR();
        config["RSSI"] = rf95.lastRssi();
        config["Total_Time_To_Transfer"] = (millis() - file_transfer_init_time) / 1000;

        serializeJson(config, msg, 10000);
        mqttClient.publish(pub_topics[6], 0, false, msg);
        // Serial.print("actual page: ");
        // Serial.println(actual_page);
        actual_page++;
        // Serial.println("checking the string generated: ");
        // Serial.println((char *)&data[3]);
        // Serial.println("");
      }

      if (cancel_sending_file)
      {
        Serial.println("File send aborted by user...");
        cancel_sending_file = false;
        estationFile.close();
      }
      else
      {
        Serial.println("File send done");
        estationFile.close();

        DynamicJsonDocument jsonBuffer(1024);
        JsonObject config = jsonBuffer.to<JsonObject>();
        // config["FileUploaded"] = String(filename);

        config["Total_Time_To_Transfer"] = (millis() - file_transfer_init_time) / 1000;
        config["Status"] = 0;
        config["KBPS"] = 0;
        config["SNR"] = 0;
        config["RSSI"] = 0;

        serializeJson(config, msg, 10000);
        mqttClient.publish(pub_topics[6], 0, false, msg);
      }
    }
    else
    {
      Serial.println("File send ERROR!");
    }
  }
}
void taskUpdate(void *pvParameters)
{

  // spiGetClockDiv()
  vTaskDelay(5000);
  download_file = true;
  //https://raw.githack.com/LucasFeliciano21/Gateway_Comando/master/build/ComandoGateway.ino.partitions.bin
  // downloadFile("http://raw.githubusercontent.com/LucasFeliciano21/Gateway_Comando/master/build/ComandoGateway.ino.bin", "/images/ComandoGateway.ino.bin", SD, &received_size, &actual_received_size);
  // downloadFile("http://mia.futurehosting.com/test.zip", "/images/ComandoGateway.ino.bin", SD, &received_size, &actual_received_size);
  downloadFile("187.108.112.23/testebw/file1.test", "test.zip", sd, &received_size, &actual_received_size);
  // // vTaskDelay(1000);
  download_file = false;
  // vTaskDelete(TaskHandle_1);
  // // vTaskDelete(TaskHandle_2);
  // vTaskDelete(TaskHandle_3);
  // vTaskDelete(TaskHandle_4);
  // vTaskDelete(TaskHandle_5);
  // // vTaskDelete(TaskHandle_6);
  // vTaskDelay(1000);
  // digitalWrite(2, LOW);
  // GW.updateFromFS(SD, "/images/ESP32_gateway_Async.ino.bin", false, update_status);

  while (1)
    vTaskDelay(10);
  if (upload_file)
  {
  }
}

// void ICACHE_RAM_ATTR downloadFile(char *host, char *file_name, fs::FS &fs, float *received_size, float *actual_received_size)
void downloadFile(char *host, char *file_name, SdFs &fs, float *received_size, float *actual_received_size)
{
  HTTPClient http;
  Serial.printf("Openning file to download \n");
  FsFile file_download;
  sd.remove(file_name);
  if (sd.exists(file_name))
  {
    Serial.printf("exists, excluding  \n");
    sd.remove(file_name);
    vTaskDelay(100);
  }

  Serial.printf("Opened \n");

  if (file_download.open(file_name, O_RDWR_SD | O_CREAT_SD))
  {

    while (http.begin("187.108.112.23", 80, "/testebw/file1.test") == 0)
    {
      vTaskDelay(500);
      Serial.printf("Retrying connection\n");
    }

    int httpCode = http.GET();
    int len = http.getSize();

    if (!file_download.preAllocate(len))
    {
      error("preAllocate failed");
    }
    *received_size = len;

    Serial.printf("--------------------------------------------downloading to file with size %d \n", len);

    if (httpCode > 0)
    {
      if (httpCode == HTTP_CODE_OK)
      {
        uint8_t buff[1024] = {0};

        WiFiClient *stream = http.getStreamPtr();
        while (http.connected() && (len > 0 || len == -1))
        {
          // get available data size
          size_t size = stream->available();
          // Serial.print("size available: ");
          // Serial.println(size);

          if (size)
          {
            // read up to 128 byte
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
            file_download.write(buff, c);

            if (len > 0)
            {
              len -= c;
              *actual_received_size += c;
            }
          }
          vTaskDelay(2);
          // asm volatile("nop");
          // asm volatile("nop");
          // asm volatile("nop");
          // asm volatile("nop");
        }
      }
    }
    else
    {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
    file_download.close();

    if (file_download.open(file_name, O_RDONLY))
    {
      int size = file_download.size();
      file_download.close();
      Serial.printf("--------------------------------------------Done Downloading, the file size is : %d\n", size);
    }
  }
  else
  {
    Serial.printf("File doesn't exist");
  }
}
void handle_radio(void *pvParameters)
{
  int i = 1;
  for (;;)
  {
    // checkNewDevices();
    // vTaskDelay(100);
    if (rf95._interrupt)
      rf95.check_incomming();
    if (!upload_file)
    {

      if (manager.available())
      {

        uint8_t mensagem_in[RH_MESH_MAX_MESSAGE_LEN];
        uint8_t mensagem_out[RH_MESH_MAX_MESSAGE_LEN];
        len = RH_MESH_MAX_MESSAGE_LEN;

        int waiting_time = 1000;

        if (manager.recvfromAck(mensagem_in, &len, &from, &to))
        {
          Serial.print("Nova mensagem - size:");
          Serial.print(len);
          Serial.print(" - de : ");
          Serial.println(from);
          if ((int)from >= 100 && (int)from < 150)
          {
            Serial.println("Message from other gateway, ignoring it...");
          }
          else
          {
            if (mensagem_in[CONNECTION_INDEX] == 0x04)
            {
              if (len < 1)
              {
                Serial.println("Size is to small...");
                break;
              }

              // lora_config_modem(LoraMode);

              DynamicJsonDocument estationbuffer(1024);
              JsonObject estationconfigjson = estationbuffer.to<JsonObject>();

              memcpy(&estationdata[from]->config, &mensagem_in[1], sizeof(estationdata[from]->config));

              estationconfigjson["name"] = estationdata[from]->config.name;
              estationconfigjson["time_of_registry"] = millis();
              estationconfigjson["battery_level"] = estationdata[from]->config.battery_volts;
              estationconfigjson["Latitude"] = estationdata[from]->config.gps_flat;
              estationconfigjson["Longitude"] = estationdata[from]->config.gps_flon;
              estationconfigjson["GPSfix"] = estationdata[from]->config.last_gps_fix;
              // estationconfigjson["Lora_pwr"] = estationdata[from]->config.radio_pwr;

              // estationconfigjson.prettyPrintTo(Serial);

              serializeJsonPretty(estationconfigjson, Serial);
              // serializeJson(root, msg, 512);

              Serial.printf("Received registration request from %d with name: %s\r\n -RSSI reported: %d \r\n",
                            from, estationdata[from]->config.name, estationdata[from]->config.last_received_RSSI);

              if (estationdata[from]->config.Card_present == true)
              {
                Serial.printf(" -SD Memory %d GB memory and %d GB free card space\r\n",
                              estationdata[from]->config.CardSize, estationdata[from]->config.CardFreeSpace);
                estationconfigjson["SDcardSize"] = estationdata[from]->config.CardSize;
              }

              Serial.printf(" -The station is LAT: %f LON: %f, at a hight of %f m, the last fix is %d hours ago\r\n",
                            estationdata[from]->config.gps_flat, estationdata[from]->config.gps_flon, estationdata[from]->config.gps_altitude, ((now() - estationdata[from]->config.last_gps_fix) / 3600));
              Serial.printf(" -The present battery voltage is %f V, the number of remote sensors is: %d \r\n",
                            estationdata[from]->config.battery_volts, estationdata[from]->config.Remote_sensors);

              serializeJson(estationconfigjson, msg, 10000);
              sprintf(pub_topics[4], "%s/%s/config", s_devname, estationdata[from]->config.name);

              mqttClient.publish(pub_topics[4], 2, false, msg);

              sprintf(sub_topics[4], "%s/%s/incommingConfig", s_devname, estationdata[from]->config.name);

              Serial.printf("Subscribe to: %s\r\n", sub_topics[4]);

              mensagem_out[0] = '\x05';
              Serial.println("Got connection request");

              estationdata[from]->config.read_interval = 60000;
              // memcpy(&mensagem_out[1], &GW.gateway_std_data, sizeof(GW.gateway_std_data));

              memcpy(&mensagem_out[1], &estationdata[from]->config, sizeof(Estation_configuration));
              memcpy(&mensagem_out[sizeof(Estation_configuration) + 1], &GW.gateway_std_data, sizeof(GW.gateway_std_data));

              //manager.waitPacketSent();

              Serial.printf("size of message: %d  \r\n", sizeof(Estation_configuration) + sizeof(GW.gateway_std_data));

              if (manager.sendtoWait(mensagem_out, sizeof(Estation_configuration) + sizeof(GW.gateway_std_data) + 1, from) == RH_ROUTER_ERROR_NONE)
              {
                Serial.println("Connection request responded!");
                // radio_busy = false;
                total_stations++;

                Serial.printf("Sended connection confirmation to %s with gateway Information...\r\n the read interval for this station is: %d \r\n",
                              estationdata[from]->config.name, estationdata[from]->config.read_interval);
              }
              else
              {
                Serial.println("message not delivered");
                // manager.sendtoWait(mensagem_out, sizeof(Estation) + 1, from);
              }
            }
            else if (mensagem_in[CONNECTION_INDEX] == 0x16)
            {
              if (from != 1)
                if (manager.sendtoWait((uint8_t *)"\x16", 1, from) == RH_ROUTER_ERROR_NONE)
                {
                  Serial.println("Position update request");
                }
            }
            else if (mensagem_in[CONNECTION_INDEX] == 0x06)
            {
              DynamicJsonDocument estationbuffer(1024);
              JsonObject estationconfigjson = estationbuffer.to<JsonObject>();

              Estation renew_position;

              memcpy(&renew_position.config, &mensagem_in[1], sizeof(renew_position.config));

              estationconfigjson["name"] = renew_position.config.name;
              estationconfigjson["time_of_registry"] = millis();
              estationconfigjson["battery_level"] = renew_position.config.battery_volts;
              estationconfigjson["Latitude"] = renew_position.config.gps_flat;
              estationconfigjson["Longitude"] = renew_position.config.gps_flon;
              estationconfigjson["GPSfix"] = renew_position.config.last_gps_fix;
              // estationconfigjson["Lora_pwr"] = renew_position.config.radio_pwr;

              // estationconfigjson.prettyPrintTo(Serial);

              Serial.printf(" - The station: %s is LAT: %f LON: %f, at a hight of %f m, the last fix is %d hours ago\r\n",
                            renew_position.config.name, renew_position.config.gps_flat, renew_position.config.gps_flon, renew_position.config.gps_altitude, ((now() - renew_position.config.last_gps_fix) / 3600));
            }
            else if (mensagem_in[CONNECTION_INDEX] == 0x06)
            {
              DynamicJsonDocument estationbuffer(1024);
              JsonObject estationconfigjson = estationbuffer.to<JsonObject>();

              Estation renew_position;

              memcpy(&renew_position.config, &mensagem_in[1], sizeof(renew_position.config));

              estationconfigjson["name"] = renew_position.config.name;
              estationconfigjson["time_of_registry"] = millis();
              estationconfigjson["battery_level"] = renew_position.config.battery_volts;
              estationconfigjson["Latitude"] = renew_position.config.gps_flat;
              estationconfigjson["Longitude"] = renew_position.config.gps_flon;
              estationconfigjson["GPSfix"] = renew_position.config.last_gps_fix;
              // estationconfigjson["Lora_pwr"] = renew_position.config.radio_pwr;

              // estationconfigjson.prettyPrintTo(Serial);

              Serial.printf(" - The station: %s is LAT: %f LON: %f, at a hight of %f m, the last fix is %d hours ago\r\n",
                            renew_position.config.name, renew_position.config.gps_flat, renew_position.config.gps_flon, renew_position.config.gps_altitude, ((now() - renew_position.config.last_gps_fix) / 3600));
            }
            else
            {
              Serial.println("Got Readed Data");
              // manager.sendtoWait((uint8_t *)"\x83", 1, from);

              estationdata[from]->lasttimeseen = millis();
              estationdata[from]->active = true;

              switch (mensagem_in[1])
              {
              case TUPPB:
              {
                DynamicJsonDocument jsonBuffer(1024);
                JsonObject root = jsonBuffer.to<JsonObject>();

                JsonObject estation = root.createNestedObject("Station");
                Serial.printf("--------------estation with temp, hum, pres, and rain--------\r\n");

                memcpy(&IncommingSmall, &mensagem_in[2], sizeof(IncommingSmall));

                estationdata[from]->lasttimeseen = millis();
                estationdata[from]->active = true;

                estationdata[from]->temperature = IncommingSmall.temperature;
                estationdata[from]->humidity = IncommingSmall.humidity;
                estationdata[from]->pressure = IncommingSmall.pressure;
                estationdata[from]->battery_volts = IncommingSmall.battery_volts;

                if (!isnan(estationdata[from]->config.gps_flat))
                  estation["latitude"] = estationdata[from]->config.gps_flat;
                if (!isnan(estationdata[from]->config.gps_flon))
                  estation["longitude"] = estationdata[from]->config.gps_flon;

                estation["local_address"] = from;
                estation["signal_strengh"] = rf95.lastRssi();
                estation["signal_to_noise_ratio"] = rf95.lastSNR();
                estation["SUID"] = estationdata[from]->config.name;

                estation["temperature"] = IncommingSmall.temperature;
                estation["humidity"] = IncommingSmall.humidity;
                estation["pressure"] = IncommingSmall.pressure;
                estation["battery_voltage"] = IncommingSmall.battery_volts;
                estation["rain"] = IncommingSmall.rain;

                estation["read_timestamp"] = IncommingSmall.timestamp;
                // estationjson["sizeofmassage"] = estationjson.size();

                serializeJsonPretty(root, Serial);
                serializeJson(root, msg, 512);

                sprintf(pub_topics[4], "%s/%s/sensor", s_devname, estationdata[from]->config.name);
                // Serial.printf("topic: %s data: %s\r\n", pub_topics[4], msg);

                mqttClient.publish(pub_topics[4], 2, false, msg);

                if (estationdata[from]->active == false || strlen(estationdata[from]->config.name) == 0)
                {
                  Serial.printf("Don't know the name of this device, lets check\r\n");
                  //manager.waitPacketSent();
                  manager.sendtoWait((uint8_t *)"\x11", 1, from);
                }
                break;
              }
              case TUPVDIRPBPGPS:
              {
                DynamicJsonDocument jsonBuffer(1024);
                JsonObject root = jsonBuffer.to<JsonObject>();

                JsonObject estation = root.createNestedObject("Station");

                Serial.printf("-----------------------estation ------------------------------\r\n");
                memcpy(&Incomming, &mensagem_in[2], sizeof(Incomming));
                memcpy(&incommingSensor, &mensagem_in[2 + sizeof(Incomming)], sizeof(remoteSensors));

                // estation["address"] = from;
                // estation["lora_rssi"] = rf95.lastRssi();
                // estation["name"] = estationdata[from]->config.name;

                estationdata[from]->lasttimeseen = millis();
                estationdata[from]->active = true;

                estationdata[from]->temperature = Incomming.temperature;
                estationdata[from]->humidity = Incomming.humidity;

                if (!isnan(Incomming.temperature))
                  estation["temperatura"] = Incomming.temperature;
                if (!isnan(Incomming.humidity))
                  estation["umidade"] = Incomming.humidity;
                if (!isnan(Incomming.pressure))
                  estation["pressao"] = Incomming.pressure;

                if (!isnan(estationdata[from]->config.gps_flat))
                  estation["latitude"] = estationdata[from]->config.gps_flat;
                if (!isnan(estationdata[from]->config.gps_flon))
                  estation["longitude"] = estationdata[from]->config.gps_flon;

                estationdata[from]->battery_volts = Incomming.battery_volts;

                // estationdata[from]->gps_flat = Incomming.gps_flat;
                // estationdata[from]->gps_flon = Incomming.gps_flon;
                // estationdata[from]->gps_altitude = Incomming.gps_altitude;

                estation["local_address"] = from;
                estation["signal_strengh"] = rf95.lastRssi();
                estation["signal_noise_ratio"] = rf95.lastSNR();
                estation["SUID"] = estationdata[from]->config.name;
                // estation["latitude"] = estationdata[from]->config.gps_flat;
                // estation["longitude"] = estationdata[from]->config.gps_flon;
                // estation["altitude"] = renew_position.gps_altitude;
                // estation["temperatura"] = Incomming.temperature;
                // estation["umidade"] = Incomming.humidity;
                // estation["pressao"] = Incomming.pressure;
                estation["tensao_bateria"] = Incomming.battery_volts;
                estation["windspeed"] = Incomming.windspeed;
                estation["irradiation_in"] = Incomming.irradiation_in;
                estation["winddir"] = Incomming.winddir;
                estation["rain"] = Incomming.rain;
                estation["timestamp"] = Incomming.timestamp;
                estation["distance_from"] = gps.distanceBetween(estationdata[from]->config.gps_flat, estationdata[from]->config.gps_flon, GW.gateway_std_data.stored_lat, GW.gateway_std_data.stored_lon);

                serializeJsonPretty(root, Serial);
                serializeJson(root, msg, 512);

                DynamicJsonDocument estationbuffer(1024);
                JsonObject stationstorage = estationbuffer.to<JsonObject>();

                sprintf(pub_topics[4], "%s/%s/sensor", s_devname, estationdata[from]->config.name);

                mqttClient.publish(pub_topics[4], 2, false, msg);
                // Serial.printf("topic: %s data: %s\r\n", pub_topics[4], msg);

                if (estationdata[from]->active == false || strlen(estationdata[from]->config.name) == 0)
                {
                  Serial.printf("Don't know the name of this device, lets check\r\n");
                  //manager.waitPacketSent();
                  manager.sendtoWait((uint8_t *)"\x11", 1, from);
                }

                break;
              }
              case TUPVDIRPBPGPS + 1:
              {
                DynamicJsonDocument jsonBuffer(1024);
                JsonObject root = jsonBuffer.to<JsonObject>();

                JsonObject estation = root.createNestedObject("estation");

                JsonObject remotesensors = root.createNestedObject("remoteSensor");

                Serial.printf("-----------------------estation +1 remote sensor------------------------------\r\n");
                memcpy(&Incomming, &mensagem_in[2], sizeof(Incomming));
                memcpy(&incommingSensor, &mensagem_in[2 + sizeof(Incomming)], sizeof(remoteSensors));

                remotesensors["address"] = incommingSensor.node;
                remotesensors["motherStation"] = from;
                remotesensors["soil_m10"] = incommingSensor.soil_m10 - 30.74;
                remotesensors["soil_m20"] = incommingSensor.soil_m20 - 36.75;
                remotesensors["soil_m30"] = incommingSensor.soil_m30 - 32.15;
                remotesensors["soil_m40"] = incommingSensor.soil_m40 - 65.82;
                remotesensors["soil_m50"] = incommingSensor.soil_m50 - 76.66;
                remotesensors["soil_moisture"] = incommingSensor.soil_moisture;
                remotesensors["bat_level"] = incommingSensor.batlevel;
                remotesensors["timestamp"] = incommingSensor.lasttimeseen;

                estation["address"] = from;
                estation["lora_rssi"] = rf95.lastRssi();
                estation["name"] = estationdata[from]->config.name;
                // parseUnion(incomingdata, mensagem_in[INCOMING_SIZE], from, mensagem_in);

                estationdata[from]->lasttimeseen = millis();
                estationdata[from]->active = true;

                estationdata[from]->temperature = Incomming.temperature;
                estationdata[from]->humidity = Incomming.humidity;
                estationdata[from]->pressure = Incomming.pressure;
                estationdata[from]->battery_volts = Incomming.battery_volts;
                //estationdata[from]->gps_flat = Incomming.gps_flat;
                //estationdata[from]->gps_flon = Incomming.gps_flon;
                //estationdata[from]->gps_altitude = Incomming.gps_altitude;

                estation["temperatura"] = Incomming.temperature;
                estation["umidade"] = Incomming.humidity;
                estation["pressao"] = Incomming.pressure;
                estation["tensao_bateria"] = Incomming.battery_volts;
                estation["windspeed"] = Incomming.windspeed;
                estation["irradiation_in"] = Incomming.irradiation_in;
                estation["winddir"] = Incomming.winddir;
                estation["rain"] = Incomming.rain;
                estation["timestamp"] = Incomming.timestamp;

                serializeJsonPretty(root, Serial);
                serializeJson(root, msg, 512);

                sprintf(pub_topics[4], "%s/%s/sensor", s_devname, estationdata[from]->config.name);

                if (estationdata[from]->active == false || strlen(estationdata[from]->config.name) == 0)
                {
                  Serial.printf("Don't know the name of this device, lets check\r\n");
                  //manager.waitPacketSent();
                  manager.sendtoWait((uint8_t *)"\x11", 1, from);
                  break;
                }

                if (mqtt_client.publish(pub_topics[4], msg))
                {
                  Serial.println("published ok!");
                }
                else
                {
                  Serial.println("message not delivered!");

                  // reconnect();
                  mqtt_client.publish(pub_topics[4], msg);
                }

                break;
              }
              }
            }
          }
        }
        else
        {
          // Serial.println("------------------------Time out---------------------");
        }
      }
      // else
      // {
      //   Serial.println("------------------------No message available---------------------");
      // }
      if (ActualLoraMode == 1)
      {

        // if (timeout_for_connection < millis() - timed_send)
        // {
        //   lora_config_modem(LoraMode);
        //   Serial.printf("rolling back to mode: %d\r\n", LoraMode);
        // }
      }
    }
    vTaskDelay(10);
  }
}

void send_or_store_mqtt_message(char *payload, char *topic, char *station, uint8_t from)
{

  // if (!mqtt_connected)
  // {
  //   if (strlen(estationdata[from]->config.name) > 0)
  //   {

  //     char filename[64];
  //     estationdata[from]->storing_offline_data = true;

  //     sprintf(filename, "/offline/SD-%.2d%.2d%.2d-%s.json",
  //             day(now()), month(now()), (year(now()) - 2000), estationdata[from]->config.name);

  //     strcpy(estationdata[from]->filename, filename);
  //     String FileName = String(filename);
  //     Serial.printf("filename to save - '%s'\r\n", FileName.c_str());

  //     File estationFile = SD.open(FileName.c_str(), FILE_APPEND);

  //     if (estationFile)
  //     {

  //       if (estationFile.size() <= 0)
  //       {
  //         estationFile.print("[");
  //       }
  //       else
  //       {
  //         estationFile.print(",");
  //       }

  //       estation.printTo(estationFile);
  //       estationFile.close();
  //     }
  //     else
  //     {
  //       Serial.printf("ERROR ON FILE-  '%s', creating file!\r\n", FileName.c_str());
  //       File estationFile = SPIFFS.open(FileName.c_str(), FILE_WRITE);
  //       estationFile.close();
  //     }
  //   }
  // }
  // else
  // {
  //   if (estationdata[from]->storing_offline_data == true)
  //   {
  //     estationdata[from]->storing_offline_data = false;
  //     File estationFile = SD.open(estationdata[from]->filename, FILE_APPEND);
  //     if (estationFile)
  //     {
  //       estationFile.print("]");
  //       estationFile.close();
  //     }
  //     else
  //     {
  //       Serial.printf("ERROR FINISHING THE FILE-  '%s,FILE DOESN'T EXIST\r\n", FileName.c_str());
  //     }
  //   }
  // }

  // if (!mqtt_connected)
  // {
  //   Serial.printf("MQTT is offline! Damn :( \r\n");
  //   if (strlen(estationdata[from]->config.name) > 0)
  //   {
  //   }
  //   else
  //   {
  //     Serial.printf("MQTT is offline and also we don't know the name of this station \r\n");
  //   }
  // }dd
  // else
  // {
  //   Serial.printf("Publishing on MQTT\r\n");
  //   mqttClient.publish(topic, 2, false, payload);
  // }
}
bool checkNewDevices()
{

  if (LoraConnected && !upload_file)
  {
    // Serial.println("sending Solicitation ------------------------------");
    //manager.waitPacketSent();

    char message[2] = {'\x03', LoraMode};
    // lora_config_modem(1);
    timed_send = millis();
    Serial.println("------------------------Sending Solicitation---------------------");

    if (manager.sendtoWait((uint8_t *)message, 2, 255) == RH_ROUTER_ERROR_NONE)
    {
      Serial.println("------------------------Solicitation sent ---------------------");
      timed_send = millis();
    }
    else
    {
      Serial.println("Solicitation not delivered...");
      setupLora();
    }
  }
}
bool checkDeviceConfig(uint8_t address)
{

  stations_to_check[address].valid = true;
  has_stations_to_check = true;
  return true;
}
int renewConnection()
{
  int count, connected_stations = 0;
  for (count = 0; count < max_stations; count++)
  {
    if (millis() - estationdata[count]->lasttimeseen > estationdata[count]->config.read_interval)
    {
      estationdata[count]->active = false;
    }
    if (estationdata[count]->active == true)
    {
      connected_stations++;
    }
  }
  return connected_stations;
}
void setupLora()
{
#ifdef DEBUG
  digitalWrite(4, LOW);
  delay(1000);
  digitalWrite(4, HIGH);
  delay(1000);
#else
  digitalWrite(2, LOW);
  delay(1000);
  digitalWrite(2, HIGH);
  delay(1000);
#endif

  if (!manager.init())
  {
    Serial.println("init failed");
    while (1)
    {
      Serial.println("LoRa offline, restarting Lora");
      vTaskDelay(500);
      setupLora();
    }
  }
  else
  {

    timed_send = millis();
    rf95.setTxPower(23, false);
    lora_config_modem(2);
    rf95.setFrequency(433);
    rf95.setCADTimeout(1000);
    // rf95.setSignalBandwidth(500000);
    // rf95.setSpreadingFactor(7);
    // rf95.setCodingRate4(5);

    // rf95.setPreambleLength(16);

    // manager.setRetries(6);
    // return;
    // rf95.setCADTimeout(2000);
    Serial.println("LoRa up and running");
    // manager.sendtoWait((uint8_t *)"\x03", 1, 255);

    LoraConnected = true;
  }
}
void automaticStationDataRate(uint8_t device)
{
  int modes = 1;
  uint8_t test_message[3];
  while (modes < 4)
  {
    lora_config_modem(modes);

    modes++;
    char message[2] = {'\x15', modes};

    if (manager.sendtoWait((uint8_t *)message, 2, device))
    {
      if (manager.sendtoWait((uint8_t *)"\x90teste", sizeof("\x90teste"), device))
      {
        Serial.print("Message delivered on mode ? ");
        Serial.println(modes);
      }
    }
  }
}
void lora_config_modem(int x)
{

  Serial.print("Changing radio mode to: ");
  Serial.println(x);

  ActualLoraMode = x;
  switch (x)
  {
  case 1:
  {
    rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
    //Serial1.println("cfg 1");
    // Station.station_std_data.lora_connection_mode = 1;
    // Station.saveConfig();
    break;
  }
  case 2:
  {
    rf95.setModemConfig(RH_RF95::Bw500Cr45Sf128);
    manager.setTimeout(100);
    rf95.setPreambleLength(16);
    //Serial1.println("cfg 2");
    // Station.station_std_data.lora_connection_mode = 2;
    // Station.saveConfig();
    break;
  }
  case 3:
  {
    rf95.setModemConfig(RH_RF95::Bw31_25Cr48Sf512);
    manager.setTimeout(1000);
    rf95.setPreambleLength(16);
    //Serial1.println("cfg 3");
    // Station.station_std_data.lora_connection_mode = 3;
    // Station.saveConfig();
    break;
  }
  case 4:
  {
    rf95.setModemConfig(RH_RF95::Bw125Cr48Sf4096);
    //Serial1.println("cfg 4");
    // Station.station_std_data.lora_connection_mode = 4;
    // Station.saveConfig();
    break;
  }
  default:
  {
    rf95.setModemConfig(RH_RF95::Bw31_25Cr48Sf512);
    break;
  }
  }
}
// void loadHistory()
// {
//   File file = SPIFFS.open(HISTORY_FILE, "r");
//   if (!file)
//   {
//     Serial.println("Aucun historique existe - No History Exist");
//   }
//   else
//   {
//     size_t size = file.size();
//     if (size == 0)
//     {
//       Serial.println("Fichier historique vide - History file empty !");
//     }
//     else
//     {
//       std::unique_ptr<char[]> buf(new char[size]);
//       file.readBytes(buf.get(), size);
//       JsonObject root = jsonBuffer.parseObject(buf.get());
//       if (!root.success())
//       {
//         Serial.println("Impossible de lire le JSON - Impossible to read JSON file");
//       }
//       else
//       {
//         Serial.println("Historique charge - History loaded");
//         root.printTo(Serial);
//       }
//     }
//     file.close();
//   }
// }

void create_wifi_file()
{
  // FsFile temp_file;
  Serial.println("Creating file:");

  // if (temp_file.open(wifi_config_file, O_RDWR_SD | O_CREAT_SD | O_TRUNC_SD))
  File file = SPIFFS.open(wifi_config_file, FILE_WRITE);
  if (file)
  {
    DynamicJsonDocument jsonBuffer(1024);
    JsonObject root = jsonBuffer.to<JsonObject>();
    JsonArray SSID = root.createNestedArray("SSID");
    JsonArray PSK = root.createNestedArray("PSK");

    SSID.add("abc123");
    PSK.add("abcd1234");

    serializeJsonPretty(root, Serial);
    serializeJson(root, file);

    Serial.print("Check the created size:");
    Serial.println((int)file.size());

    file.close();
  }
  else
  {
    Serial.print("Error criating file!");
    // break;
  }
}

bool savewificonfig()
{

  if (!SPIFFS.exists(wifi_config_file))
  {
    create_wifi_file();
  }
  else
    Serial.println("File exists...");

  DynamicJsonDocument jsonBuffer(1024);

  WiFi.disconnect();
  int n = WiFi.scanNetworks(false, false, false, 500);
  Serial.print("founded network:");
  Serial.println(n);
  // File wifi_file = SD.open(wifi_config_file, FILE_WRITE);
  // wifi_file.close();
  bool known_network = true;

  // FsFile filerd;
  // filerd.open(wifi_config_file, O_RDONLY_SD);

  File file = SPIFFS.open(wifi_config_file);

  int size = file.size();

  if (size == 0)
  {
    file.close();

    Serial.printf("No %s file, creating file and waiting...\r\n", wifi_config_file);

    // FsFile wifi_file; if (wifi_file.open(wifi_config_file, O_RDWR_SD | O_CREAT_SD | O_TRUNC_SD))
    File file = SPIFFS.open(wifi_config_file, FILE_WRITE);

    if (file)
    {
      Serial.println("Saving first config on the file!");
      DynamicJsonDocument jsonBuffer(1024);
      JsonObject root = jsonBuffer.to<JsonObject>();
      JsonArray SSID = root.createNestedArray("SSID");
      JsonArray PSK = root.createNestedArray("PSK");

      SSID.add("abc123");
      PSK.add("abcd1234");

      serializeJsonPretty(root, Serial);
      serializeJson(root, file);

      Serial.print("Check if the file has the size:");
      Serial.println((int)file.size());

      file.close();

      savewificonfig();
    }
    file.close();
  }
  else
  {
    Serial.println("Wifi file exists, lets go...");
    int size = file.size();

    // Serial.print("checking size before deserializa: ");
    // Serial.println(size);

    if (size == 0)
    {
      file.close();
      Serial.println("History file empty ! Writing the first data");

      // if (!filerd.open(wifi_config_file, O_RDWR_SD | O_CREAT_SD | O_TRUNC_SD))
      File file = SPIFFS.open(wifi_config_file, FILE_WRITE);
      if (file)
      {
        DynamicJsonDocument jsonBuffer(1024);
        JsonObject root = jsonBuffer.to<JsonObject>();

        JsonArray SSID = root.createNestedArray("SSID");
        JsonArray PSK = root.createNestedArray("PSK");

        //JsonArray &SSID = root["SSID"];
        //JsonArray &PSK = root["PSK"];
        SSID.add("abc123");
        SSID.add("Casa 2.4");

        PSK.add("teste");
        PSK.add("keinlexy");

        serializeJsonPretty(root, Serial);
        serializeJson(root, file);

        Serial.print("checking size: ");
        Serial.println((int)file.size());

        file.close();
        savewificonfig();
      }
      else
      {
        Serial.println("Impossible to put the first data...");
      }
    }
    else
    {
      // Serial.print("checking size before deserializa: ");
      // Serial.println((int)filerd.size());

      // FsFile file_wifi;
      // file_wifi.open(wifi_config_file, O_RDONLY_SD);

      File file = SPIFFS.open(wifi_config_file);

      DynamicJsonDocument root(4096);

      DeserializationError error = deserializeJson(root, file);

      if (error)
      {

        Serial.println(F("Failed to read file, using default configuration"));
        file.close();
        SPIFFS.remove(wifi_config_file);
        savewificonfig();
      }
      else
      {
        // Serial.println("Redes conhecidas...");

        int arraySize = root["SSID"].size();

        for (int i = 0; i < arraySize; i++)
        {
          const char *ccSSID = root["SSID"][i];
          const char *ccPSK = root["PSK"][i];

          String SSID = root["SSID"][i];
          String PSK = root["PSK"][i];

          // Serial.print("Rede: ");
          // Serial.print(SSID);
          // Serial.print(" -- Passwd: ");
          // Serial.println(PSK);
          int status;
          for (int i = 0; i < n; ++i)
          {
            // Print SSID and RSSI for each network found
            // Serial.print(i + 1);
            // Serial.print(": ");
            // Serial.print(WiFi.SSID(i));
            // Serial.print(" (");
            // Serial.print(WiFi.RSSI(i));
            // Serial.println(")");
            if (SSID == WiFi.SSID(i))
            {
              int count = 0;
              Serial.printf("Connecting to known network: %s with %s psk\r\n", ccSSID, ccPSK);
              // WiFi.enableSTA(true);
              // esp_wifi_start();
              // esp_wifi_connect();
              WiFi.begin(ccSSID, ccPSK);
              while (WiFi.status() != WL_CONNECTED)
              {

                timerWrite(timer, 0);
                // WiFi.setHostname("gateway");
                // WiFi.mode(WIFI_STA);
                // WiFi.reconnect();

                // WiFi.setSleep(false); // <--- this command disables WiFi energy save mode and eliminate connected():

                vTaskDelay(10);
                Serial.print(".");
                if (count > 60000)
                {
                  Serial.printf("Unable to connect, restarting...");

                  ESP.restart();
                }
                count++;
              }
              if (WiFi.status() == WL_CONNECTED)
              {
                Serial.println("Connected to wifi on savewificonfig...");
                // status_LED = WIFI_CONNECTED;
                return true;
              }
              else
              {
                setupWiFi();
              }
            }
            else
            {
              // setupWiFi();

              known_network = false;
            }
          }
        }
      }
    }
  }

  file.close();

  setupWiFi();
}
void setupWiFi()
{
  Serial.println("setupWiFi: init...");
  DynamicJsonDocument jsonBuffer(1024);

  if (WiFi.status() != WL_CONNECTED)
  {

    WiFi.mode(WIFI_AP_STA);

    status_LED = WIFI_WAITING_PASSWD;

    WiFi.beginSmartConfig();
    // xTimerStop(wifiReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi

    // if (!sd.exists(wifi_config_file))
    // {
    //   Serial.println("setupWiFi: no wificonfig2.json doesn't exist");
    // }
    // else
    // {

    // FsFile filessid;
    // if (!filessid.open(wifi_config_file, O_RDONLY_SD))
    File file = SPIFFS.open(wifi_config_file);
    if (!file)
    {
      Serial.println("Error openning file on setupWiFi");

      DynamicJsonDocument root(1024);

      DeserializationError error = deserializeJson(root, file);

      if (error)
      {
        Serial.println(F("Failed to read file, using default configuration - SetupWIFI"));
      }

      while (!WiFi.smartConfigDone())
      {
        delay(500);
        Serial.print(".");
      }
      while (WiFi.status() != WL_CONNECTED)
      {
        vTaskDelay(500);
        Serial.print(".");
        Status = WIFI_CONNECTED;
      }

      if (!SPIFFS.exists(wifi_config_file))
      {
        Serial.println("No History Exist");
      }
      else
      {
        create_wifi_file();
        setupWiFi();
      }
    }
    else
    {
      DynamicJsonDocument root(1024);

      DeserializationError error = deserializeJson(root, file);
      file.close();

      if (error)
      {
        // Serial.print("size: ");
        // Serial.print(size);
        Serial.println(" Impossible to read JSON file - setupWifi");
      }
      else
      {
        Serial.println(F("Ok, waiting for smartConfigDone to save the SSID do file"));
        int timeout_counter = 0;
        while (!WiFi.smartConfigDone())
        {
          timerWrite(timer, 0);
          delay(1000);
          timeout_counter++;
          if (timeout_counter > 60)
          {
            WiFi.stopSmartConfig();
            Serial.println("Timeout do smartconfig, sem conexão, tentando novamente com as redes conhecidas");
            // WiFi.mode(WIFI_MODE_STA);
            savewificonfig();
            return;
          }
          else
          {
            delay(500);
            Serial.println("waiting...");
          }
        }
        // xTimerStart(wifiReconnectTimer, 0);
        //

        int arraySize = root["SSID"].size();
        File file = SPIFFS.open(wifi_config_file, FILE_WRITE);
        if (!file)
        {
          Serial.println("No History Exist");
        }
        else
        {
          JsonArray SSID = root["SSID"];
          SSID.add(WiFi.SSID());
          JsonArray PSK = root["PSK"];
          PSK.add(WiFi.psk());

          serializeJsonPretty(root, Serial);
          serializeJson(root, file);
        }

        for (int i = 0; i < arraySize; i++)
        {

          const char *ccSSID = root["SSID"][i];
          const char *ccPSK = root["PSK"][i];

          String SSID = root["SSID"][i];
          String PSK = root["PSK"][i];

          Serial.print("Rede: ");
          Serial.print(ccSSID);
          Serial.print(" -- Passwd: ");
          Serial.println(ccPSK);
        }
        file.close();
      }
    }
    file.close();
  }
  else
  {
    // reconnect();
  }
}
String GetExternalIP()
{
  WiFiClient client;
  if (!client.connect("api.ipify.org", 80))
  {
    // Serial.println("Failed to connect with 'api.ipify.org' !");
  }
  else
  {
    int timeout = millis() + 5000;
    client.print("GET /?format=json HTTP/1.1\r\nHost: api.ipify.org\r\n\r\n");
    while (client.available() == 0)
    {
      if (timeout - millis() < 0)
      {
        // Serial.println(">>> Client Timeout !");
        client.stop();
        return String("error");
      }
    }
    int size;
    while ((size = client.available()) > 0)
    {
      uint8_t *msg = (uint8_t *)malloc(size);
      size = client.read(msg, size);

      String externalIPrequest = String((char *)msg);
      int init_index = externalIPrequest.lastIndexOf("\"ip\":\"");
      int last_index = externalIPrequest.lastIndexOf("\"}");
      int external_ip_size = externalIPrequest.substring(init_index + 6, last_index).length();
      // Serial.print("response :");
      // Serial.println(externalIPrequest);
      Serial.print("parsed :");
      Serial.println(externalIPrequest.substring(init_index + 6, last_index));
      Serial.print("size :");
      Serial.println(external_ip_size);
      // terminal.flush();
      // terminal.write(msg, size);
      // terminal.flush();

      if (external_ip_size > 15 || external_ip_size < 7)
        return String("error");
      else
        return externalIPrequest.substring(init_index + 6, last_index);
    }
  }
}

String getHeaderValue(String header, String headerName)
{
  return header.substring(strlen(headerName.c_str()));
}
String getMacAddress()
{
  uint8_t baseMac[6];
  // Get MAC address for WiFi station
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  char baseMacChr[20] = {0};
  sprintf(baseMacChr, "%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  return String(baseMacChr);
}
// int Publish_AWS(char *topic, char *payload, int retry_time, int times_to_retry)
// {
//   bool status = false;
//   int retry_times = 0;
//   int in_time = millis();

//   while (status == false)
//   {
//     if (awsClient.publish(topic, payload) == 0)
//     {
//       Serial.print("Publish Message:");
//       Serial.println(payload);
//       status = true;
//       return 0x01;
//     }
//     else
//     {
//       while (retry_times < times_to_retry)
//       {
//         if (in_time + retry_time > millis())
//         {

//           if (awsClient.publish(topic, payload) == 0)
//           {
//             Serial.print("Publish Message:");
//             Serial.println(payload);
//             status = true;
//             return 0x01;
//           }
//           else
//           {
//             in_time = millis();
//             retry_times++;
//             Serial.println("No success on retry...");
//             status = true;
//             return 0x00;
//           }
//         }
//       }
//     }
//   }
// }
bool file = false;
FsFile filesd;
void saveConfig()
{

  Serial.println("Saving Config");
  // FsFile configfile;
  File file = SPIFFS.open("/config.json", FILE_WRITE);

  // if (configfile.open("configuration/config.json", O_RDWR_SD | O_CREAT_SD | O_TRUNC_SD))
  if (file)
  {
    StaticJsonDocument<500> config;
    // JsonObject config = jsonBuffer.to<JsonObject>();

    config["flat"] = GW.gateway_std_data.stored_lat;
    config["flon"] = GW.gateway_std_data.stored_lon;
    config["altitude"] = GW.gateway_std_data.stored_alt;

    config["lastfix"] = now();

    config["connected_stations"] = connected_stations;
    config["heap"] = ESP.getFreeHeap();
    config["Wifi_SSID"] = WiFi.SSID();
    config["Wifi_RSSI"] = WiFi.RSSI();
    config["Time_Connected"] = millis() / 1000;
    config["server_address"] = "34.228.111.174";
    config["calibration"] = Battery_Calibration;

    serializeJson(config, file);
    // serializeJsonPretty(config, Serial);

    // config.prettyPrintTo(Serial);
    // Serial.println("New position saved on file");
  }
  else
  {
    Serial.println("unable to open file with SDFAT");
  }
  file.close();
}
void loadConfig()
{
  Serial.println("Loading config...");
  File file = SPIFFS.open("/config.json");
  // if (configfile.open("configuration/config.json", O_RDONLY_SD))
  if (file)
  {
    // size_t size = configfile.size();
    // std::unique_ptr<char[]> buf(new char[size]);
    // configfile.readBytes(buf.get(), size);

    Serial.println("Open...");

    DynamicJsonDocument jsonBuffer(1024);

    DeserializationError error = deserializeJson(jsonBuffer, file);
    if (error)
    {
      Serial.println(F("Failed to read file, using default configuration"));

      SPIFFS.remove("config.json");
      file.close();
      saveConfig();
      loadConfig();
    }
    else
    {

      GW.gateway_std_data.stored_lat = jsonBuffer["flat"];
      GW.gateway_std_data.stored_lon = jsonBuffer["flon"];
      GW.gateway_std_data.stored_alt = jsonBuffer["altitude"];
      strcpy(mqtt_server, (const char *)jsonBuffer["server_address"]);

      Battery_Calibration = jsonBuffer["calibration"];
      //mqtt_server = ;
      // config["lastfix"] = timeClient.getEpochTime();

      // config["connected_stations"] = connected_stations;
      // config["heap"] = ESP.getFreeHeap();
      // config["Wifi_SSID"] = WiFi.SSID();
      // config["Wifi_RSSI"] = WiFi.RSSI();
      // config["Time_Connected"] = millis() / 1000;

      // Serial.print("data from config file: ");

      // serializeJsonPretty(jsonBuffer, Serial);
      // serializeJson(jsonBuffer, file);
      serializeJsonPretty(jsonBuffer, Serial);
      // config.prettyPrintTo(Serial);
    }
  }
  else
  {
    Serial.println("unable to open file");
  }
  file.close();
}

void adjustInternalTime()
{
  // unsigned long age;
  // int Year;
  // byte Month, Day, Hour, Minute, Second;
  // gps.crack_datetime(&Year, &Month, &Day, &Hour, &Minute, &Second, NULL, &age);

  if (gps.date.isValid())
  {
    // set the Time to the latest GPS reading
    setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
    adjustTime(offset * 3600);
    // if (tt != now())
    // {
    //   rt.setTime(now());
    // }
  }
}
double mapDouble(double x, double in_min, double in_max, double out_min, double out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
void createDir(fs::FS &fs, const char *path)
{
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path))
  {
    Serial.println("Dir created");
  }
  else
  {
    Serial.println("mkdir failed");
  }
}
// void downloadFile(char *host, char *file_name)
// {
//   HTTPClient http;
//   File f = SD.open(file_name, FILE_WRITE);

//   if (f)
//   {
//     http.begin(host);
//     int httpCode = http.GET();
//     int len = http.getSize();
//     received_size = len;

//     Serial.printf("--------------------------------------------downloading to file with size %d \n", len);

//     if (httpCode > 0)
//     {
//       if (httpCode == HTTP_CODE_OK)
//       {
//         uint8_t buff[4096] = {0};

//         WiFiClient *stream = http.getStreamPtr();
//         while (http.connected() && (len > 0 || len == -1))
//         {
//           // get available data size
//           size_t size = stream->available();
//           // Serial.print("size available: ");
//           // Serial.println(size);

//           if (size)
//           {
//             // read up to 128 byte
//             int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
//             f.write(buff, c);

//             if (len > 0)
//             {
//               len -= c;
//               actual_received_size += c;
//             }
//           }
//           vTaskDelay(1);
//         }
//       }
//     }
//     else
//     {
//       Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
//     }
//     http.end();
//     f.close();

//     File f = SD.open(file_name, O_RDONLY);
//     int size = f.size();
//     f.close();
//     Serial.printf("--------------------------------------------Done Downloading, the file size is : %d\n", size);
//   }
//   else
//   {
//     Serial.printf("File doesn't exist");
//   }
// }

bool isFileNameValid(const char *fileName)
{
  char c;
  while ((c = *fileName++))
    if (!isgraph(c))
      return false;
  return true;
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{

  Serial.printf("Listing directory: %s\n", dirname);
  folder_name = String(dirname);
  inside_folder = true;

  File root = fs.open(dirname);
  if (!root)
  {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();

  JsonObject dirjson = folderlistbuffer.to<JsonObject>();
  // folder_name = String(file.name());
  dirjson["name"] = folder_name;
  dirjson["type"] = "folder";
  dirjson["size"] = (float)file.size() / 1000;

  JsonArray content = dirjson.createNestedArray("content");

  while (file)
  {
    if (file.isDirectory())
    {
      // Serial.print("  DIR : ");
      // Serial.println(file.name());
      if (levels)
      {
        listDir(fs, file.name(), levels - 1);
      }
    }
    else
    {
      if (!isFileNameValid(file.name()))
      {
        fs.remove(file.name());
      }
      else
      {
        file_name = String(file.name());
        int index_of_foldername = file_name.indexOf(folder_name);
        int folder_name_size = folder_name.length();

        file_name.remove(index_of_foldername, folder_name_size);

        JsonObject filejson = folderlistbuffer.to<JsonObject>();

        filejson["name"] = file_name;
        filejson["type"] = "file";
        filejson["size"] = (float)file.size() / 1000;

        content.add(filejson);

        Serial.printf(" file inside a folder name: %s \r\n", file_name.c_str());
      }
    }

    file = root.openNextFile();
  }

  jarray.add(dirjson);
  Serial.println("Out a folder --------------------------------------------------------------------------------");
  Serial.print("  Exiting DIR : ");
  Serial.println(folder_name);
  folder_name = String("");
}

//------------------------------------------------------------------------------
// bool cidDmp()
// {
//   cout << F("\nManufacturer ID: ");
//   cout << uppercase << showbase << hex << int(m_cid.mid) << dec << endl;
//   cout << F("OEM ID: ") << m_cid.oid[0] << m_cid.oid[1] << endl;
//   cout << F("Product: ");
//   for (uint8_t i = 0; i < 5; i++)
//   {
//     cout << m_cid.pnm[i];
//   }
//   cout << F("\nVersion: ");
//   cout << int(m_cid.prv_n) << '.' << int(m_cid.prv_m) << endl;
//   cout << F("Serial number: ") << hex << m_cid.psn << dec << endl;
//   cout << F("Manufacturing date: ");
//   cout << int(m_cid.mdt_month) << '/';
//   cout << (2000 + m_cid.mdt_year_low + 10 * m_cid.mdt_year_high) << endl;
//   cout << endl;
//   return true;
// }
// //------------------------------------------------------------------------------
// bool csdDmp()
// {
//   bool eraseSingleBlock;
//   if (m_csd.v1.csd_ver == 0)
//   {
//     eraseSingleBlock = m_csd.v1.erase_blk_en;
//     m_eraseSize = (m_csd.v1.sector_size_high << 1) | m_csd.v1.sector_size_low;
//   }
//   else if (m_csd.v2.csd_ver == 1)
//   {
//     eraseSingleBlock = m_csd.v2.erase_blk_en;
//     m_eraseSize = (m_csd.v2.sector_size_high << 1) | m_csd.v2.sector_size_low;
//   }
//   else
//   {
//     cout << F("m_csd version error\n");
//     return false;
//   }
//   m_eraseSize++;
//   cout << F("cardSize: ") << 0.000512 * sdCardCapacity(&m_csd);
//   cout << F(" MB (MB = 1,000,000 bytes)\n");

//   cout << F("flashEraseSize: ") << int(m_eraseSize) << F(" blocks\n");
//   cout << F("eraseSingleBlock: ");
//   if (eraseSingleBlock)
//   {
//     cout << F("true\n");
//   }
//   else
//   {
//     cout << F("false\n");
//   }
//   return true;
// }
// //------------------------------------------------------------------------------
// void errorPrint()
// {
//   if (SD.sdErrorCode())
//   {
//     cout << F("SD errorCode: ") << hex << showbase;
//     printSdErrorSymbol(&Serial, SD.sdErrorCode());
//     cout << F(" = ") << int(SD.sdErrorCode()) << endl;
//     cout << F("SD errorData = ") << int(SD.sdErrorData()) << endl;
//   }
// }
// //------------------------------------------------------------------------------
// bool mbrDmp()
// {
//   MbrSector_t mbr;
//   bool valid = true;
//   if (!SD.card()->readSector(0, (uint8_t *)&mbr))
//   {
//     cout << F("\nread MBR failed.\n");
//     errorPrint();
//     return false;
//   }
//   cout << F("\nSD Partition Table\n");
//   cout << F("part,boot,bgnCHS[3],type,endCHS[3],start,length\n");
//   for (uint8_t ip = 1; ip < 5; ip++)
//   {
//     MbrPart_t *pt = &mbr.part[ip - 1];
//     if ((pt->boot != 0 && pt->boot != 0X80) ||
//         getLe32(pt->relativeSectors) > sdCardCapacity(&m_csd))
//     {
//       valid = false;
//     }
//     cout << int(ip) << ',' << uppercase << showbase << hex;
//     cout << int(pt->boot) << ',';
//     for (int i = 0; i < 3; i++)
//     {
//       cout << int(pt->beginCHS[i]) << ',';
//     }
//     cout << int(pt->type) << ',';
//     for (int i = 0; i < 3; i++)
//     {
//       cout << int(pt->endCHS[i]) << ',';
//     }
//     cout << dec << getLe32(pt->relativeSectors) << ',';
//     cout << getLe32(pt->totalSectors) << endl;
//   }
//   if (!valid)
//   {
//     cout << F("\nMBR not valid, assuming Super Floppy format.\n");
//   }
//   return true;
// }
// //------------------------------------------------------------------------------
// void dmpVol()
// {
//   cout << F("\nScanning FAT, please wait.\n");
//   uint32_t freeClusterCount = SD.freeClusterCount();
//   if (SD.fatType() <= 32)
//   {
//     cout << F("\nVolume is FAT") << int(SD.fatType()) << endl;
//   }
//   else
//   {
//     cout << F("\nVolume is exFAT\n");
//   }
//   cout << F("sectorsPerCluster: ") << SD.sectorsPerCluster() << endl;
//   cout << F("clusterCount:      ") << SD.clusterCount() << endl;
//   cout << F("freeClusterCount:  ") << freeClusterCount << endl;
//   cout << F("fatStartSector:    ") << SD.fatStartSector() << endl;
//   cout << F("dataStartSector:   ") << SD.dataStartSector() << endl;
//   if (SD.dataStartSector() % m_eraseSize)
//   {
//     cout << F("Data area is not aligned on flash erase boundary!\n");
//     cout << F("Download and use formatter from www.sdcard.org!\n");
//   }
// }
// void updateFromFS(SdFat &fs) {
//    File updateBin = fs.open("/update.bin");
//    if (updateBin) {
//       if(updateBin.isDirectory()){
//          Serial.println("Error, update.bin is not a file");
//          updateBin.close();
//          return;
//       }

//       size_t updateSize = updateBin.size();

//       if (updateSize > 0) {
//          Serial.println("Try to start update");
//          performUpdate(updateBin, updateSize);
//       }
//       else {
//          Serial.println("Error, file is empty");
//       }

//       updateBin.close();

//       // whe finished remove the binary from SD card to indicate end of the process
//       fs.remove("/update.bin");
//    }
//    else {
//       Serial.println("Could not load update.bin from SD root");
//    }
// }

// String post(char *file_name, char *post_host, int post_port, char *url_char)
// {

//   File myFile = SD.open(file_name, O_RDONLY);

//   String url = String(url_char);

//   String fileName = file_name;
//   String size = String(fileName.length());

//   Serial.println();
//   Serial.println("file exists");
//   Serial.println(fileName);

//   if (myFile)
//   {

//     // print content length and host
//     Serial.println("contentLength");
//     Serial.println(size);
//     Serial.print("connecting to ");
//     Serial.println(post_host);

//     // try connect or return on fail
//     if (!S3Client.connect(post_host, post_port))
//     {
//       Serial.println("http post connection failed");
//       return String("Post Failure");
//     }

//     // We now create a URI for the request
//     Serial.println("Connected to server");
//     Serial.print("Requesting URL: ");
//     Serial.println(url);

//     // Make a HTTP request and add HTTP headers
//     String boundary = "Comandogatewayuploadjg2qVIUS8teOAbN3";
//     String contentType = "image/png";
//     String portString = String(post_port);
//     String hostString = String(post_host);

//     // post header
//     String postHeader = "POST " + url + " HTTP/1.1\r\n";
//     postHeader += "Host: " + hostString + ":" + portString + "\r\n";
//     postHeader += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
//     postHeader += "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
//     postHeader += "Accept-Encoding: gzip,deflate\r\n";
//     postHeader += "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n";
//     postHeader += "User-Agent: Arduino/Solar-Server\r\n";
//     postHeader += "Keep-Alive: 300\r\n";
//     postHeader += "Connection: keep-alive\r\n";
//     postHeader += "Accept-Language: en-us\r\n";

//     // key header
//     String keyHeader = "--" + boundary + "\r\n";
//     keyHeader += "Content-Disposition: form-data; name=\"key\"\r\n\r\n";
//     keyHeader += "${filename}\r\n";

//     // request header
//     String requestHead = "--" + boundary + "\r\n";
//     requestHead += "Content-Disposition: form-data; name=\"file\"; filename=\"" + fileName + "\"\r\n";
//     requestHead += "Content-Type: " + contentType + "\r\n\r\n";

//     // request tail
//     String tail = "\r\n--" + boundary + "--\r\n\r\n";

//     // content length
//     int contentLength = keyHeader.length() + requestHead.length() + myFile.size() + tail.length();
//     postHeader += "Content-Length: " + String(contentLength, DEC) + "\n\n";

//     // send post header
//     char charBuf0[postHeader.length() + 1];
//     postHeader.toCharArray(charBuf0, postHeader.length() + 1);
//     S3Client.write(charBuf0);
//     Serial.print(charBuf0);

//     // send key header
//     char charBufKey[keyHeader.length() + 1];
//     keyHeader.toCharArray(charBufKey, keyHeader.length() + 1);
//     S3Client.write(charBufKey);
//     Serial.print(charBufKey);

//     // send request buffer
//     char charBuf1[requestHead.length() + 1];
//     requestHead.toCharArray(charBuf1, requestHead.length() + 1);
//     S3Client.write(charBuf1);
//     Serial.print(charBuf1);

//     // create buffer
//     const int bufSize = 4096;
//     byte clientBuf[bufSize];
//     int clientCount = 0;
//     int size = 0;

//     Serial.print(" File Size: ");
//     Serial.print(myFile.size());
//     Serial.println(" Bytes");

//     while (myFile.available())
//     {

//       size = myFile.read(clientBuf, bufSize);

//       if (size)
//       {
//         clientCount += size;
//         S3Client.write((const uint8_t *)clientBuf, ((size > sizeof(clientBuf)) ? sizeof(clientBuf) : size));
//         Serial.print("Uploading... ");
//         Serial.println(myFile.size() - clientCount);
//       }

//       // S3Client.write((const uint8_t *)clientBuf, bufSize);
//       // clientBuf[clientCount] = myFile.read();

//       // clientCount += bufSize;
//       // round++;

//       // if (clientCount > (bufSize - 1))
//       // {
//       //
//       //   Serial.println(" Bytes");

//       //   clientCount = 0;
//       // }
//     }

//     // if (clientCount > 0)
//     // {
//     //   S3Client.write((const uint8_t *)clientBuf, clientCount);
//     //   Serial.println("Sent LAST buffer");
//     // }

//     // send tail
//     char charBuf3[tail.length() + 1];
//     tail.toCharArray(charBuf3, tail.length() + 1);
//     S3Client.write(charBuf3);
//     Serial.print(charBuf3);

//     // Read all the lines of the reply from server and print them to Serial
//     Serial.println("request sent");
//     String responseHeaders = "";

//     while (S3Client.connected())
//     {
//       // Serial.println("while client connected");
//       String line = S3Client.readStringUntil('\n');
//       Serial.println(line);
//       responseHeaders += line;
//       if (line == "\r")
//       {
//         Serial.println("headers received");
//         break;
//       }
//     }

//     String line = S3Client.readStringUntil('\n');

//     Serial.println("reply was:");
//     Serial.println("==========");
//     Serial.println(line);
//     Serial.println("==========");
//     Serial.println("closing connection");

//     // close the file:
//     myFile.close();
//     return responseHeaders;
//   }
// }