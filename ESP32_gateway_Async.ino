#include <Abellion.h>
#include <AWS_IOT.h>
#include "ping.h"

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>

#include <SPIFFS.h>
// #include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <NTPClient.h>

#include <NeoPixelAnimator.h>
#include <NeoPixelBus.h>

#include <TimeLib.h>

#include <SPIFFSEditor.h>
// #include <SPIFFSEditor.cpp>

#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <RH_RF95.h>
#include <RHMesh.h>
#include <pthread.h>
#include <PubSubClient.h>
#include <HTTPClient.h>

#include <Update.h>

#include <SD_MMC.h>

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

#include <SPI.h>
#include "SD.h"

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

// Lora configuration driver
#include <RH_RF95.h>
#include <RHReliableDatagram.h>
String getMacAddress();

Abellion GW;

uint32_t epochTime;
int offset = -3;

#define GPSPort Serial1

//Serial.println("request file open...");

AWS_IOT awsClient;

char HOST_ADDRESS[] = "a37xdfod1g833h.iot.us-east-1.amazonaws.com";
char CLIENT_ID[] = "ESP-Gateway_data";
char TOPIC_NAME[] = "Gateway_1/status";

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

const uint8_t AnimationChannels = 1;
#ifdef DEBUG
NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> strip(1, 0);
#else
NeoPixelBus<NeoGrbwFeature, NeoWs2813Method> strip(1, 25);
#endif

NeoPixelAnimator animations(AnimationChannels);

struct MyAnimationState
{
  RgbwColor StartingColor;
  RgbwColor EndingColor;
};

MyAnimationState animationState[AnimationChannels];

int colorSaturation = 125; // saturation of color constants

WiFiClient espClient,
    Client,
    S3Client;
PubSubClient mqtt_client(espClient);

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

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

char sub_topics[10][50];
char pub_topics[10][50];

int max_reconnect = 50;
int number_of_reconnects;

char mqtt_server[50];
char msg[10000];

int total_stations;
int total_old_data;

TinyGPSPlus gps;

float flat, flon, alt;
float readed_gps_flat, readed_gps_flon;
long last_fix;
unsigned long age = 0;
const int max_stations = 20;
const int max_remote_sensors = 40;

Estation estationdata[max_stations];
remoteSensors remoteSensors[max_remote_sensors], incommingSensor;

struct Connections_to_verify
{
  uint8_t station_address;
  bool valid = false;

} stations_to_check[max_stations];
bool has_stations_to_check = false;

TransmissionData Incomming;
TransmissionDataSmall IncommingSmall;

String data_in;
int connected_stations;

double Internal_battery_voltage;
bool External_power_connected;

bool external_power_event = false, external_power_last = false;
bool power_changed_state = false, first_state = true;

int address_gateway = 101;
bool radio_busy = false;

char *wifi_config_file = "/configuration_files/wificonfig.json";
bool connecting_on_wifi = false;
bool connected_to_cloud = false;
bool mqtt_connected = false;

//Status Variables
bool position_changed = false;
volatile bool upload_file = false;

float received_size;
float actual_received_size = 0;
uint32_t previous_time = 0;
float previous_size = 0;
bool downloading_file = false;

DynamicJsonBuffer filelistbuffer;
JsonArray &jarray = filelistbuffer.createArray();

enum WIFI_STATUS
{
  WIFI_OFFLINE = 0x00,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_LOWBATTERY,
  WIFI_WAITING_PASSWD,
  NETWORK_CONNECTING,
  MQTT_CONNECTING,
  MQTT_CONNECT,
  LOW_POWER_CONNECTED,
  EXTRA_MODE
};

char status_LED = WIFI_OFFLINE, status_LED_now = 0x00;

#ifdef DEBUG
RH_RF95 rf95(5, 17);
#else
RH_RF95 rf95(5, 4);
RHReliableDatagram manager(rf95, address_gateway);
#endif

// SKETCH BEGIN
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
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
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_PONG)
  {
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
  }
  else if (type == WS_EVT_DATA)
  {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    String msg = "";
    if (info->final && info->index == 0 && info->len == len)
    {
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

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
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

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
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final)
        {
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
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

// void IRAM_ATTR taskOne(void *pvParameters)
void taskOne(void *pvParameters)
{
  // Serial.println("<booting> Task one ------------------------------");
  for (;;)
  {
    connected_stations = renewConnection();

    if (!radio_busy)
    {
      if (connected_stations < 3)
      {
        checkNewDevices();
        // Serial.print("time:");
        // Serial.println(now());
        if (!has_stations_to_check)
          vTaskDelay(30000);
        else
          vTaskDelay(1000);
      }
      else
      {
        checkNewDevices();
        vTaskDelay(60000);
      }
    }
    else
    {
      vTaskDelay(1000);
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
  // Serial.println("<booting> Task LED ------------------------------");
  int status = 0;
  for (;;)
  {

    if (animations.IsAnimating())
    {

      animations.UpdateAnimations();
      // Serial.println("animation...");
      strip.Show();
      vTaskDelay(10);
    }
    else
    {
      switch (status_LED)
      {
      case WIFI_OFFLINE:
      {
        if (status == 1)
        {
          animationState[0].StartingColor = RgbwColor(colorSaturation, 0, 0, 0);
          animationState[0].EndingColor = RgbwColor(0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set out...");
          status = 0;
        }
        else if (status == 0)
        {
          animationState[0].StartingColor = RgbwColor(0);
          animationState[0].EndingColor = RgbwColor(colorSaturation, 0, 0, 0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set in...");
          status = 1;
        }
        break;
      }
      case WIFI_CONNECTING:
      {
        if (status == 1)
        {
          animationState[0].StartingColor = RgbwColor(colorSaturation, 10, 0, 0);
          animationState[0].EndingColor = RgbwColor(10, colorSaturation, 0, 0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set out...");
          status = 0;
        }
        else if (status == 0)
        {
          animationState[0].StartingColor = RgbwColor(10, colorSaturation, 0, 0);
          animationState[0].EndingColor = RgbwColor(colorSaturation, 10, 0, 0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set in...");
          status = 1;
        }
        break;
      }
      case MQTT_CONNECT:
      {
        if (status == 1)
        {
          animationState[0].StartingColor = RgbwColor(0);
          animationState[0].EndingColor = RgbwColor(0, colorSaturation, 0, 0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set out...");
          status = 0;
        }
        else if (status == 0)
        {
          animationState[0].StartingColor = RgbwColor(0, colorSaturation, 0, 0);
          animationState[0].EndingColor = RgbwColor(0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set in...");
          status = 1;
        }
        break;
      }
      case WIFI_WAITING_PASSWD:
      {
        if (status == 1)
        {
          animationState[0].StartingColor = RgbwColor(0, colorSaturation, 0, 0);
          animationState[0].EndingColor = RgbwColor(0, 0, colorSaturation, 0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set out...");
          status = 0;
        }
        else if (status == 0)
        {
          animationState[0].StartingColor = RgbwColor(0, 0, colorSaturation, 0);
          animationState[0].EndingColor = RgbwColor(0, colorSaturation, 0, 0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set in...");
          status = 1;
        }
        break;
      }
      case MQTT_CONNECTING:
      {
        if (status == 1)
        {
          animationState[0].StartingColor = RgbwColor(colorSaturation, colorSaturation, 0, 0);
          animationState[0].EndingColor = RgbwColor(0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set out...");
          status = 0;
        }
        else if (status == 0)
        {
          animationState[0].StartingColor = RgbwColor(0);
          animationState[0].EndingColor = RgbwColor(colorSaturation, colorSaturation, 0, 0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set in...");
          status = 1;
        }
        break;
      }
      case LOW_POWER_CONNECTED:
      {
        if (status == 1)
        {
          animationState[0].StartingColor = RgbwColor(20, 20, 0, 0);
          animationState[0].EndingColor = RgbwColor(0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set out...");
          status = 0;
        }
        else if (status == 0)
        {
          animationState[0].StartingColor = RgbwColor(0);
          animationState[0].EndingColor = RgbwColor(20, 20, 0, 0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set in...");
          status = 1;
        }
        break;
      }
      case NETWORK_CONNECTING:
      {
        if (status == 1)
        {
          animationState[0].StartingColor = RgbwColor(0, 100, 100, 0);
          animationState[0].EndingColor = RgbwColor(0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set out...");
          status = 0;
        }
        else if (status == 0)
        {
          animationState[0].StartingColor = RgbwColor(0);
          animationState[0].EndingColor = RgbwColor(0, 100, 100, 0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set in...");
          status = 1;
        }
        break;
      }
      case WIFI_CONNECTED:
      {
        if (status == 1)
        {
          animationState[0].StartingColor = RgbwColor(0, 0, 100, 0);
          animationState[0].EndingColor = RgbwColor(0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set out...");
          status = 0;
        }
        else if (status == 0)
        {
          animationState[0].StartingColor = RgbwColor(0);
          animationState[0].EndingColor = RgbwColor(0, 0, 100, 0);
          animations.StartAnimation(0, 500, BlendAnimUpdate);
          //Serial.println("set in...");
          status = 1;
        }
        break;
      }
      case EXTRA_MODE:
      {
        if (status == 0)
        {
          animationState[0].StartingColor = RgbwColor(100, 0, 0, 0);
          animationState[0].EndingColor = RgbwColor(0, 100, 0, 0);
          animations.StartAnimation(0, 300, BlendAnimUpdate);
          //Serial.println("set out...");
          status = 1;
        }
        else if (status == 1)
        {
          animationState[0].StartingColor = RgbwColor(0, 100, 0, 0);
          animationState[0].EndingColor = RgbwColor(0, 0, 100, 0);
          animations.StartAnimation(0, 300, BlendAnimUpdate);
          //Serial.println("set in...");
          status = 2;
        }
        else if (status == 2)
        {
          animationState[0].StartingColor = RgbwColor(0, 0, 100, 0);
          animationState[0].EndingColor = RgbwColor(100, 0, 0, 0);
          animations.StartAnimation(0, 300, BlendAnimUpdate);
          //Serial.println("set in...");
          status = 0;
        }
        break;
      }
      }
    }
  }
}
void BlendAnimUpdate(const AnimationParam &param)
{
  RgbwColor updatedColor = RgbwColor::LinearBlend(
      animationState[param.index].StartingColor,
      animationState[param.index].EndingColor,
      param.progress);

  strip.SetPixelColor(0, updatedColor);
}
SPIClass spi;

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
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event)
  {
  case SYSTEM_EVENT_STA_GOT_IP:
    Serial.println("---------------------------------------------------------------------------------WiFi connected");
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

  DynamicJsonBuffer jsonBuffer;
  JsonObject &config = jsonBuffer.createObject();

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

  config.printTo(startingup);
  config.prettyPrintTo(Serial);

  mqttClient.publish(pub_topics[0], 2, true, startingup);

  mqttClient.subscribe(sub_topics[0], 1);
  mqttClient.subscribe(sub_topics[1], 1);
  mqttClient.subscribe(sub_topics[2], 1);
  mqttClient.subscribe(sub_topics[3], 1);
  mqttClient.subscribe(sub_topics[4], 1);
  mqttClient.subscribe(sub_topics[5], 1);
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

  // char filename[64];

  // createDir(SD, "/readed_data");

  // sprintf(filename, "/ConnLost/PF-%ld.json", now());

  // File ConnLostFile = SD.open(filename, FILE_WRITE);

  // if (ConnLostFile)
  // {
  //   DynamicJsonBuffer jsonBuffer;
  //   JsonObject &eventinfo = jsonBuffer.createObject();

  //   String date = String(day(now()));
  //   String hour_s = String(hour(now()));
  //   eventinfo["lastconnection_date"] = String(date + "/" + month(now()) + "/" + year(now()));
  //   eventinfo["lastconnection_time"] = String(hour_s + ":" + minute(now()) + ":" + second(now()));

  //   eventinfo["start_of_connection_loss_epoch"] = now();
  //   eventinfo["Connected_stations"] = connected_stations;

  //   eventinfo.printTo(ConnLostFile);
  //   //config.printTo(to_file);
  //   //Serial.print("to print: ");
  //   // Serial.println(to_file);

  //   // config.prettyPrintTo(Serial);
  //   // Serial.println("New position saved on file");
  // }
  // else
  // {
  //   Serial.println("unable to open file");
  // }
  // ConnLostFile.close();
  status_LED = MQTT_CONNECT;
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

    DynamicJsonBuffer jsonBuffer;
    JsonObject &config = jsonBuffer.createObject();

    config["Latitude"] = GW.gateway_std_data.stored_lat;
    config["Longitude"] = GW.gateway_std_data.stored_lon;
    config["altitude"] = GW.gateway_std_data.stored_alt;
    config["lastconnection"] = now();
    String date = String(day(now()));
    String hour_s = String(hour(now()));
    config["lastconnection_date"] = String(date + "/" + month(now()) + "/" + year(now()));
    config["Connected_stations"] = connected_stations;

    config["Free_Memory"] = ((float)ESP.getFreeHeap() / 1000);
    config["SSID"] = WiFi.SSID();
    config["Wifi_Power"] = WiFi.RSSI();
    config["Disk_size"] = ((float)SD.cardSize() / (1024 * 1024));
    config["Disk_used_space"] = ((float)SD.usedBytes() / (1024 * 1024));

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

    config.printTo(msg);

    // Serial.println(msg);
    mqttClient.publish(pub_topics[2], 0, false, msg);
  }
  else if (strcmp(topic, sub_topics[3]) == 0)
  {
    Serial.println("<action>TESTING OTA FUNCTION!\r\n");
    DynamicJsonBuffer jsonBuffer;
    JsonObject &config = jsonBuffer.parseObject((char *)payload);

    if (!config.success())
    {
      Serial.print("fail to parse");
    }
    else
    {
      host = config["Host_address"];
      bin = config["File"];
      version = config["Firmware_version"];
      config.prettyPrintTo(Serial);
    }

    Serial.println("<action>Starting OTA\r\n");
    snprintf(msg, 75, "{\"Time\":\"%lu\"}", now());
    Serial.println(msg);
    mqtt_client.publish(pub_topics[1], msg);

    upload_file = true;
  }
  else if (strcmp(topic, sub_topics[1]) == 0)
  {

    Serial.println("<action>RESETTING\r\n");
    snprintf(msg, 75, "{\"Time\":\"%lu\"}", now());
    Serial.println(msg);
    mqttClient.publish(pub_topics[1], 0, false, msg);
    delay(1000);
    ESP.restart();
  }
  else if (strcmp(topic, sub_topics[5]) == 0)
  {

    // filelistbuffer.clear();
    listDir(SD, "/", 1);
    Serial.println("----------------------------------------------------------------------------------------------------\r\n");

    jarray.prettyPrintTo(Serial);

    Serial.println("<action>Sending file list\r\n");
    // root.prettyPrintTo(Serial);
    jarray.printTo(msg);

    mqttClient.publish(pub_topics[5], 0, false, msg);
    // delay(100);
    filelistbuffer.clear();
    JsonArray &jarray = filelistbuffer.createArray();
  }
  else if (strcmp(topic, sub_topics[6]) == 0)
  {

    Serial.println("<action>TESTING OTA FUNCTION!\r\n");
    DynamicJsonBuffer jsonBuffer;
    JsonObject &config = jsonBuffer.parseObject((char *)payload);

    if (!config.success())
    {
      Serial.print("fail to parse");
    }
    else
    {
      host = config["Host_address"];
      bin = config["File"];
      version = config["Firmware_version"];
      config.prettyPrintTo(Serial);
    }

    Serial.println("<action>Starting File Download\r\n");
    snprintf(msg, 75, "{\"Time\":\"%lu\"}", now());
    Serial.println(msg);
    mqtt_client.publish(pub_topics[1], msg);

    upload_file = true;
  }
}

void setup()
{
  delay(1000);
  Serial.begin(250000);

  Serial.println("serial Init");
  pinMode(25, OUTPUT); //LED
  pinMode(33, ANALOG); //Leitura Bateria
  pinMode(32, ANALOG); //Status da energia

  analogReadResolution(12);       //12 bits
  analogSetAttenuation(ADC_11db); //For all pins

  pinMode(34, OUTPUT);
  // pinMode(33, OUTPUT);
  pinMode(2, OUTPUT);
  // pinMode(4, OUTPUT);
  pinMode(15, OUTPUT);
  pinMode(13, INPUT_PULLUP);
  digitalWrite(2, HIGH);
  // digitalWrite(4, HIGH);
  spi = SPIClass(HSPI);

  spi.begin();

  strip.Begin();
  // strip.SetBrightness(125);
  strip.Show();
  Serial.println("Started");

  // if (psramInit())
  // {
  //   Serial.println("PSRAM was found and loaded");
  // }
  //GW.init();

  // Serial.printf("Internal Total heap %d, internal Free Heap %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
  // //Internal RAM
  // //SPI RAM
  // Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram());

  // Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());

  // Serial.printf(" Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());

  // Serial.println("PSRAM init PASS");
  devname = "dev" + getMacAddress();
  devname.toCharArray(s_devname, 20);
  memcpy(GW.gateway_std_data.name, s_devname, strlen(s_devname));
  Serial.println("PSRAM was found and loaded");

  Serial.println(s_devname);

  auto speed = 48000000; //3.4 mins on a 22.24 MB file @ 35MHz
                         //3.2 mins on a 22.24 MB file @ 48MHz
  if (!SD.begin(15, spi, speed))
  {
    Serial.println("NOT INIT");
    delay(100);
    ESP.restart();
  }
  status_LED = WIFI_OFFLINE;
  status_LED_now = status_LED;
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
  {
    Serial.println("MMC");
  }
  else if (cardType == CARD_SD)
  {
    Serial.println("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  }
  else
  {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  uint64_t usedBytes = SD.usedBytes() / (1024 * 1024);
  Serial.printf("SD Card Size: %llu MB, SD card used data: %llu MB\n", cardSize, usedBytes);

  if (!SPIFFS.begin(true))
  {
    Serial.println("Failed to mount file system");
  }
  // saveConfig();
  loadConfig();

  Serial.printf("mqtt server addr: %s\r\n", mqtt_server);

  if (!SD.exists("/configuration_files"))
  {
    createDir(SD, "/configuration_files");
    Serial.println("folder didn't exist, creating");
  }

#ifdef DEBUG
  GPSPort.begin(4800);
#else
  GPSPort.begin(9600);
#endif

  gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT);

  sprintf(sub_topics[0], "%s/config", s_devname);
  sprintf(sub_topics[1], "%s/config/reset", s_devname);
  sprintf(sub_topics[2], "%s/config/status_report", s_devname);
  sprintf(sub_topics[3], "%s/config/OTA_firmware", s_devname);
  sprintf(sub_topics[4], "%s/config/lora_config", s_devname);
  sprintf(sub_topics[5], "%s/config/list_files", s_devname);
  sprintf(sub_topics[6], "%s/config/downloadFile", s_devname);

  sprintf(pub_topics[0], "%s/status/startingup", s_devname);
  sprintf(pub_topics[1], "%s/status/reset", s_devname);
  sprintf(pub_topics[2], "%s/status/report", s_devname);
  sprintf(pub_topics[3], "%s/status/coldstart", s_devname);
  sprintf(pub_topics[4], "%s/status/statusupdate", s_devname);
  sprintf(pub_topics[5], "%s/status/list_files", s_devname);
  sprintf(pub_topics[6], "%s/status/file_LORA_upload", s_devname);
  sprintf(pub_topics[7], "%s/status/downloadFile", s_devname);

  for (int i = 0; i < 7; i++)
  {
    Serial.printf("-  topic subscribed: %s\r\n", sub_topics[i]);
  }
  status_LED = WIFI_CONNECTING;

  Serial.println("CPU0 reset reason: ");
  print_reset_reason(rtc_get_reset_reason(0));

  Serial.println("CPU1 reset reason: ");
  print_reset_reason(rtc_get_reset_reason(1));

  xTaskCreatePinnedToCore(
      taskGPS,   /* Task function. */
      "taskGPS", /* String with name of task. */
      8192,      /* Stack size in words. */
      NULL,      /* Parameter passed as input of the task */
      1,         /* Priority of the task. */
      NULL,
      1); /* Task handle. */

  xTaskCreatePinnedToCore(
      taskLED,   /* Task function. */
      "taskLED", /* String with name of task. */
      2048,      /* Stack size in words. */
      NULL,      /* Parameter passed as input of the task */
      1,         /* Priority of the task. */
      NULL,
      1); /* Task handle. */

  xTaskCreatePinnedToCore(
      taskOne,   /* Task function. */
      "TaskOne", /* String with name of task. */
      8192,      /* Stack size in words. */
      NULL,      /* Parameter passed as input of the task */
      1,         /* Priority of the task. */
      NULL,
      0); /* Task handle. */

  xTaskCreatePinnedToCore(
      handle_radio,   /* Task function. */
      "handle_radio", /* String with name of task. */
      8192,           /* Stack size in words. */
      NULL,           /* Parameter passed as input of the task */
      1,              /* Priority of the task. */
      NULL,
      0); /* Task handle. */

  // ESP_LOGI("<booting> Attempting WiFi connection");
  //or reconnect they work about the same

  //strip.SetPixelColor(0, yellow);
  //strip.Show();

  setupLora();

  WiFi.onEvent(WiFiEvent);

  mqttReconnectTimer = xTimerCreate("mqttTimer",
                                    pdMS_TO_TICKS(2000),
                                    pdFALSE, (void *)0,
                                    reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));

  Serial.printf("mqtt_server: %s\r\n", mqtt_server);
  const char *mqtt_server_address = "mqtt.comandosolutions.com";
  mqttClient.setServer(mqtt_server_address, 1883);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);

  wifiReconnectTimer = xTimerCreate("wifiTimer",
                                    pdMS_TO_TICKS(2000),
                                    pdFALSE, (void *)0,
                                    reinterpret_cast<TimerCallbackFunction_t>(savewificonfig()));

  printLocalTime();

  if (!MDNS.begin("gatewayconfig"))
  {
    // Serial.println("Error setting up MDNS responder!");
    while (1)
    {
      delay(10);
    }
  }
  else
    // Serial.println("UP MDNS responder!");

    xTaskCreatePinnedToCore(
        taskRunning,   /* Task function. */
        "taskRunning", /* String with name of task. */
        16384,         /* Stack size in words. */
        NULL,          /* Parameter passed as input of the task */
        1,             /* Priority of the task. */
        NULL,
        1); /* Task handle. */

  xTaskCreatePinnedToCore(
      taskLORAupdate,   /* Task function. */
      "taskLORAupdate", /* String with name of task. */
      8192,             /* Stack size in words. */
      NULL,             /* Parameter passed as input of the task */
      1,                /* Priority of the task. */
      NULL,
      1); /* Task handle. */

  xTaskCreatePinnedToCore(
      taskPing,   /* Task function. */
      "taskPing", /* String with name of task. */
      8192,       /* Stack size in words. */
      NULL,       /* Parameter passed as input of the task */
      1,          /* Priority of the task. */
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

  int8_t j;
  esp_wifi_set_max_tx_power(8);

  esp_wifi_get_max_tx_power(&j);

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // webSocket.begin();
  MDNS.addService("http", "tcp", 80);
  // webSocket.onEvent(webSocketEvent);

  events.onConnect([](AsyncEventSourceClient *client) {
    client->send("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
  });
  server.addHandler(&events);

  server.addHandler(new SPIFFSEditor(SD));

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
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

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

    // File estationFile = SD.open("/SD-01042019-STFF3605D9.json", FILE_READ);

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
    root.printTo(*response);
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

    //   DynamicJsonBuffer jsonBuffer;

    //   JsonObject &config = jsonBuffer.parseObject(buf.get());
    //   config.printTo(*response);
    // }

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

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
    root.prettyPrintTo(*response);

    request->send(response);
  });
  server.on("/lora_parameters.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Get \"/lora_parameters.json\" request received");
    AsyncResponseStream *response = request->beginResponseStream("application /json");
    // Exemplo de dados para coleta.
    response->addHeader("Access-Control-Allow-Origin", "*");

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["Power"] = "Gateway01";
    root["Power"] = "Gateway01";
    root["Power"] = "Gateway01";
    root["Power"] = "Gateway01";
    root["Power"] = "Gateway01";
    root["Power"] = "Gateway01";

    root.printTo(*response);

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

    returnPing = ping_start(adr_mqtt, 4, 0, 0, 1);
    ping_mqtt = returnPing.total_time;
    returnPing = ping_start(adr_google, 4, 0, 0, 1);
    ping_google = returnPing.total_time;
  }

  vTaskDelay(100000);
}
void taskLORAupdate(void *pvParameters)
{
  for (;;)
  {
    if (upload_file)
    {
      send_file_over_LORA("/WeatherStationMedium_filetransfer.bin");
      upload_file = false;
    }
    vTaskDelay(10);
  }
}

void taskRunning(void *pvParameters)
{
  for (;;)
  {

    DynamicJsonBuffer httprequestjson;
    JsonObject &stored_file_info = httprequestjson.createObject();

    if (millis() - timed_send > 120000)
    {
      Serial.println("Lost LORA");
      LoraConnected = false;
      setupLora();
    }

    // String hour_s = String(hour(now()));
    // stored_file_info["Time_of_power_fail"] = String(hour_s + ":" + minute(now()) + ":" + second(now()));
    // stored_file_info["data"] = 0;

    // config["altitude"] = GW.gateway_std_data.stored_alt;
    // config["lastconnection"] = now();
    // String date = String(day(now()));
    // String hour_s = String(hour(now()));
    // config["lastconnection_date"] = String(date + "/" + month(now()) + "/" + year(now()));
    // config["lastconnection_time"] = String(hour_s + ":" + minute(now()) + ":" + second(now()));

    // config["LocalIP"] = String(WiFi.localIP().toString());
    // config["externalIP"] = GetExternalIP();
    // config["Battery_Voltage"] = String(Internal_battery_voltage, 3);
    // config["External_Power_Connected"] = String(External_power_connected);

    // stored_file_info.printTo(msg);
    // stored_file_info.prettyPrintTo(Serial);

    // String header = "{\"Station\":{\"address\" :2,\"data\":";

    // HTTPClient http;

    // File estationFile = SD.open("/SD-01042019-STFF3605D9.json", FILE_READ);

    // http.begin("http://34.238.249.191:1880/fileuploder"); //Specify destination for HTTP request
    // http.addHeader("Content-Type", "application/json");   //Specify content-type header
    // http.addHeader("cache-control", "no-cache");          //Specify content-type header
    // // http.addHeader("cache-control", "no-cache");          //Specify content-type header

    // // http.addHeader("Content-Length", String(estationFile.size()));
    // float time_start = millis();
    // // int httpResponseCode = http.sendRequest("POST", &estationFile, estationFile.size()); //Send the actual POST request
    // // int httpResponseCode = http.sendRequest("POST", (uint8_t *)header.c_str(), &estationFile, header.length(), estationFile.size()); //Send the actual POST request
    // int httpResponseCode = http.sendRequest("POST", (uint8_t *)msg, &estationFile, strlen(msg) - 2, estationFile.size()); //Send the actual POST request
    // float time_end = millis();

    // if (httpResponseCode > 0)
    // {
    //   String response = http.getString(); //Get the response to the request
    //   Serial.println(httpResponseCode);   //Print return code
    //   Serial.println(response);           //Print request answer

    //   Serial.printf("performance on HTTP request - file size: %.1f - transfer rate: %.1f bps", (float)estationFile.size(), (float)(estationFile.size() / ((time_end - time_start) / 1000)));
    // }
    // else
    // {

    //   Serial.print("Error on sending POST: ");
    //   Serial.println(httpResponseCode);
    // }

    // http.end();

    // estationFile.close();

    // //chack if the position has changed
    if (position_changed)
    {
      position_changed = false;
      DynamicJsonBuffer jsonBuffer;
      JsonObject &config = jsonBuffer.createObject();

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

      config.printTo(msg);
      // config.prettyPrintTo(Serial);

      mqtt_client.publish(pub_topics[5], msg, true);
    }
    double battery_volts = 0;
    for (int i = 0; i < 10000; i++)
    {
      battery_volts += analogRead(33);
      delayMicroseconds(1000);
    }

    battery_volts /= 10000;

    Internal_battery_voltage = (battery_volts * 6.6 / 4096) * 1.0736196319018404907975460;

    if (power_changed_state && !first_state)
    {
      power_changed_state = false;

      DynamicJsonBuffer jsonBuffer;
      JsonObject &config = jsonBuffer.createObject();

      config["Latitude"] = GW.gateway_std_data.stored_lat;
      config["Longitude"] = GW.gateway_std_data.stored_lon;
      config["altitude"] = GW.gateway_std_data.stored_alt;
      config["lastconnection"] = now();
      String date = String(day(now()));
      String hour_s = String(hour(now()));
      config["lastconnection_date"] = String(date + "/" + month(now()) + "/" + year(now()));
      config["lastconnection_time"] = String(hour_s + ":" + minute(now()) + ":" + second(now()));

      config["LocalIP"] = String(WiFi.localIP().toString());
      config["externalIP"] = externalIP;
      config["Battery_Voltage"] = String(Internal_battery_voltage, 3);
      config["External_Power_Connected"] = String(External_power_connected);
      config["External_Power_Value"] = String(analogRead(32));

      // config.printTo(msg);
      // config.prettyPrintTo(Serial);
      // if (mqtt_client.connected())
      //   mqtt_client.publish(pub_topics[0], msg, true);
    }

    if ((analogRead(32) < 700 && analogRead(32) > -1) && !external_power_last) // Retornou a energia no dispositivo
    {
      if (first_state)
      {
        External_power_connected = true;
        status_LED = status_LED_now;
        power_changed_state = true;
      }
    }
    else if (!(analogRead(32) < 700 && analogRead(32) > -1) && external_power_last) // Dispositivo desconectou da energia elétrica
    {

      status_LED_now = status_LED;
      status_LED = LOW_POWER_CONNECTED;
      power_changed_state = true;
    }

    if (!(analogRead(32) < 700 && analogRead(32) > -1))
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
    int delta = actual_received_size - previous_size;
    float trasnsmit_interval = (millis() - previous_time) / 1000;

    Serial.printf("%f battery volts, data: %d \r\n  --- file received total %f %\r\n --- speed %f BPS\r\n",
                  Internal_battery_voltage,
                  analogRead(32),
                  mapDouble(actual_received_size, 0, received_size, 0, 100),
                  float(delta / trasnsmit_interval));
    // }
    previous_size = actual_received_size;
    previous_time = millis();

    first_state = false;

    vTaskDelay(100);
  }
}
bool send_file_over_LORA(const char *path)
{
  File estationFile = SD.open(path, FILE_READ);
  // vTaskSuspend(taskPing);
  // vTaskSuspend(handle_radio);
  // vTaskSuspend()

  if (estationFile)
  {
    while (radio_busy)
    {
      Serial.printf("Waiting radio to send file\r\n");
      vTaskDelay(250);
    }

    radio_busy = true;
    uint8_t data[RH_RF95_MAX_MESSAGE_LEN];
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

    int pages = size / (RH_RF95_MAX_MESSAGE_LEN - 3); //TAMANHO MAXIMO DO FRAME POR MENSAGEM
    int actual_page = 0;
    if (size % (RH_RF95_MAX_MESSAGE_LEN - 3) > 0)
      // pages = pages + 1;

      Serial.printf("Preparing file for transfer, size: %d - n° of parts : %d, the max size is : %d\r\n", size, pages, RH_RF95_MAX_MESSAGE_LEN);

    data[0] = 0xA0;
    memcpy(&data[1], &filename, strlen(filename));

    memcpy(&data[2 + strlen(filename)], &size, sizeof(size));

    if (!manager.sendtoWait(data, 1 + strlen(filename) + sizeof(size), 10))
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
        data[0] = 0xAA;
        int page_traking = actual_page;
        uint8_t low_page = page_traking & 0xff;
        uint8_t high_page = (page_traking >> 8);
        bool last_message = false;

        data[1] = low_page;
        data[2] = high_page;

        for (int i = 0; i < RH_RF95_MAX_MESSAGE_LEN - 3; i++)
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
          // Serial.print(i);
          // present_page_bytes = i;
        }
        DynamicJsonBuffer jsonBuffer;
        JsonObject &config = jsonBuffer.createObject();

        if (last_message)
        {
          start_time = millis();
          if (!manager.sendtoWait(data, present_page_bytes + 3, 10))
          {
            Serial.println("File send ERROR!");
            break;
          }
          end_time = millis();
        }
        else
        {
          start_time = millis();
          if (!manager.sendtoWait(data, RH_RF95_MAX_MESSAGE_LEN, 10))
          {
            Serial.println("File send ERROR!");
            break;
          }
          end_time = millis();
        }

        timed_send = millis();
        vTaskDelay(100);
        // Serial.println("<action>Awensoring report\r\n");

        config["FileUploaded"] = String(filename);
        config["Status"] = mapDouble(actual_page, 0, pages + 1, 0, 100);
        config["KBPS"] = RH_RF95_MAX_MESSAGE_LEN / ((float)(end_time - start_time) / 1000);
        config["SNR"] = rf95.lastSNR();
        config["RSSI"] = rf95.lastRssi();
        config["Total_Time_To_Transfer"] = (millis() - file_transfer_init_time) / 1000;

        config.printTo(msg);
        mqttClient.publish(pub_topics[6], 0, false, msg);
        Serial.print("actual page: ");
        Serial.println(actual_page);
        actual_page++;
        // Serial.println("checking the string generated: ");
        // Serial.println((char *)&data[3]);
        // Serial.println("");
      }

      Serial.println("File send done");
      radio_busy = false;
      estationFile.close();

      DynamicJsonBuffer jsonBuffer;
      JsonObject &config = jsonBuffer.createObject();

      // config["FileUploaded"] = String(filename);

      config["Total_Time_To_Transfer"] = (millis() - file_transfer_init_time) / 1000;
      config["Status"] = 0;
      config["KBPS"] = 0;
      config["SNR"] = 0;
      config["RSSI"] = 0;

      config.printTo(msg);
      mqttClient.publish(pub_topics[6], 0, false, msg);
    }
    else
    {
      Serial.println("File send ERROR!");
      radio_busy = false;
    }
  }
}
void taskPing(void *pvParameters)
{
  delay(10000);
  downloadFile("http://speedtest.ftp.otenet.gr/files/test10Mb.db", "/10MB.zip");
  while (1)
    vTaskDelay(10);
  if (upload_file)
  {
  }
}
void handle_radio(void *pvParameters)
{
  for (;;)
  {
    // if (!radio_busy)
    {
      // if (manager.available() && !radio_busy)
      {
        radio_busy = true;
        if (manager.available())
        {

          uint8_t mensagem_in[RH_RF95_MAX_MESSAGE_LEN];
          uint8_t mensagem_out[RH_RF95_MAX_MESSAGE_LEN];
          len = RH_RF95_MAX_MESSAGE_LEN;

          Serial.println("got new data");
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

                DynamicJsonBuffer estationbuffer;
                JsonObject &estationconfigjson = estationbuffer.createObject();

                memcpy(&estationdata[from].config, &mensagem_in[1], sizeof(estationdata[from].config));

                estationconfigjson["name"] = estationdata[from].config.name;
                estationconfigjson["time_of_registry"] = millis();
                estationconfigjson["battery_level"] = estationdata[from].config.battery_volts;
                estationconfigjson["Latitude"] = estationdata[from].config.gps_flat;
                estationconfigjson["Longitude"] = estationdata[from].config.gps_flon;
                estationconfigjson["GPSfix"] = estationdata[from].config.last_gps_fix;
                estationconfigjson["Lora_pwr"] = estationdata[from].config.radio_pwr;

                estationconfigjson.prettyPrintTo(Serial);

                Serial.printf("Received registration request from %d with name: %s\r\n -RSSI reported: %d \r\n",
                              from, estationdata[from].config.name, estationdata[from].config.last_received_RSSI);

                if (estationdata[from].config.Card_present == true)
                {
                  Serial.printf(" -SD Memory %d GB memory and %d GB free card space\r\n",
                                estationdata[from].config.CardSize, estationdata[from].config.CardFreeSpace);
                  estationconfigjson["SDcardSize"] = estationdata[from].config.CardSize;
                }

                Serial.printf(" -The station is LAT: %f LON: %f, at a hight of %f m, the last fix is %d hours ago\r\n",
                              estationdata[from].config.gps_flat, estationdata[from].config.gps_flon, estationdata[from].config.gps_altitude, ((now() - estationdata[from].config.last_gps_fix) / 3600));
                Serial.printf(" -The present battery voltage is %f V, the number of remote sensors is: %d \r\n",
                              estationdata[from].config.battery_volts, estationdata[from].config.Remote_sensors);

                estationconfigjson.printTo(msg);
                sprintf(pub_topics[4], "%s/%s/config", s_devname, estationdata[from].config.name);

                mqttClient.publish(pub_topics[4], 2, false, msg);

                // if (mqtt_client.publish(pub_topics[4], msg))
                // {

                //   //Subscribe to incommingConfig TOPIC for this station
                // }
                // else
                // {
                //   estationconfigjson.printTo(OldFiles[total_old_data].stored_to_send);
                //   OldFiles[total_old_data].station_address = from;
                //   OldFiles[total_old_data].valid = true;
                //   total_old_data++;
                //   Serial.println("MQTT not connected, storing station register:");

                //   // reconnect();
                //   // estationconfigjson.prettyPrintTo(Serial);
                // }

                sprintf(sub_topics[4], "%s/%s/incommingConfig", s_devname, estationdata[from].config.name);

                Serial.printf("Subscribe to: %s\r\n", sub_topics[4]);

                mensagem_out[0] = '\x05';
                Serial.println("Got connection request");

                estationdata[from].config.read_interval = 60000;

                memcpy(&mensagem_out[1], &estationdata[from], sizeof(Estation));
                // memcpy(&mensagem_out[sizeof(GW.gateway_std_data) + 1], &estationdata[from], sizeof(Estation));

                // while (radio_busy)
                // {
                //   Serial.println("Radio is busy! waiting to send ok to station ");
                //   delay(500);
                // }
                if (manager.sendtoWait(mensagem_out, sizeof(Estation) + 1, from))
                {
                  Serial.println("Connection request responded!");
                  // radio_busy = false;
                  total_stations++;

                  Serial.printf("Sended connection confirmation to %s with gateway Information...\r\n the read interval for this station is: %d \r\n",
                                estationdata[from].config.name, estationdata[from].config.read_interval);
                  radio_busy = false;
                }
                else
                {
                  Serial.println("message not delivered");
                  // manager.sendtoWait(mensagem_out, sizeof(Estation) + 1, from);
                  // radio_busy = false;
                }
              }
              else if (mensagem_in[CONNECTION_INDEX] == 0x06)
              {

                DynamicJsonBuffer estationbuffer;
                JsonObject &estationconfigjson = estationbuffer.createObject();
                Estation_position renew_position;

                memcpy(&renew_position, &mensagem_in[1], sizeof(renew_position));

                estationconfigjson["name"] = estationdata[from].config.name;

                estationconfigjson["Latitude"] = renew_position.gps_flat;
                estationconfigjson["Longitude"] = renew_position.gps_flon;
                estationconfigjson["Altitude"] = renew_position.gps_altitude;

                // estationconfigjson.prettyPrintTo(Serial);

                Serial.printf("Received position update from: %d with name: %s\r\n -Latitude reported: %f \r\n -Longitude reported: %f \r\n -Altitude reported: %f \r\n",
                              from, estationdata[from].config.name, renew_position.gps_flat, renew_position.gps_flon, renew_position.gps_altitude);
              }
              else
              {
                Serial.println("Got Data");
                // manager.sendtoWait((uint8_t *)"\x83", 1, from);
                DynamicJsonBuffer estationbuffer;

                JsonObject &estationjson = estationbuffer.createObject();

                estationdata[from].lasttimeseen = millis();
                estationdata[from].active = true;

                switch (mensagem_in[1])
                {
                case TUPPB:
                {
                  Serial.printf("--------------estation with temp, hum, pres, and rain--------\r\n");

                  memcpy(&IncommingSmall, &mensagem_in[2], sizeof(IncommingSmall));

                  estationdata[from].lasttimeseen = millis();
                  estationdata[from].active = true;

                  estationdata[from].temperature = IncommingSmall.temperature;
                  estationdata[from].humidity = IncommingSmall.humidity;
                  estationdata[from].pressure = IncommingSmall.pressure;
                  estationdata[from].battery_volts = IncommingSmall.battery_volts;

                  estationjson["local_address"] = from;
                  estationjson["signal_strengh"] = rf95.lastRssi();
                  estationjson["signal_noise_ratio"] = rf95.lastSNR();
                  estationjson["SUID"] = estationdata[from].config.name;
                  estationjson["latitude"] = estationdata[from].config.gps_flat;
                  estationjson["longitude"] = estationdata[from].config.gps_flon;

                  estationjson["temperatura"] = IncommingSmall.temperature;
                  estationjson["umidade"] = IncommingSmall.humidity;
                  estationjson["pressao"] = IncommingSmall.pressure;
                  estationjson["tensao_bateria"] = IncommingSmall.battery_volts;
                  estationjson["rain"] = IncommingSmall.rain;

                  estationjson["timestamp"] = IncommingSmall.timestamp;
                  // estationjson["sizeofmassage"] = estationjson.size();

                  estationjson.prettyPrintTo(Serial);
                  estationjson.printTo(msg, 512);

                  sprintf(pub_topics[4], "%s/%s/sensor", s_devname, estationdata[from].config.name);
                  // Serial.printf("topic: %s data: %s\r\n", pub_topics[4], msg);

                  mqttClient.publish(pub_topics[4], 2, false, msg);

                  if (estationdata[from].active == false || strlen(estationdata[from].config.name) == 0)
                  {
                    Serial.printf("Don't know the name of this device, lets check\r\n");
                    checkDeviceConfig(from);
                  }
                  break;
                }
                case TUPVDIRPBPGPS:
                {
                  DynamicJsonBuffer jsonBuffer;
                  JsonObject &root = jsonBuffer.createObject();

                  JsonObject &estation = root.createNestedObject("Station");

                  Serial.printf("-----------------------estation ------------------------------\r\n");
                  memcpy(&Incomming, &mensagem_in[2], sizeof(Incomming));
                  memcpy(&incommingSensor, &mensagem_in[2 + sizeof(Incomming)], sizeof(remoteSensors));

                  // estation["address"] = from;
                  // estation["lora_rssi"] = rf95.lastRssi();
                  // estation["name"] = estationdata[from].config.name;

                  estationdata[from].lasttimeseen = millis();
                  estationdata[from].active = true;

                  estationdata[from].temperature = Incomming.temperature;
                  estationdata[from].humidity = Incomming.humidity;

                  if (!isnan(Incomming.temperature))
                    estation["temperatura"] = Incomming.temperature;
                  if (!isnan(Incomming.humidity))
                    estation["umidade"] = Incomming.humidity;
                  if (!isnan(Incomming.pressure))
                    estation["pressao"] = Incomming.pressure;

                  if (!isnan(estationdata[from].config.gps_flat))
                    estation["latitude"] = estationdata[from].config.gps_flat;
                  if (!isnan(estationdata[from].config.gps_flon))
                    estation["longitude"] = estationdata[from].config.gps_flon;

                  estationdata[from].battery_volts = Incomming.battery_volts;

                  // estationdata[from].gps_flat = Incomming.gps_flat;
                  // estationdata[from].gps_flon = Incomming.gps_flon;
                  // estationdata[from].gps_altitude = Incomming.gps_altitude;

                  estation["local_address"] = from;
                  estation["signal_strengh"] = rf95.lastRssi();
                  estation["signal_noise_ratio"] = rf95.lastSNR();
                  estation["SUID"] = estationdata[from].config.name;
                  // estation["latitude"] = estationdata[from].config.gps_flat;
                  // estation["longitude"] = estationdata[from].config.gps_flon;
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

                  root.prettyPrintTo(Serial);
                  root.printTo(msg, 512);

                  DynamicJsonBuffer estationbuffer;
                  JsonObject &stationstorage = estationbuffer.createObject();

                  if (strlen(estationdata[from].config.name) > 0)
                  {

                    char filename[32];

                    sprintf(filename, "/readed_data/SD-%.2d%.2d%.2d-%s.json",
                            day(now()), month(now()), (year(now()) - 2000), estationdata[from].config.name);

                    String FileName = String(filename);
                    Serial.printf("filename to save - '%s'\r\n", FileName.c_str());

                    File estationFile = SD.open(FileName.c_str(), FILE_APPEND);

                    if (estationFile)
                    {

                      if (estationFile.size() <= 0)
                      {
                        estationFile.print("[");
                      }
                      else
                      {
                        estationFile.print(",");
                      }

                      estation.printTo(estationFile);
                      estationFile.close();
                    }
                    else
                    {
                      Serial.printf("ERROR ON FILE-  '%s', creating file!\r\n", FileName.c_str());
                    }
                  }

                  sprintf(pub_topics[4], "%s/%s/sensor", s_devname, estationdata[from].config.name);

                  mqttClient.publish(pub_topics[4], 2, false, msg);
                  // Serial.printf("topic: %s data: %s\r\n", pub_topics[4], msg);

                  if (estationdata[from].active == false || strlen(estationdata[from].config.name) == 0)
                  {
                    Serial.printf("Don't know the name of this device, lets check\r\n");
                    checkDeviceConfig(from);
                  }

                  break;
                }
                case TUPVDIRPBPGPS + 1:
                {
                  DynamicJsonBuffer jsonBuffer;
                  JsonObject &root = jsonBuffer.createObject();

                  JsonObject &estation = root.createNestedObject("estation");

                  JsonObject &remotesensors = root.createNestedObject("remoteSensor");

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
                  estation["name"] = estationdata[from].config.name;
                  // parseUnion(incomingdata, mensagem_in[INCOMING_SIZE], from, mensagem_in);

                  estationdata[from].lasttimeseen = millis();
                  estationdata[from].active = true;

                  estationdata[from].temperature = Incomming.temperature;
                  estationdata[from].humidity = Incomming.humidity;
                  estationdata[from].pressure = Incomming.pressure;
                  estationdata[from].battery_volts = Incomming.battery_volts;
                  //estationdata[from].gps_flat = Incomming.gps_flat;
                  //estationdata[from].gps_flon = Incomming.gps_flon;
                  //estationdata[from].gps_altitude = Incomming.gps_altitude;

                  estation["temperatura"] = Incomming.temperature;
                  estation["umidade"] = Incomming.humidity;
                  estation["pressao"] = Incomming.pressure;
                  estation["tensao_bateria"] = Incomming.battery_volts;
                  estation["windspeed"] = Incomming.windspeed;
                  estation["irradiation_in"] = Incomming.irradiation_in;
                  estation["winddir"] = Incomming.winddir;
                  estation["rain"] = Incomming.rain;
                  estation["timestamp"] = Incomming.timestamp;

                  root.prettyPrintTo(Serial);
                  root.printTo(msg, 512);
                  sprintf(pub_topics[4], "%s/%s/sensor", s_devname, estationdata[from].config.name);

                  if (estationdata[from].active == false || strlen(estationdata[from].config.name) == 0)
                  {
                    Serial.printf("Don't know the name of this device, lets check\r\n");
                    checkDeviceConfig(from);
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
          }
        }
        radio_busy = false;
      }
    }
    vTaskDelay(10);
  }
}
void send_or_store_mqtt_message(char *payload, char *topic, char *station)
{

  if (!mqtt_connected)
  {
    Serial.printf("MQTT is offline! Damn :( \r\n");
    if (strlen(estationdata[from].config.name) > 0)
    {
    }
    else
    {
      Serial.printf("MQTT is offline and also we don't know the name of this station \r\n");
    }
  }
  else
  {
    Serial.printf("Publishing on MQTT\r\n");
    mqttClient.publish(topic, 2, false, payload);
  }
}
void checkNewDevices()
{

  if (LoraConnected)
  {

    if (has_stations_to_check)
    {
      for (int i = 0; i < max_stations; i++)
      {
        if (!radio_busy)
        {
          if (stations_to_check[i].valid)
          {
            timed_send = millis();
            radio_busy = true;
            if (manager.sendtoWait((uint8_t *)"\x11", 1, i))
            {
              Serial.println("------------------------name test send ---------------------");
              stations_to_check[i].valid = false;
              radio_busy = false;
              vTaskDelay(10000);
            }
          }
        }
      }
      has_stations_to_check = false;
    }
    else
    {
      timed_send = millis();
      radio_busy = true;
      lora_config_modem(3);
      if (manager.sendtoWait((uint8_t *)"\x03", 1, 255))
      {
        Serial.println("------------------------Solicitation sent ---------------------");
      }
      else
      {
        Serial.println("Solicitation not delivered...");
        setupLora();
      }

      radio_busy = false;
    }
    lora_config_modem(5);
  }
}
void checkDeviceConfig(uint8_t address)
{

  stations_to_check[address].valid = true;
  has_stations_to_check = true;

  // if (LoraConnected)
  // {
  //   timed_send = millis();

  //   if (!radio_busy)
  //   {
  //     radio_busy = true;
  //     if (manager.sendtoWait((uint8_t *)"\x11", 1, address))
  //     {
  //       Serial.println("------------------------name test send ---------------------");
  //     }
  //     else
  //     {
  //       Serial.println("Solicitation not delivered...");
  //       setupLora();
  //     }

  //     radio_busy = false;
  //   }
  //   else
  //   {
  //     Serial.println("Radio is busy!");
  //   }
  // }
}
int renewConnection()
{
  int count, connected_stations = 0;
  for (count = 0; count < max_stations; count++)
  {
    if (millis() - estationdata[count].lasttimeseen > estationdata[count].config.read_interval)
    {
      estationdata[count].active = false;
    }
    if (estationdata[count].active == true)
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
    // rf95.setModemConfig(RH_RF95::Bw500Cr45Sf128);

    rf95.setModemConfig(RH_RF95::Bw500Cr48Sf64);
    // rf95.spiWrite(0x31, 0x05);
    // rf95.spiWrite(0x37, 0x0C);

    // rf95.setModemConfig(RH_RF95::Bw500Cr45Sf128);
    rf95.setFrequency(434);
    rf95.setPreambleLength(16);
    // return;
    // rf95.setCADTimeout(2000);
    Serial.println("LoRa up and running");
    LoraConnected = true;
    // return;
  }
}
void automaticStationDataRate()
{

  digitalWrite(2, LOW);
  delay(1000);
  digitalWrite(2, HIGH);
  delay(1000);

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
    rf95.setModemConfig(RH_RF95::Bw31_25Cr48Sf512);
    // rf95.setModemConfig(RH_RF95::Bw500Cr45Sf128);
    rf95.setFrequency(434);
    rf95.setPreambleLength(16);
    // return;
    // rf95.setCADTimeout(2000);
    Serial.println("LoRa up and running");
    // return;
  }
}
void lora_config_modem(int x)
{
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
    //Serial1.println("cfg 2");
    // Station.station_std_data.lora_connection_mode = 2;
    // Station.saveConfig();
    break;
  }
  case 3:
  {
    rf95.setModemConfig(RH_RF95::Bw31_25Cr48Sf512);
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
  case 5:
  {
    rf95.setModemConfig(RH_RF95::Bw500Cr48Sf64);
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
//       JsonObject &root = jsonBuffer.parseObject(buf.get());
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

bool savewificonfig()
{
  DynamicJsonBuffer jsonBuffer;
  // status_LED = NETWORK_CONNECTING;
  // file.close();

  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  Serial.print("founded network:");
  Serial.println(n);
  // File wifi_file = SD.open(wifi_config_file, FILE_WRITE);
  // wifi_file.close();

  bool known_network = true;
  // File filerd = SD.open(wifi_config_file, FILE_READ);

  if (!SD.exists(wifi_config_file))
  {

    Serial.printf("No %s file, creating file and waiting...\r\n", wifi_config_file);
    File wifi_file = SD.open("/configuration_files/wificonfig.json", FILE_WRITE);
    if (wifi_file)
    {
      Serial.println("Saving first config on the file!");
      DynamicJsonBuffer jsonBuffer;
      JsonObject &root = jsonBuffer.createObject();
      JsonArray &SSID = root.createNestedArray("SSID");
      JsonArray &PSK = root.createNestedArray("PSK");

      SSID.add("abc123");
      PSK.add("keinlexy");

      root.prettyPrintTo(Serial);
      root.printTo(wifi_file);

      wifi_file.close();

      savewificonfig();
    }
  }
  else
  {
    File filerd = SD.open(wifi_config_file, FILE_READ);
    size_t size = filerd.size();
    if (size == 0)
    {
      Serial.println("History file empty ! Writing the first data");

      DynamicJsonBuffer jsonBuffer;
      JsonObject &root = jsonBuffer.createObject();
      File filerd = SD.open(wifi_config_file, FILE_WRITE);
      if (filerd)
      {
        DynamicJsonBuffer jsonBuffer;
        JsonObject &root = jsonBuffer.createObject();
        JsonArray &SSID = root.createNestedArray("SSID");
        JsonArray &PSK = root.createNestedArray("PSK");

        //JsonArray &SSID = root["SSID"];
        SSID.add("abc123");
        SSID.add("Casa 2.4");
        //JsonArray &PSK = root["PSK"];
        PSK.add("teste");
        PSK.add("keinlexy");

        root.printTo(Serial);
        root.printTo(filerd);
      }
    }
    else
    {
      std::unique_ptr<char[]> buf(new char[size]);
      filerd.readBytes(buf.get(), size);
      JsonObject &root = jsonBuffer.parseObject(buf.get());
      if (!root.success())
      {
        Serial.println("Impossible to read JSON file");
        SD.remove(wifi_config_file);
        savewificonfig();
      }
      else
      {
        //Serial.println("Redes conhecidas...");

        int arraySize = root["SSID"].size();

        for (int i = 0; i < arraySize; i++)
        {
          const char *ccSSID = root["SSID"][i];
          const char *ccPSK = root["PSK"][i];

          String SSID = root["SSID"][i];
          String PSK = root["PSK"][i];

          Serial.print("Rede: ");
          Serial.print(SSID);
          Serial.print(" -- Passwd: ");
          Serial.println(PSK);
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

                // WiFi.setHostname("gateway");
                // WiFi.mode(WIFI_STA);
                // WiFi.reconnect();

                // WiFi.setSleep(false); // <--- this command disables WiFi energy save mode and eliminate connected():

                vTaskDelay(10);
                Serial.print(".");
                if (count > 6000)
                {
                  Serial.printf("Unable to connect, restarting...");

                  ESP.restart();
                }
                count++;
              }
              if (status_LED == WIFI_CONNECTED)
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
  setupWiFi();
}
void setupWiFi()
{
  Serial.println("setupWiFi: init...");
  DynamicJsonBuffer jsonBuffer;

  if (WiFi.status() != WL_CONNECTED)
  {

    WiFi.mode(WIFI_AP_STA);

    status_LED = WIFI_WAITING_PASSWD;

    WiFi.beginSmartConfig();

    if (!SD.exists(wifi_config_file))
    {
      Serial.println("setupWiFi: no wificonfig2.json doesn't exist");
    }
    else
    {
      File filessid = SD.open(wifi_config_file, FILE_READ);
      size_t size = filessid.size();
      if (size == 0)
      {
        Serial.println("History file empty !");

        std::unique_ptr<char[]> buf(new char[size]);
        filessid.readBytes(buf.get(), size);
        JsonObject &root = jsonBuffer.parseObject(buf.get());

        while (!WiFi.smartConfigDone())
        {
          // delay(500);
          //Serial.print(".");
        }
        while (WiFi.status() != WL_CONNECTED)
        {
          vTaskDelay(500);
          Serial.print(".");
          Status = WIFI_CONNECTED;
        }

        if (!SD.exists(wifi_config_file))
        {
          Serial.println("No History Exist");
        }
        else
        {
          File filedata = SD.open(wifi_config_file, FILE_WRITE);
          Serial.println("Open file ok...");

          JsonArray &SSID = root.createNestedArray("SSID");
          JsonArray &PSK = root.createNestedArray("PSK");

          //JsonArray &SSID = root["SSID"];
          SSID.add(WiFi.SSID());
          //JsonArray &PSK = root["PSK"];
          PSK.add(WiFi.psk());

          root.printTo(Serial);
          root.printTo(filedata);
        }
      }
      else
      {
        std::unique_ptr<char[]> buf(new char[size]);
        filessid.readBytes(buf.get(), size);
        JsonObject &root = jsonBuffer.parseObject(buf.get());

        if (!root.success())
        {
          Serial.println("Impossible to read JSON file");
        }
        else
        {
          Serial.println(F("Ok, waiting for smartConfigDone to save the SSID do file"));
          int timeout_counter = 0;
          while (!WiFi.smartConfigDone())
          {
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
          //

          int arraySize = root["SSID"].size();

          File filedata = SD.open(wifi_config_file, FILE_WRITE);
          if (!filedata)
          {
            Serial.println("No History Exist");
          }
          else
          {
            JsonArray &SSID = root["SSID"];
            SSID.add(WiFi.SSID());
            JsonArray &PSK = root["PSK"];
            PSK.add(WiFi.psk());

            root.printTo(Serial);
            root.printTo(filedata);
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
        }
      }
    }
  }
  else
  {
    // reconnect();
  }
}
// void reconnect()
// {
//   if (WiFi.status() == WL_CONNECTED)
//   {
//     status_LED = MQTT_CONNECTING;
//     if (!mqtt_client.connected())
//     {
//       Serial.println("Attempting MQTT connection...");
//       // Create a random client ID
//       String clientId = String("Node");
//       clientId += String(random(0xffffffff), HEX);
//       // Serial.printf("number of reconnect: %d\r\n", number_of_reconnects);
//       // Serial.println(clientId.c_str());
//       number_of_reconnects++;
//       // Attempt to connect
//       if (mqtt_client.connect(clientId.c_str()))
//       {
//         Serial.println("connected");
//         number_of_reconnects = 0;

//         DynamicJsonBuffer jsonBuffer;
//         JsonObject &config = jsonBuffer.createObject();

//         config["Latitude"] = GW.gateway_std_data.stored_lat;
//         config["Longitude"] = GW.gateway_std_data.stored_lon;
//         //GW.gateway_std_data.stored_alt = gps.f_altitude();
//         config["lastconnection"] = now();
//         String date = String(day(now()));
//         String hour_s = String(hour(now()));
//         config["lastconnection_date"] = String(date + "/" + month(now()) + "/" + year(now()));
//         config["lastconnection_time"] = String(hour_s + ":" + minute(now()) + ":" + second(now()));

//         config["LocalIP"] = String(WiFi.localIP().toString());
//         config["externalIP"] = externalIP;

//         config.printTo(msg);
//         // config.prettyPrintTo(Serial);

//         mqtt_client.publish(pub_topics[0], msg, true);

//         //Verifica se há qualquer mensagem ainda não publicada a ser publicada no MQTT
//         CheckOldMessages();

//         // Once connected, publish an announcement...
//         //client.publish("outTopic", "hello world");
//         // ... and resubscribe

//         mqtt_client.subscribe(sub_topics[0]);
//         mqtt_client.subscribe(sub_topics[1]);
//         mqtt_client.subscribe(sub_topics[2]);
//         mqtt_client.subscribe(sub_topics[3]);
//         mqtt_client.subscribe(sub_topics[4]);

//         status_LED = MQTT_CONNECT;
//       }
//       else
//       {

//         Serial.print("failed, rc=");
//         Serial.print(mqtt_client.state());
//         Serial.println(" try again in 5 seconds");
//         // reconnect();
//         // delay(5000);
//         if (number_of_reconnects > max_reconnect)
//         {
//           Serial.println("Connecting to Wifi again");

//           // savewificonfig();
//           // ESP.restart();
//           number_of_reconnects = 0;
//         }
//       }
//     }
//   }
// }
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
void execOTA(String host)
{
  Serial.println("Connecting to: " + String(host));
  // Connect to S3
  if (Client.connect(host.c_str(), port))
  {
    // Connection Succeed.
    // Fecthing the bin
    Serial.println("Fetching Bin: " + String(bin));

    // Get the contents of the bin file
    Client.print(String("GET ") + bin + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Cache-Control: no-cache\r\n" +
                 "Connection: close\r\n\r\n");

    // Check what is being sent
    //    Serial.print(String("GET ") + bin + " HTTP/1.1\r\n" +
    //                 "Host: " + host + "\r\n" +
    //                 "Cache-Control: no-cache\r\n" +
    //                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (Client.available() == 0)
    {
      if (millis() - timeout > 5000)
      {
        Serial.println("Client Timeout !");
        Client.stop();
        return;
      }
    }
    // Once the response is available,
    // check stuff

    /*
         Response Structure
          HTTP/1.1 200 OK
          x-amz-id-2: NVKxnU1aIQMmpGKhSwpCBh8y2JPbak18QLIfE+OiUDOos+7UftZKjtCFqrwsGOZRN5Zee0jpTd0=
          x-amz-request-id: 2D56B47560B764EC
          Date: Wed, 14 Jun 2017 03:33:59 GMT
          Last-Modified: Fri, 02 Jun 2017 14:50:11 GMT
          ETag: "d2afebbaaebc38cd669ce36727152af9"
          Accept-Ranges: bytes
          Content-Type: application/octet-stream
          Content-Length: 357280
          Server: AmazonS3

          {{BIN FILE CONTENTS}}
      */
    while (Client.available())
    {
      // read line till /n
      String line = Client.readStringUntil('\n');
      // remove space, to check if the line is end of headers
      line.trim();

      // if the the line is empty,
      // this is end of headers
      // break the while and feed the
      // remaining `Client` to the
      // Update.writeStream();
      if (!line.length())
      {
        //headers ended
        break; // and get the OTA started
      }

      // Check if the HTTP Response is 200
      // else break and Exit Update
      if (line.startsWith("HTTP/1.1"))
      {
        if (line.indexOf("200") < 0)
        {
          Serial.println("Got a non 200 status code from server. Exiting OTA Update.");
          break;
        }
      }

      // extract headers here
      // Start with content length
      if (line.startsWith("Content-Length: "))
      {
        contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
        Serial.println("Got " + String(contentLength) + " bytes from server");
      }

      // Next, the content type
      if (line.startsWith("Content-Type: "))
      {
        String contentType = getHeaderValue(line, "Content-Type: ");
        Serial.println("Got " + contentType + " payload.");
        if (contentType == "application/octet-stream")
        {
          isValidContentType = true;
        }
      }
    }
  }
  else
  {
    // Connect to S3 failed
    // May be try?
    // Probably a choppy network?
    Serial.println("Connection to " + String(host) + " failed. Please check your setup");
    // retry??
    // execOTA();
  }

  // Check what is the contentLength and if content type is `application/octet-stream`
  Serial.println("contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));

  // check contentLength and content type
  if (contentLength && isValidContentType)
  {
    // Check if there is enough to OTA Update
    bool canBegin = Update.begin(contentLength);

    // If yes, begin
    if (canBegin)
    {
      Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
      // No activity would appear on the Serial monitor
      // So be patient. This may take 2 - 5mins to complete
      size_t written = Update.writeStream(Client);

      if (written == contentLength)
      {
        Serial.println("Written : " + String(written) + " successfully");
      }
      else
      {
        Serial.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
        // retry??
        // execOTA();
      }

      if (Update.end())
      {
        Serial.println("OTA done!");
        if (Update.isFinished())
        {
          Serial.println("Update successfully completed. Rebooting.");
          ESP.restart();
        }
        else
        {
          Serial.println("Update not finished? Something went wrong!");
        }
      }
      else
      {
        Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      }
    }
    else
    {
      // not enough space to begin OTA
      // Understand the partitions and
      // space availability
      Serial.println("Not enough space to begin OTA");
      Client.flush();
    }
  }
  else
  {
    Serial.println("There was no content in the response");
    Client.flush();
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
int Publish_AWS(char *topic, char *payload, int retry_time, int times_to_retry)
{
  bool status = false;
  int retry_times = 0;
  int in_time = millis();

  while (status == false)
  {
    if (awsClient.publish(topic, payload) == 0)
    {
      Serial.print("Publish Message:");
      Serial.println(payload);
      status = true;
      return 0x01;
    }
    else
    {
      while (retry_times < times_to_retry)
      {
        if (in_time + retry_time > millis())
        {

          if (awsClient.publish(topic, payload) == 0)
          {
            Serial.print("Publish Message:");
            Serial.println(payload);
            status = true;
            return 0x01;
          }
          else
          {
            in_time = millis();
            retry_times++;
            Serial.println("No success on retry...");
            status = true;
            return 0x00;
          }
        }
      }
    }
  }
}
bool file = false;
void saveConfig()
{

  //Serial.println("file opened...");

  File configfile = SD.open("/configuration_files/config.json", FILE_WRITE);

  if (configfile)
  {
    StaticJsonBuffer<500> jsonBuffer;
    JsonObject &config = jsonBuffer.createObject();

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

    config.printTo(configfile);
    //config.printTo(to_file);
    //Serial.print("to print: ");
    // Serial.println(to_file);

    // config.prettyPrintTo(Serial);
    // Serial.println("New position saved on file");
  }
  else
  {
    Serial.println("unable to open file");
  }
  configfile.close();
}
void loadConfig()
{

  File configfile = SD.open("/configuration_files/config.json", FILE_READ);
  if (configfile)
  {
    size_t size = configfile.size();
    std::unique_ptr<char[]> buf(new char[size]);
    configfile.readBytes(buf.get(), size);

    DynamicJsonBuffer jsonBuffer;

    JsonObject &config = jsonBuffer.parseObject(buf.get());

    if (!config.success())
    {
      Serial.print("fail to parse");
      SD.remove("/configuration_files/config.json");
      saveConfig();
      loadConfig();
    }
    else
    {

      GW.gateway_std_data.stored_lat = config["flat"];
      GW.gateway_std_data.stored_lon = config["flon"];
      GW.gateway_std_data.stored_alt = config["altitude"];
      strcpy(mqtt_server, config["server_address"]);
      //mqtt_server = ;
      // config["lastfix"] = timeClient.getEpochTime();

      // config["connected_stations"] = connected_stations;
      // config["heap"] = ESP.getFreeHeap();
      // config["Wifi_SSID"] = WiFi.SSID();
      // config["Wifi_RSSI"] = WiFi.RSSI();
      // config["Time_Connected"] = millis() / 1000;

      Serial.print("data from config file: ");
      // config.prettyPrintTo(Serial);
      config.printTo(configfile);
    }
  }
  else
  {
    Serial.println("unable to open file");
  }
  configfile.close();
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
void downloadFile(char *host, char *file_name)
{
  HTTPClient http;
  File f = SD.open(file_name, FILE_WRITE);

  if (f)
  {
    http.begin(host);
    int httpCode = http.GET();
    int len = http.getSize();
    received_size = len;

    Serial.printf("--------------------------------------------downloading to file with size %d \n", len);

    if (httpCode > 0)
    {
      if (httpCode == HTTP_CODE_OK)
      {
        uint8_t buff[4096] = {0};

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
            f.write(buff, c);
            vTaskDelay(1);
            if (len > 0)
            {
              len -= c;
              actual_received_size += c;
            }
          }
        }
      }
    }
    else
    {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
    f.close();

    File f = SD.open(file_name, FILE_READ);
    int size = f.size();
    f.close();
    Serial.printf("--------------------------------------------Done Downloading, the file size is : %d\n", size);
  }
}

DynamicJsonBuffer folderlistbuffer;
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

  JsonObject &dirjson = folderlistbuffer.createObject();
  // folder_name = String(file.name());
  dirjson["name"] = folder_name;
  dirjson["type"] = "folder";
  dirjson["size"] = (float)file.size() / 1000;

  JsonArray &content = dirjson.createNestedArray("content");

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

      file_name = String(file.name());
      int index_of_foldername = file_name.indexOf(folder_name);
      int folder_name_size = folder_name.length();

      file_name.remove(index_of_foldername, folder_name_size);

      JsonObject &filejson = folderlistbuffer.createObject();

      filejson["name"] = file_name;
      filejson["type"] = "file";
      filejson["size"] = (float)file.size() / 1000;

      content.add(filejson);

      Serial.printf(" file inside a folder name: %s \r\n", file_name.c_str());
    }
    file = root.openNextFile();
  }

  jarray.add(dirjson);
  Serial.println("Out a folder --------------------------------------------------------------------------------");
  Serial.print("  Exiting DIR : ");
  Serial.println(folder_name);
  folder_name = String("");
}
// void updateFromFS(fs::FS &fs) {
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
    
//       // whe finished remove the binary from sd card to indicate end of the process
//       fs.remove("/update.bin");      
//    }
//    else {
//       Serial.println("Could not load update.bin from sd root");
//    }
// }