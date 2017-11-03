#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

//OBSLUGA DS18B20
#define ONE_WIRE_BUS D5
#include <OneWire.h>
#include <DallasTemperature.h>
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float temp = 0;


// Wifi: SSID and password
const char* WIFI_SSID = "ssid";
const char* WIFI_PASSWORD = "pass";

// MQTT: ID, server IP, port, username and password
#define MQTT_VERSION MQTT_VERSION_3_1_1
const PROGMEM char* MQTT_CLIENT_ID = "maja_lampka";
const PROGMEM char* MQTT_SERVER_IP = "192.168.1.5";
const PROGMEM uint16_t MQTT_SERVER_PORT = 1883;
const PROGMEM char* MQTT_USER = "";
const PROGMEM char* MQTT_PASSWORD = "";

// MQTT: topic

const char* MQTT_MAJA_SENSOR_TEMP = "maja/sensor/temp";
const char* MQTT_LIGHT_STATE_TOPIC = "maja/lampka/state";
const char* MQTT_LIGHT_COMMAND_TOPIC = "maja/lampka/command";
String strTopic;


// WS2812
#define PIN_WS D2
byte red = 255;
byte green = 255;
byte blue = 255;
volatile byte brightness = 255;
volatile byte realRed = 0;
volatile byte realGreen = 0;
volatile byte realBlue = 0;
bool stateOn = false;
const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);
const char* on_cmd = "ON";
const char* off_cmd = "OFF";
byte flashRed = red;
byte flashGreen = green;
byte flashBlue = blue;
byte flashBrightness = brightness;
#define NUMPIXELS 4
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN_WS, NEO_GRB + NEO_KHZ800);


//inne
unsigned long lastMillis;
unsigned long reconnectMillis;
volatile unsigned long pm = 0;

WiFiClient wifiClient;
PubSubClient client(wifiClient);


void publishTemp(float p_temperature) {

	StaticJsonBuffer<200> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();

	root["temperature"] = (String)p_temperature;

	root.prettyPrintTo(Serial);
	Serial.println("");
	/*
	   {
		  "temperature": "23.20" ,

	   }
	*/
	char data[200];
	root.printTo(data, root.measureLength() + 1);
	client.publish(MQTT_MAJA_SENSOR_TEMP, data, true);

}







void callback(char* topic, byte* payload, unsigned int length) {
	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");
	strTopic = String((char*)topic);
	if (strTopic == MQTT_LIGHT_COMMAND_TOPIC)
	{
		char message[length + 1];
		for (int i = 0; i < length; i++) {
			message[i] = (char)payload[i];
		}
		message[length] = '\0';
		Serial.println(message);

		if (!processJson(message)) {
			return;
		}

		if (stateOn) {
			// Update lights
			realRed = map(red, 0, 255, 0, brightness);
			realGreen = map(green, 0, 255, 0, brightness);
			realBlue = map(blue, 0, 255, 0, brightness);
		}
		else {
			realRed = 0;
			realGreen = 0;
			realBlue = 0;
		}



		sendState();
	}
}


bool processJson(char* message) {
	StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

	JsonObject& root = jsonBuffer.parseObject(message);

	if (!root.success()) {
		Serial.println("parseObject() failed");
		return false;
	}

	if (root.containsKey("state")) {
		if (strcmp(root["state"], on_cmd) == 0) {
			stateOn = true;
		}
		else if (strcmp(root["state"], off_cmd) == 0) {
			stateOn = false;
		}
	}




	if (root.containsKey("color")) {
		red = root["color"]["r"];
		green = root["color"]["g"];
		blue = root["color"]["b"];
	}

	if (root.containsKey("brightness")) {
		brightness = root["brightness"];
	}




	return true;
}

void sendState() {
	for (int i = 0; i < NUMPIXELS; i++)
	{
		pixels.setPixelColor(i, realRed, realGreen, realBlue);
		pixels.show();



	}


	StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

	JsonObject& root = jsonBuffer.createObject();

	root["state"] = (stateOn) ? "ON" : "OFF";
	JsonObject& color = root.createNestedObject("color");
	color["r"] = red;
	color["g"] = green;
	color["b"] = blue;

	root["brightness"] = brightness;
	char buffer[400];
	root.printTo(buffer, root.measureLength() + 1);
	Serial.print("stan do wyslania");
	Serial.println(buffer);
	client.publish(MQTT_LIGHT_STATE_TOPIC, buffer, true);
}

void reconnect() {
	// Loop until we're reconnected
	while (!client.connected()) {
		Serial.print("INFO: Attempting MQTT connection...");
		// Attempt to connect
		if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
			Serial.println("INFO: connected");
			client.subscribe("maja/#");
			//return true;
		}
		else {
			Serial.print("ERROR: failed, rc=");
			Serial.print(client.state());
			Serial.println("DEBUG: try again in 5 seconds");
			//return false;

		}
	}
}

void setup() {
	// init the serial
	Serial.begin(115200);
	pixels.begin();

	// init the WiFi connection
	Serial.println();
	Serial.println();
	Serial.print("INFO: Connecting to ");
	WiFi.mode(WIFI_STA);
	Serial.println(WIFI_SSID);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("");
	Serial.println("INFO: WiFi connected");
	Serial.println("INFO: IP address: ");
	Serial.println(WiFi.localIP());

	// init the MQTT connection
	client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
	client.setCallback(callback);
	lastMillis = millis() - 500001;
	sensors.begin();
	pinMode(D7, INPUT_PULLUP);
	//attachInterrupt(D7, wlaczRecznie, FALLING);
	
}

void loop() {
	if (!client.connected()) {
		
		reconnect();


	}
	client.loop();

	Serial.print("odczyt portu D7: ");
	Serial.println(digitalRead(D7));
	if (digitalRead(D7) == 0)
	{
		wlaczRecznie();
		delay(300);
	}


	if (millis() - lastMillis > 500000)
	{
		lastMillis = millis();
		sensors.requestTemperatures();
		temp = sensors.getTempCByIndex(0);
		Serial.println(temp);
		publishTemp(temp);

	}

}

void wlaczRecznie()
{
	
		Serial.println("Przerwanie");

		Serial.print(brightness);

		if (stateOn)
		{
			Serial.println("wylacza");
			stateOn = false;

			realBlue = 0;
			realRed = 0;
			realGreen = 0;

		}
		else
		{
		

			Serial.println("wlaczanie sw");
			realBlue = 255;
			realRed = 255;
			realGreen = 255;
			brightness = 255;
			red = 255;
			green = 255;
			blue = 255;
			stateOn = true;
		}
		sendState();
	
}
