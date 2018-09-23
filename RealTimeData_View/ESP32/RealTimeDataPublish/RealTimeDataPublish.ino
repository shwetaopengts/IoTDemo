/*
  PubNub sample JSON-parsing client with WiFi support

  This combines two sketches: the PubNubJson example of PubNub library
  and the WifiWebClientRepeating example of the WiFi library.

  This sample client will properly parse JSON-encoded PubNub subscription
  replies using the aJson library. It will send a simple message, then
  properly parsing and inspecting a subscription message received back.

  This is achieved by integration with the aJson library. You will need
  a version featuring Wiring Stream integration, that can be found
  at http://github.com/pasky/aJson as of 2013-05-30.

  Circuit:
  * Wifi shield attached to pins 10, 11, 12, 13
  * (Optional) Analog sensors attached to analog pin.
  * (Optional) LEDs to be dimmed attached to PWM pinsmy 8 and 9.


  Please refer to the PubNubJson example description for some important
  notes, especially regarding memory saving on Arduino Uno/Duemilanove.
  You can also save some RAM by not using WiFi password protection.


  created 30 May 2013
  by Petr Baudis

  https://github.com/pubnub/pubnub-api/tree/master/arduino
  This code is in the public domain.
  */

/**
 * This is programme given by PubNub. Modified slightly for Demo.
 */
 
#include <SPI.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#define PubNub_BASE_CLIENT WiFiClient
#include <PubNub.h>
#include "DHTesp.h"
#include <aJSON.h>

/**
 * Put your Wifi Configurations here.
 */

static char ssid[] = "iPhone";        // your network SSID (name)
static char pass[] = "Geosense123";   // your network password


static int keyIndex = 0;              // your network key Index number (needed only for WEP)


/**
 * This is the Configuratin done on PubNub Website
 */
const static char pubkey[] = "pub-c-297a57ea-72a4-4d73-9ef6-41b0033f4f44";
const static char subkey[] = "sub-c-6736acd8-bc13-11e8-b416-8238ea107fd1";
const static char channel[] = "realtimepubsub";

StaticJsonBuffer<200> jsonBuffer;
/**
 * This is For Reading the DHT22 Temp and Humidity Sensor Modules
 * Data Pin for DHT22 Module is 27 and hence below DHTPIN is defined
 * as 27
 */
 
DHTesp dht;
#define DHTPIN 27
#define DHTTYPE DHT22
float temperature = 0;
float humidity = 0;


JsonObject& root = jsonBuffer.createObject();
unsigned long check_wifi = 30000;

/*LED GPIO pin*/
const char led = 12;


long lastMsg = 0;
char msg[20];


void setup()
{
  Serial.begin(9600);
  Serial.println("Serial set up..");

  int status;
  // attempt to connect to Wifi network:
  WiFi.begin("iPhone", "Geosense123");

  while (WiFi.status() != WL_CONNECTED) {  
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("WiFi set up");
  /**
   * PubNub Initialization
   */
  PubNub.begin(pubkey, subkey);


  /**
   * DHT22 Module Initialization
   */
  dht.setup(DHTPIN,DHTesp::DHT22);


  root["deviceId"] = "tempHumidSensor";
  root["temp"] = 0;
  root["humid"] = 0;

  Serial.println("PubNub set up Done");
}

aJsonObject *createMessage()
{
  aJsonObject *msg = aJson.createObject();

  aJsonObject *sender = aJson.createObject();
  aJson.addStringToObject(sender, "name", "Arduino");
  aJson.addItemToObject(msg, "sender", sender);

  int analogValues[6];
  for (int i = 0; i < 6; i++) {
    analogValues[i] = analogRead(i);
  }
  aJsonObject *analog = aJson.createIntArray(analogValues, 6);
  aJson.addItemToObject(msg, "analog", analog);
  return msg;
}

/* Process message like: { "pwm": { "8": 0, "9": 128 } } */
void processPwmInfo(aJsonObject *item)
{
  aJsonObject *pwm = aJson.getObjectItem(item, "pwm");
  if (!pwm) {
    Serial.println("no pwm data");
    return;
  }

  const static int pins[] = { 8, 9 };
  const static int pins_n = 2;
  for (int i = 0; i < pins_n; i++) {
    char pinstr[3];
    snprintf(pinstr, sizeof(pinstr), "%d", pins[i]);

    aJsonObject *pwmval = aJson.getObjectItem(pwm, pinstr);
    if (!pwmval) continue; /* Value not provided, ok. */
    if (pwmval->type != aJson_Int) {
      Serial.print(" invalid data type ");
      Serial.print(pwmval->type, DEC);
      Serial.print(" for pin ");
      Serial.println(pins[i], DEC);
      continue;
    }

    Serial.print(" setting pin ");
    Serial.print(pins[i], DEC);
    Serial.print(" to value ");
    Serial.println(pwmval->valueint, DEC);
  //  analogWrite(pins[i], pwmval->valueint);
  }
}

void dumpMessage(Stream &s, aJsonObject *msg)
{
  int msg_count = aJson.getArraySize(msg);
  for (int i = 0; i < msg_count; i++) {
    aJsonObject *item, *sender, *analog, *value;
    s.print("Msg #");
    s.println(i, DEC);

    item = aJson.getArrayItem(msg, i);
    if (!item) { s.println("item not acquired"); delay(1000); return; }

    processPwmInfo(item);

    /* Below, we parse and dump messages from fellow Arduinos. */

    sender = aJson.getObjectItem(item, "sender");
    if (!sender) { s.println("sender not acquired"); delay(1000); return; }

    s.print(" A2: ");
    analog = aJson.getObjectItem(item, "analog");
    if (!analog) { s.println("analog not acquired"); delay(1000); return; }
    value = aJson.getArrayItem(analog, 2);
    if (!value) { s.println("analog[2] not acquired"); delay(1000); return; }
    s.print(value->valueint, DEC);

    s.println();
  }
}


void loop()
{
  
  WiFiClient *client;

    temperature = dht.getTemperature();
    humidity  = dht.getHumidity();
    Serial.println("\n Temp and Humidity are ");
    
    Serial.println(temperature);
    Serial.println(humidity);

    /**
     * Prepare JSON Data from Temp and Humidity
     */
    char JSONmessageBuffer[300];
    if (!isnan(temperature)) {
      root["temp"] = temperature;
      root["humid"] = humidity;
      root.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
      root.printTo(Serial);
      //free(JSONmessageBuffer);
    }

    Serial.print("publishing a message: ");

    aJsonObject *msg = createMessage();
    char *msgStr = aJson.print(msg);
    aJson.deleteItem(msg);

    msgStr = (char *) realloc(msgStr, strlen(msgStr) + 1);

    //client = PubNub.publish(channel, msgStr);
    client = PubNub.publish(channel, JSONmessageBuffer);
    free(msgStr);
    if (!client) {
      Serial.println("publishing error");
      delay(1000);
      return;
    }
    client->stop();

    //WAIT for 10 Seconds before publishing next
    delay(10000);
}
