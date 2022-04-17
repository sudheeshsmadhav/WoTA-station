//update as on 04042022
//serially scanning and posting ble info, machine data in parallel task

/* periodical cleaning at the hardware is required for the accuracy on control signals(Machine data)
- Weft sensor -   sensing part is dirty means, the weft break signal will start to blink, this is byepassed in 
                  WoTA station code, hence if the sensor is dirty t may consider manual break as weft(at random cases)
                  */
                 //updated to github - version
//---------------------------------------------------------------------------------------------
#include <Arduino.h>        //for vscode-PIO
#include <WiFi.h>           
#include <PubSubClient.h>
#include <ESP32Time.h>      // local Time 
#include <WiFiUdp.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
// #include <Preferences.h>
// Preferences preferences;
//----------------------------------------------------------------------------------------------
//stationId 21 - 26, 41, 43 and 45 are for 9x9 model
const char    *stationId      =  "STATION43";
int           beaconScanTime  =  4;                 //scan time - MAKE 6S AS DEFAULT TIME - 3s on 31.03.2022
int           serial_baud     =  115200;
//----------------------------------------------------------------------------------------------
#undef        MQTT_MAX_PACKET_SIZE
#define       MQTT_MAX_PACKET_SIZE 8000             //can vary if required
uint8_t       message_char_buffer[MQTT_MAX_PACKET_SIZE];
uint8_t       message_char_buffer1[MQTT_MAX_PACKET_SIZE];
// const char    *WIFI_SSID      =  "TP-Link_FA9A";
// const char    *WIFI_PASSWORD  =  "78126718";
const char    *WIFI_SSID      =  "WOTA-1";
const char    *WIFI_PASSWORD  =  "admin@123";
const char    *MQTT_HOST      =  "192.168.43.128";  //host ip -- server
const int     MQTT_PORT       =  1883;
const char    *MQTT_USER      =  "";                //empty
const char    *MQTT_PASSWORD  =  "";
const char    *L_Time         =  "/wota/localtime"; // topic to subscribe
const char    *topic          =  "/nodejs/mqtt/";  
const char    *topic_log      =  "/nodejs/mqtt";  
const char    *ec             =  "/ec/";

//----------------------------------------------------------------------------------------------

String        stationId_s(stationId);

String        topic_s(topic);
String        mqtt_topic_s    =  topic_s + stationId_s;
const char    *mqtt_topic     =  mqtt_topic_s.c_str(); //topic to publish - /nodejs/mqtt/STATIONX

String        ec_s(ec);
String        mData_s         = ec_s + stationId_s;
const char    *mData          = mData_s.c_str();      // topic to publish mData - /ec/stationId
//----------------------------------------------------------------------------------------------
uint32_t      upload_count    =  0;
String        tagData         =  "";
int           tagIndex        =  0;
String        tagIds[20];
int           rssiIds[20];
char          Rx_data[10];
WiFiClient    client;
PubSubClient  mqttClient(client);     //client
int           RSSI_bt;
ESP32Time     rtc;
BLEScan*      pBLEScan;
String        formattedDate;
TaskHandle_t  BT_Task;
uint32_t      wifi_status     =   0;
uint32_t      mData_status    =   0;
//TaskHandle_t  EC_Task;
//----------------------------------------------------------------------------------------------
// //for custom dot pcb
// #define       manual_in         25 //esp pins to read machine error codes
// #define       weft_in           26
// #define       warp_in           27
// #define       doffing_in        18 // can be used for rpm  as we are not using this as error signal.
//-----------------------------------------------------------------------------------------------
// for the new board1 only
#define       manual_in         18 //esp pins to read machine error codes
#define       weft_in           26
#define       warp_in           27
#define       doffing_in        25 // can be used for rpm  as we are not using this as error signal.

#define       manual_out        34//esp pins to display mData
#define       weft_out          35
#define       warp_out          32
#define       doffing_out       33

#define       wifi_flag         19
#define       mData_flag        21

int           manual;
int           weft;
int           warp;
int           doffing;
long          rpm;
//---------error codes--------------------------------------------------------------------------
int           ecmanual        =  0;   //manual/emergency stop
int           ecweft          =  1;   //weft
int           ecwarp          =  2;   //warp
int           running         = -1;   //no error
int           errorCode;
//----------------------------------------------------------------------------------------------

void addTag(String tagId, int rssiId)
{
  bool isfound = false;
  for (int i = 0; i <= tagIndex; i++)
  {
    if (tagIds[i] == tagId)
    {
      isfound = true;
      if (rssiIds[i] < rssiId)
      {
        rssiIds[i] = rssiId;
      }
    }
  }
  if (!isfound)
  {
    // Serial.printf("Add device: %s %d\n", String(tagId), rssiId);
    tagIds[tagIndex] = tagId;
    rssiIds[tagIndex] = rssiId;
    tagIndex = tagIndex + 1;
  }
}
//----------------------------------------------------------------------------------------------

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    // Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
    if (advertisedDevice.haveRSSI())
    {
      addTag(advertisedDevice.getAddress().toString().c_str(), advertisedDevice.getRSSI());
    }
  }
};
//----------------------------------------------------------------------------------------------

void machine_data()
{
//   manual = digitalRead(manual_in);
//   weft = digitalRead(weft_in);
//   warp = digitalRead(warp_in);
//   doffing = digitalRead(doffing_in);

  int manual1 = digitalRead(manual_in);
  int weft1 = digitalRead(weft_in);
  int warp1 = digitalRead(warp_in);
  delay(500);

  int manual2 = digitalRead(manual_in);
  int weft2 = digitalRead(weft_in);
  int warp2 = digitalRead(warp_in);
  delay(500); 

  int manual3 = digitalRead(manual_in);
  int weft3 = digitalRead(weft_in);
  int warp3 = digitalRead(warp_in);
  delay(500); 

  int manual4 = digitalRead(manual_in);
  int weft4 = digitalRead(weft_in);
  int warp4 = digitalRead(warp_in);
  

 manual = manual1 * manual2 * manual3 * manual4;
 weft =  weft1 * weft2 * weft3 * weft4;
 warp = warp1 * warp2 * warp3 * warp4; // arithmetic operation to bypass hardware toggling at loom
 digitalWrite(mData_flag, mData_status);
  // digitalWrite(manual_out, digitalRead(manual_in));
  // digitalWrite(weft_out, digitalRead(weft_in));
  // digitalWrite(warp_out, digitalRead(warp_in));
  // digitalWrite(doffing_out, digitalRead(doffing_in));

  String payloadString1 = "{\'stationId\':";
  payloadString1 += "'";
  payloadString1 += String(stationId);
  payloadString1 += "'";
  payloadString1 += ",";
  payloadString1 += "\'time\':";
  payloadString1 += "'";
  time_t now;
  // struct tm ts;
  // char buf[80];
  // time(&now);
  // ts = *localtime(&now);
  // strftime(buf, sizeof(buf), "%Y-%m-%d %H: %M: %S", &ts);
  // //payloadString1 += String(rtc.getTime("%Y-%m-%d"))+"T"+String(rtc.getTime("%H:%M:%S"))+"Z";
  payloadString1 += time(&now);
  payloadString1 += "'";
  payloadString1 += ",";
  payloadString1 += "\'errorCode\':";

  //----------analysing machine data----------------------------------------------------------
  
  if (manual == 0)
  {
    if (manual == 0 && warp == 1 && weft == 1)
    {
      // delay(1000);
      // if(manual == 0 && warp == 1 && weft == 1) // commented -  machine toggling bypassed.
      // {
      errorCode = ecmanual;
      // }
      
      
    }
//-----------------------------------------------------------------------
    if (manual == 0 && weft == 0 && warp == 1)
    {
      // delay(1000);
      // if (manual == 0 && weft == 0 && warp == 1) // commented -  machine toggling bypassed.
      // {
      // errorCode = ecweft;
      // }
      // if(manual == 0 && weft == 0 && warp == 0)
      // {
      errorCode = ecweft;
      // }
      
    }
//------------------------------------------------------------------------------
    if (manual == 0 && warp == 0 && weft == 1)
    {
      // delay(1000);
      // if(manual == 0 && warp == 0 && weft == 1)  // commented -  machine toggling bypassed.
      // {
      // errorCode = ecwarp;
      // }
      // if(manual == 0 && warp == 0 && weft == 0)
      // {
      errorCode = ecwarp;
      // }
    }

    // if (manual == 0 && warp == 0 && weft == 0)
    // {
    //   delay(1000);
    //   if(manual == 0 && warp == 0 && weft == 1)
    //   {
    //     if (manual == 0 && warp == 0 && weft == 1) // added manual march 29
    //     {
    //     errorCode = ecwarp;
    //     }
    //     if (manual == 0  && warp == 0 && weft == 0) //added manual march 29
    //     {
    //       errorCode = ecwarp;
    //     }
    //   }

    // }
    
  }

  else //if manual ==1
  {
    delay(1000);
    Serial.println("machine is running");
    errorCode = running;
  }
  //--------------------------------------------------------------------------------------------
  payloadString1 += String(errorCode);
  // payloadString1 += ",";
  // payloadString1 += "\'rpm\':";
  // payloadString1 += String(rpm);
  payloadString1 += "}";
  Serial.println(payloadString1);
  payloadString1.getBytes(message_char_buffer1, payloadString1.length() + 1);
  boolean result = mqttClient.publish_P(mData, message_char_buffer1, payloadString1.length(), false);
  if (!result)
    Serial.println("machine data not published");
  ESP.getFreeHeap();
  mData_status =  ~mData_status;
}

void ble_upload()
{
  String payloadString = "{\'stationId\':";
  payloadString += "'";
  payloadString += String(stationId);
  payloadString += "'";
  payloadString += ",";
  payloadString += "\'time\':";
  payloadString += "'";
  payloadString += String(rtc.getTime("%Y-%m-%d")) + "T" + String(rtc.getTime("%H:%M:%S")) + "Z";
  payloadString += "'";
  payloadString += ",";
  payloadString += "\'tags\':[";

  for (int i = 0; i <= tagIndex; i++)
  {
    String payload;
    String tagId = tagIds[i];
    int rssiId = rssiIds[i];
    payload += "{";
    payload += "\'tagId\':";
    payload += "'";
    payload += String(tagId);
    payload += "'";
    payload += ",";
    payload += "\'rssi\':";
    payload += String(rssiId);
    payload += "}";
    tagData += payload;
    if (i < tagIndex - 1)
    {
      tagData += ", ";
    }
    RSSI_bt = rssiId;
  }
  payloadString += tagData;
  //------------------------------------------------------------------------------------------

  String payloadString_new = payloadString.substring(0, payloadString.length() - 21); //removing empty array
  payloadString_new += "]";
  payloadString_new += "}";
  if (payloadString_new.length() > 100) // neglecting payload string doesn't has ble info. 
  {
    Serial.println(payloadString_new);
    payloadString_new.getBytes(message_char_buffer, payloadString_new.length() + 1);
    boolean result = mqttClient.publish_P(mqtt_topic, message_char_buffer, payloadString_new.length(), false);
    Serial.print("PUB Result: ");
    Serial.println(result);
    if (result)
    {
      upload_count++;
    }
    Serial.print("number of uploads: ");
    Serial.println(upload_count);
    Serial.println();
  }
}

void ScanBeacons()
{
  BLEScan *pBLEScan = BLEDevice::getScan(); // create new scan
  MyAdvertisedDeviceCallbacks cb;
  pBLEScan->setAdvertisedDeviceCallbacks(&cb);
  pBLEScan->setActiveScan(true); // active scan uses more power, but get results faster
  BLEScanResults foundDevices = pBLEScan->start(beaconScanTime);
  // Stop BLE
  pBLEScan->stop();
  pBLEScan->clearResults();
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.print("] :");
  Serial.println();
  for (int i = 0; i < length; i++)
  {
    Rx_data[i] = (char)payload[i];
  }
  Serial.print(Rx_data);
  Serial.print("\t");
  Serial.print("CORE: ");
  Serial.print(xPortGetCoreID());
  Serial.println();
  rtc.setTime(atol(Rx_data));
  Serial.println("Time sync with server is completed");
}

void BT_Task_code(void *pvParameters)
{
  // Serial.print("BT_Task running on core ");
  // Serial.println(xPortGetCoreID());
  for (;;)
  {
      while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ESP.restart(); // if not connected retry by restarting EP32 after 2seconds.
  }
    digitalWrite(wifi_flag, wifi_status);
    Serial.println("Scanning nearby BLE beacons");
    ScanBeacons();
    ble_upload();

    RSSI_bt   = 0;
    tagData   = "";
    tagIndex  = 0;
    memset(tagIds, 0, sizeof(tagIds));
    memset(rssiIds, 0, sizeof(rssiIds));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    wifi_status = ~wifi_status;
  }
  
}

void setup()
{
  Serial.begin(serial_baud);
  Serial.println("-----------WoTA Station---------------");
  Serial.print("Station Name: ");
  Serial.println(stationId);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // preferences.begin("esp32_wota" , false);
  // unsigned int counter = preferences.getUInt("counter", 0);
  // counter++;
  // Serial.printf("restart count: %u\n", counter);
  // preferences.putUInt("counter", counter);
  // preferences.end();
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(1000);
    ESP.restart(); // if not connected retry by restarting EP32 after 2seconds.
  }
  Serial.println();
  Serial.print("Connected to Wi-Fi : ");
  Serial.print(WIFI_SSID);
  Serial.print("\t");
  Serial.print("WiFi_RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.print("\t");
  Serial.print("\t");
  Serial.print("Working_CORE: ");
  Serial.print(xPortGetCoreID());
  Serial.println();
  Serial.print("MQTT Server: ");
  Serial.println(MQTT_HOST);
  Serial.println();
  BLEDevice::init("");
  delay(1500);

  xTaskCreatePinnedToCore(
      BT_Task_code, /* Task function. */
      "BT_Task",    /* name of task. */
      30000,        /* Stack size of task */
      NULL,         /* parameter of the task */
      1,            /* priority of the task */
      &BT_Task,     /* Task handle to keep track of created task */
      0);           /* pin task to core 0 */

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(callback);

  while (!client.connected())
  {
    if (mqttClient.connect(stationId, MQTT_USER, MQTT_PASSWORD))
    {
      Serial.println("Connected to MQTT broker");
    }
    else
    {
      delay(500);
      Serial.print(".");
    }
  }
  mqttClient.subscribe(L_Time);
  pinMode(manual_in, INPUT_PULLUP);
  pinMode(weft_in, INPUT_PULLUP);
  pinMode(warp_in, INPUT_PULLUP);
  pinMode(doffing_in, INPUT_PULLUP);

  pinMode(manual_out, OUTPUT);
  pinMode(weft_out, OUTPUT);
  pinMode(warp_out, OUTPUT);
  pinMode(doffing_out, OUTPUT);
  pinMode(wifi_flag, OUTPUT);
  pinMode(mData_flag, OUTPUT);
  Serial.println("initialization completed");
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(stationId))
    {
      Serial.println("connected");
      // Subscribe
      mqttClient.subscribe(L_Time);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
    }
  }
}

void loop()
{
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Restarted  - No WiFI available");
    // Serial.println(info.disconnected.reason);
    delay(1000);
    ESP.restart();
  }
  if (!client.connected())
  {
    reconnect();
  }    
  mqttClient.loop();
  
  if (client.connected())
  {
    machine_data();
  }
  
}
//---------------------------------- end of stationId-----------------------------------------------------