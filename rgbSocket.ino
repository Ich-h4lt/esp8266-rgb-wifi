#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Ticker.h>

#define max(a,b) ((a)>(b)?(a):(b))

//To use MQTT, install Library "PubSubClient" and switch next line to 1
#define USE_MQTT 0
//If you don`t want to use local button switch to 0
#define USE_LOCAL_BUTTON 0

// Allow WebUpdate
#define USE_WEBUPDATE 1
#if USE_WEBUPDATE == 1
#include <ESP8266HTTPUpdateServer.h>
#endif

#if USE_MQTT == 1
	#include <PubSubClient.h>
	//Your MQTT Broker
	const char* mqtt_server = "your mqtt broker";
	const char* mqtt_in_topic = "socket/switch/set";
  const char* mqtt_out_topic = "socket/switch/status";
	
#endif


//Yout Wifi SSID
const char* ssid = "DoSo-Infra";
//Your Wifi Key
const char* password = "abc1234!";


ESP8266WebServer server(80);
#if USE_WEBUPDATE == 1
ESP8266HTTPUpdateServer httpUpdater;
#endif

#if USE_MQTT == 1
  WiFiClient espClient;
  PubSubClient client(espClient);
  bool status_mqtt = 1;
#endif
// gpio0 fuer flash ist ioo 
// rgb = 10 (gpio 12), 12 (gpio 13), 13 (gpio15) 

// green 12 
// blau 13
// red 5

int gpio13Blue = 13;
int gpio12Green = 12;
int gpio5Red = 5;

String readString;           //String to hold incoming request
String hexString = "112233"; //Define inititial color here (hex value)

int state;

int red = 0;
int green = 0;
int blue = 0;

float float_red;
float float_green;
float float_blue;

int x;
int V;

// Timer
unsigned long stopAt = 0;
unsigned long startAt = 0;
// "0" means timer not running
// reference is millis()

void setup(void){
  Serial.begin(115200); 
  delay(5000);
  Serial.println("");

 
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
 
  pinMode(gpio13Blue, OUTPUT);
  analogWrite(gpio13Blue, blue);
  
  pinMode(gpio12Green, OUTPUT);
  analogWrite(gpio12Green, green);

  pinMode(gpio5Red, OUTPUT);
  analogWrite(gpio5Red, red);
  

  // Wait for WiFi
  Serial.println("");
  Serial.print("Verbindungsaufbau mit:  ");
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Verbunden");
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.localIP());


#if USE_MQTT == 1  
  client.setServer(mqtt_server, 1883);
  client.setCallback(MqttCallback);
#endif

  server.on("/", [](){
    String msg = "<H1>Sonoff</H1>\n";
    msg += "uptime " + String(millis() / 1000) + "s<br />\n";
    msg += "<H2>Befehle</H2>\n<p>";
    msg += "<a href=\"ein\">Einschalten</a><br />\n";
    msg += "<a href=\"aus\">Ausschalten</a><br />\n";
    msg += "<a href=\"toggle\">Umschalten</a><br />\n";
    msg += "<a href=\"state\">Schaltstatus</a><br />\n";
    msg += "<a href=\"timer\">Timer</a><br />\n";
#if USE_WEBUPDATE == 1
    msg += "<a href=\"update\">Update</a><br />\n";
#endif
    msg += "</p>\n";
    msg += "<H2>WiFi</H2>\n<p>";
    msg += "IP: " + WiFi.localIP().toString() + "<br />\n";
    msg += "Mask: " + WiFi.subnetMask().toString() + "<br />\n";
    msg += "GW: " + WiFi.gatewayIP().toString() + "<br />\n";
    msg += "MAC: " + WiFi.macAddress() + "<br />\n";
    msg += "SSID: " + String(WiFi.SSID()) + "<br />\n";
    msg += "RSSI: " + String(WiFi.RSSI()) + "<br />\n";
    msg += "</p>\n";
    msg += "<H2>Status</H2>\n";
    if ((startAt) > 0)
    {
      msg += "Timer bis zum Einschalten noch " + String((startAt - millis()) / 1000) + "s. ";
    }
    if ((stopAt) > 0)
    {
      msg += "Timer bis zum Ausschalten noch " + String((stopAt - millis()) / 1000) + "s. ";
    }
    server.send ( 302, "text/plain", "");  
  });  
  
  //German Path, left for compatibility


   server.on("/on", [](){
    setHex();
    server.send ( 302, "text/plain", "");  
  }); 

   server.on("/off", [](){
    allOff();
    server.send ( 302, "text/plain", "");  
  }); 

   server.on("/set", [](){
    hexString = "";
    hexString = server.arg("color");
    setHex();
    server.send ( 302, "text/plain", "");  
  }); 

   server.on("/state", [](){
    //server.println(state)
    server.send ( 200, "text/plain", String(state));  
  }); 

   server.on("/color", [](){
    //server.println(hexString)
    server.send ( 200, "text/plain", hexString);  
  }); 

   server.on("/bright", [](){
    getV();
    //server.println(V)
    server.send ( 200, "text/plain", String(V));  
  }); 
  

   server.on("/toggleR", [](){
    if(red == 0){
      server.send(200, "text/html", "Schaltsteckdose ist aktuell ein.<p><a href=\"aus\">Ausschalten</a></p>");
      Switch_red_On();
      delay(1000);
    }else{
      server.send(200, "text/html", "Schaltsteckdose ist aktuell aus.<p><a href=\"ein\">Einschalten</a></p>");
      Switch_red_Off();
      delay(1000);
    }
    server.send ( 302, "text/plain", "");  
  }); 
  
  server.on("/toggleG", [](){
    if(green == 0){
      server.send(200, "text/html", "Schaltsteckdose ist aktuell ein.<p><a href=\"aus\">Ausschalten</a></p>");
      Switch_green_On();
      delay(1000);
    }else{
      server.send(200, "text/html", "Schaltsteckdose ist aktuell aus.<p><a href=\"ein\">Einschalten</a></p>");
      Switch_green_Off();
      delay(1000);
    }
    server.send ( 302, "text/plain", "");  
  }); 
  
  server.on("/toggleB", [](){
    if(blue == 0){
      server.send(200, "text/html", "Schaltsteckdose ist aktuell ein.<p><a href=\"aus\">Ausschalten</a></p>");
      Switch_blue_On();
      delay(1000);
    }else{
      server.send(200, "text/html", "Schaltsteckdose ist aktuell aus.<p><a href=\"ein\">Einschalten</a></p>");
      Switch_blue_Off();
      delay(1000);
    }
    server.send ( 302, "text/plain", "");  
  }); 

 // Timer
  server.on("/timer", []() {
    String message = "";
    for (int i = 0; i < server.args(); i++) {

      //message += "Arg nº" + (String)i + " –> ";
      //message += server.argName(i) + ": ";
      //message += server.arg(i) + " <br />\n";

      if (server.argName(i) == "off") {
        int dauer = server.arg(i).toInt();
        startAt = millis() + (dauer * 1000);
        message += "Aus die nächsten " + String(dauer) + "s";
        server.send(200, "text/html", message);
        //Switch_Off();
        delay(100);
      }

      if (server.argName(i) == "on") {
        int dauer = server.arg(i).toInt();
        stopAt = millis() + (dauer * 1000);
        message += "An die nächsten " + String(dauer) + "s";
        server.send(200, "text/html", message);
       // Switch_On();
      }
    }
    if (message == "") //No Parameter
    {
      message += "<form action=\"timer\" id=\"on\">Timer bis zum Ausschalten <input type=\"text\" name=\"on\" id=\"on\" maxlength=\"5\"> Sekunden<button type=\"submit\">Starten</button></form>";
      message += "<form action=\"timer\" id=\"off\">Timer bis zum Einschalten <input type=\"text\" name=\"off\" id=\"off\" maxlength=\"5\"> Sekunden <button type=\"submit\">Starten</button></form>";
    server.send(200, "text/html", message);
    server.send ( 302, "text/plain", "");
    }
  });

#if USE_WEBUPDATE == 1
  httpUpdater.setup(&server);
#endif
  
  server.begin();
  Serial.println("HTTP server started");

  //checker.attach(0.1, check);

}

#if USE_MQTT == 1
void MqttCallback(char* topic, byte* payload, unsigned int length) {
  // Switch on
  if ((char)payload[0] == '1') {
    Switch_On();
  //Switch off
  } else {
    Switch_Off();
  }

}

void MqttReconnect() {
  String clientID = "SonoffSocket_"; // 13 chars
  clientID += WiFi.macAddress();//17 chars

  while (!client.connected()) {
    Serial.print("Connect to MQTT-Broker");
    if (client.connect(clientID.c_str())) {
      Serial.print("connected as clientID:");
      Serial.println(clientID);
      //publish ready
      client.publish(mqtt_out_topic, "mqtt client ready");
      //subscribe in topic
      client.subscribe(mqtt_in_topic);
    } else {
      Serial.print("failed: ");
      Serial.print(client.state());
      Serial.println(" try again...");
      delay(5000);
    }
  }
}

void MqttStatePublish() {
  if (relais == 1 and not status_mqtt)
     {
      status_mqtt = relais;
      client.publish(mqtt_out_topic, "on");
      Serial.println("MQTT publish: on");
     }
  if (relais == 0 and status_mqtt)
     {
      status_mqtt = relais;
      client.publish(mqtt_out_topic, "off");
      Serial.println("MQTT publish: off");
     }
}

#endif


//Write requested hex-color to the pins (10bit pwm)
void setHex() {
  state = 1;
  long number = (long) strtol( &hexString[0], NULL, 16);
  red = number >> 16;
  green = number >> 8 & 0xFF;
  blue = number & 0xFF;
  red = map(red, 0, 255, 0, 1023);  //added for 10bit pwm
  green = map(green, 0, 255, 0, 1023);  //added for 10bit pwm
  blue = map(blue, 0, 255, 0, 1023);  //added for 10bit pwm
  analogWrite(gpio5Red, (red));
  analogWrite(gpio12Green, (green));
  analogWrite(gpio13Blue, (blue));
}

//Compute current brightness value
void getV() {
  float_red = roundf(red/10.23);  //for 10bit pwm, was (r/2.55);
  float_green = roundf(green/10.23);  //for 10bit pwm, was (g/2.55);
  float_blue = roundf(blue/10.23);  //for 10bit pwm, was (b/2.55);
  x = max(float_red,float_green);
  V = max(x, float_blue);
}

void allOff() {
  state = 0;
  analogWrite(gpio5Red, 0);
  analogWrite(gpio12Green, 0);
  analogWrite(gpio13Blue, 0);
}

void Switch_red_On(void) {
  digitalWrite(gpio5Red, HIGH);
  red = 1;
  startAt = 0;
}
void Switch_red_Off(void) {
  digitalWrite(gpio5Red, LOW);
  red = 0;
  stopAt = 0;
}
void Switch_green_On(void) {
  digitalWrite(gpio12Green, HIGH);
  green = 1;
  startAt = 0;
}
void Switch_green_Off(void) {
  digitalWrite(gpio12Green, LOW);
  green = 0;
  stopAt = 0;
}
void Switch_blue_On(void) {
  digitalWrite(gpio13Blue, HIGH);
  blue = 1;
  startAt = 0;
}
void Switch_blue_Off(void) {
  digitalWrite(gpio13Blue, LOW);
  blue = 0;
  stopAt = 0;
}



void loop(void){
  //Webserver 
  server.handleClient();

  
#if USE_MQTT == 1
//MQTT
   if (!client.connected()) {
    MqttReconnect();
   }
   if (client.connected()) {
    MqttStatePublish();
   }
  client.loop();
#endif  
} 
