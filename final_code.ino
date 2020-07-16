// Import required libraries
#include <AWS_IOT.h>
#include "WiFi.h"
#include <DHT.h>
#include <BlynkSimpleEsp32.h>

#define DHTTYPE DHT11    // Type of the DHT Sensor, DHT11/DHT22
#define DHT_PIN 25       // DHT11(data) pin connected to ESP32 pin(IO/25)
#define Moisture_PIN 32  // Moisture sensor (data) pin connected to ESP32(IO/32)
#define LIGHT_PIN 33     // Light sensor (data) pin connected to ESP32 (IO/33)
#define ALERT_LED 13     // Buzzer Pin(data) connected to ESP32(IO/13)
 
// Soil Moister sensor constants 
const int AirValue = 2000;  // row value (high)
const int WaterValue = 500; // row value (low)
int intervals = (AirValue - WaterValue)/3;
int soilMoistureValue = 0;

int lightValue = 0;
WidgetLED Alert_LED(V9), Alarm_LED(V10), Healthy_LED(V11);

const char* server = "api.thingspeak.com";            // Thingsepeak Server
const String apiKey = "EKYMQY31OZ9CSPT7";             // Thingsepeak apiKey
// char auth[] = "Gr61so2JwLH1C9F6Iaf-sv3zlcyhgaAh";    // Blynk Authurization
char auth[] = "DG8ipzqOIeZHc5nNL7qIL43SqHITSlh9";     // Blynk Authurization

//BlynkTimer timer;                                   // Create BlynkTimer Instance
DHT dht(DHT_PIN, DHTTYPE);                            // create DHT INSTANCE
AWS_IOT aws;                                          // ceate AWS_IOT instance

// Replace with your wifi network credentials
const char* ssid ="Vodafone-75C1";
const char* password ="XaTyNCdhULcL4qnt";

//Replace with your AWS Custom endpoint Address
char HOST_ADDRESS[]="a2s1z2b78iicpd-ats.iot.eu-central-1.amazonaws.com"; 

char CLIENT_ID[]= "Plant01";   // (Thing unique ID), this id should be unique among all things associated with your AWS account.
char MQTT_TOPIC[]= "Plant01/Environment";
int status = WL_IDLE_STATUS;
char payload[512];
//------------------------------------------------------------------------
void setup(){
  // Serial port for debugging purposes
  Serial.begin(9600);
  dht.begin();                       // Initializing DHT11
  Blynk.begin(auth, ssid, password); // Initialize Blynk

  pinMode(ALERT_LED, OUTPUT);
  
  // Send Email when the Plant is online
  send_welcome_email("saif.wsm@gmail.com");

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to wifi");
  
  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());


  // Connect to AWS using Host Address and Client ID
  if(aws.connect(HOST_ADDRESS,CLIENT_ID)== 0) // connects to host and returns 0 upon success
    {
        Serial.println("Connected to AWS");
        delay(1000);
    }
    else
    {
        Serial.println("AWS connection failed, Check the HOST Address");
        while(1);
    }
}
//--------------------------------------------------------------------------------------------- 
void loop(){
    Blynk.run();
    // Generate Random Temperature and Humidity value generated
    // float Temperature = random(20, 31);
    // float Humidity = random(60,100);

    float temp = dht.readTemperature();  // return temperature in °C
    float humidity = dht.readHumidity(); // return humidity in %
    int soil_moisture = readSoilMoister();
    int light = readLight();

    Serial.print("Temperature: ");
    Serial.println(temp);
    Serial.print("Humidity: ");
    Serial.println(humidity);
    Serial.print("Moisture: ");
    Serial.println(soil_moisture);
    Serial.print("light: ");
    Serial.println(light);

    // if plant temperature higher than 24C or lower than 16C ==> Alert
    if (temp < 16 || temp > 30) {
      Alert_led(1);
    }

    // check wether reading was successful or not
    if(temp == NAN || humidity == NAN) {
      Serial.println("DHT11 Reading faild");
    }
    else {
     // Create the payload for publishing
     sprintf(payload,"{\"DeviceID\":ESP32, \"Humidity\":%f, \"Temperature\":%f, \"light\":%i, \"soil moisture\":%i}",humidity, temp, light, soil_moisture);
    }

    // publishes payload and returns 0 upon success
    if(aws.publish(MQTT_TOPIC, payload) == 0){        
        Serial.print("Publish Message:");   
        Serial.println(payload);
    }
    else{
        Serial.println("Publish failed");
    }

    // Publish Data to ThingSpeak
    SendDataToThingSpeak(temp, humidity, soil_moisture, light, 1, 2, 3, 4);

    // publish data to Blynk 
    sendDataToBlynk(temp ,humidity, soil_moisture, light);

  // Repeat every 10 seconds.
  vTaskDelay(10000 / portTICK_RATE_MS);               
}
//--------------------------------------------------------------------------------------------

//Soil Moisture sensor function
int readSoilMoister(){
  soilMoistureValue = analogRead(Moisture_PIN); //put Sensor insert into soil
  Serial.println("soilMoistureValue: ");
  Serial.println(soilMoistureValue);
  int processed_value = 100 - (scaling(soilMoistureValue, WaterValue, AirValue, 0, 100));
  
  if(processed_value > 80)
    {
      Serial.println("Very Wet");
//      Alert_LED.off();
//      Alarm_LED.off();
//      Healthy_LED.on();
        Alert_led(0);
    }
    else if(processed_value < 20)
    {
      Serial.println("Dry");
//      Alert_LED.off();
//      Alarm_LED.on();
//      Healthy_LED.off();
       send_alert_email("saif.wsm@gmail.com");
      Alert_led(1);
    }
    else
    {
      Serial.println("Wet");
//      Alert_LED.off();
//      Alarm_LED.off();
//      Healthy_LED.on();
        Alert_led(0);
    }
  return (processed_value); //SCALINE soil moisture value in range of 0% - 100%
}
//--------------------------------------------------------------------------------------------

// Light sensor function 
int readLight(){
  lightValue = analogRead(LIGHT_PIN);
  return scaling(lightValue, 0, 4095, 0, 100); // scale 0 - 4095 row value to 0% - 100%
}
//---------------------------------------------------------------------------------------------
// SEND Data to thinkspeak
void SendDataToThingSpeak(float temp, float hum, int soil_moisture, int light, int field1, int field2, int field3, int field4) {
  WiFiClient client;
  
  if (client.connect(server, 80)) {
    Serial.println("*********************");
    Serial.println("Wifi Client Connected");
  
    String POSTstr = apiKey;
    POSTstr += "&field";
    POSTstr += field1;
    POSTstr += "=";
    POSTstr += String(temp);
    
    POSTstr += "&field";
    POSTstr += field2;
    POSTstr += "=";
    POSTstr += String(hum);

    POSTstr += "&field";
    POSTstr += field3;
    POSTstr += "=";
    POSTstr += String(soil_moisture);

    POSTstr += "&field";
    POSTstr += field4;
    POSTstr += "=";
    POSTstr += String(light);
    POSTstr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + apiKey +"\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(POSTstr.length());
    client.print("\n\n");
    client.print(POSTstr);
    
    delay(1000);
  }
  client.stop();
}
//-----------------------------------------------------------------------------------------------

void sendDataToBlynk(float temp, float humidity,int soil_moisture, int light)
{
  Blynk.virtualWrite(V5, (int)temp);
  Blynk.virtualWrite(V6, (int)humidity);
  Blynk.virtualWrite(V7, soil_moisture);
  Blynk.virtualWrite(V8, light);
}

//----------------------------------------------------------------------------------------
// scaling function to convert from any range to any range
int scaling(int value, int rowMin, int rowMax, int newRange0, int newRange100 ) {
    int x = (((value - rowMin) * (newRange100 - newRange0)) / (rowMax - rowMin)) + newRange0;
    if (x < 0)        { return 0;}
    else if (x > 100) { return 100; }
    else              { return x; }
}

void Alert_led(int stat){
  if (stat == 1)      { digitalWrite(ALERT_LED, HIGH);}
  else if (stat == 0) { digitalWrite(ALERT_LED, LOW); }
  delay(1000);
}

void send_alert_email(String email_address){
  String body = String("Alert! Your Plant need your attention ");
  // Blynk.email(email_address, "Subject: Care about your plant!", body);
}

void send_welcome_email(String email_address){
  String body = "";
  body = "SAIF! Your plant is now connected to the internet, you can check it's status from below link \n";
  body += "https://thingspeak.com/channels/1076983 \n";
  body += "Bye" ;
  // Blynk.email(email_address, "Subject: Your Plant is now Online!", body);
}
