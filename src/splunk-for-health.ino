#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <Servo.h>

// wifi config
WiFiClient wifiClient;
String esid  = "<wifi-network-name>";
String epass = "<wifi-password>";

// splunk settings and http collector token
String collectorToken = "<splunk-hec-token>";
String splunkIndexer = "<your-splunk-domain-name>";
String eventData="";
String clientName ="nodemcu";

// DHT11 sesor
#define DHT_SENSOR_PIN  D7 // The ESP8266 pin D7 connected to DHT11 sensor
#define DHT_SENSOR_TYPE DHT11
DHT dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

// micro servo motor obj
Servo servo;

const int RELAY_PIN = 5;
bool isHumidifierOn = false;

void setup() {
  Serial.begin(9600);
  dht_sensor.begin(); // initialize the DHT sensor

  servo.attach(2); //D4
  servo.write(0);

  pinMode(RELAY_PIN, OUTPUT);

  Serial.println("splunk hec");
  initWiFi();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  delay(500);
}

void loop() {
  float humi  = dht_sensor.readHumidity();
  float temperature_C = dht_sensor.readTemperature();
  float temperature_F = dht_sensor.readTemperature(true);

  // check whether the reading is successful or not
  if ( isnan(temperature_C) || isnan(temperature_F) || isnan(humi)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    Serial.println(humi); 
    if (humi<63) {
      Serial.println(isHumidifierOn);
      if (isHumidifierOn == false) {
        // push the button three times to turn on the humidifier
        servo.write(120);
        delay(500);
        servo.write(0);
        delay(500);

        servo.write(120);
        delay(500);
        servo.write(0);
        delay(500);

        servo.write(120);
        delay(500);
        servo.write(0);
        delay(500);

        isHumidifierOn = true;
      }
    } else {
      Serial.println(isHumidifierOn);
      if (isHumidifierOn == true) {
        // turn off humidifier by one push
        servo.write(120);
        delay(500);
        servo.write(0);
        delay(500);

        isHumidifierOn = false;       
      } 
    }

    Serial.println(temperature_C);
    if (temperature_C > 21.00) { // threshold is 21 celsius
      Serial.println("hot, turning on fan");
      digitalWrite(RELAY_PIN, LOW);
    } else {
      Serial.println("cool, turning down fan");
      digitalWrite(RELAY_PIN, HIGH);
    }

    delay(1500);
    eventData="\"clientname\": \""+clientName+ "\",\"tempC\": \""+String(temperature_C)+"\",\"tempH\": \""+String(temperature_F)+"\",\"humidity\": \""+String(humi)+"\"";
    Serial.println(eventData);
    //send off the data to splunk
    splunkpost(collectorToken, eventData, clientName, splunkIndexer); 
    delay(2000);
  }
  
  
}

void initWiFi(){
  Serial.println();
  Serial.println("Wifi Startup");
  esid.trim();
  if ( esid.length() > 1 ) {
      // test esid 
      WiFi.disconnect();
      delay(100);
      WiFi.mode(WIFI_STA);
      Serial.print("Connecting to WiFi ");
      Serial.println(esid);
      WiFi.begin(esid.c_str(), epass.c_str());
      if ( testWifi() == 20 ) { 
          return;
      }
  }
}

int testWifi(void) {
  int c = 0;
  Serial.println("Wifi test...");  
  while ( c < 30 ) {
    if (WiFi.status() == WL_CONNECTED) { return(20); } 
    delay(500);
    Serial.print(".");    
    c++;
  }
  Serial.println("WiFi Connect timed out");
  return(10);
} 

void splunkpost(String collectorToken, String PostData, String Host, String splunkIndexer)
{
  // recieved the token, post data clienthost and the splunk indexer  
  String payload = "{ \"host\" : \"" + Host +"\", \"sourcetype\" : \"http_test\", \"index\" : \"temp-humid\", \"event\": {" + PostData + "}}";
  
  //Build the request
  HTTPClient http;
  String splunkurl="http://"+ splunkIndexer +":8088/services/collector";
  String tokenValue="Splunk " + collectorToken;
  
  // fire at will!! 
  http.begin(wifiClient, splunkurl);
  http.addHeader("Content-Type", "application/json");
  Serial.println(tokenValue);
  http.addHeader("Authorization", tokenValue);
  Serial.println(payload);
  String contentlength = String(payload.length());
  http.addHeader("Content-Length", contentlength );
  int httpCode = http.POST(payload);
  http.writeToStream(&Serial);
  Serial.print("HTTP response code: ");
  Serial.println(httpCode);
  String response = http.getString();
  Serial.println(response);
  http.end();
 
}