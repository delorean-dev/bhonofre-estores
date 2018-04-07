
#include <Timing.h> //https://github.com/scargill/Timing
//MQTT
#include <PubSubClient.h>
//ESP
#include <ESP8266WiFi.h>
//Wi-Fi Manger library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>//https://github.com/tzapu/WiFiManager
//OTA 
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Bounce2.h> //https://github.com/thomasfredericks/Bounce-Arduino-Wiring
#define AP_TIMEOUT 60
#define SERIAL_BAUDRATE 115200

#define MQTT_AUTH true
#define MQTT_USERNAME "" ////////// COLOCA ENTRE AS ASPAS O USERNAME DO TEU MQTT
#define MQTT_PASSWORD "" ////////// COLOCA ENTRE AS ASPAS A PASSWORD DO TEU MQTT

#define BLIND_OPEN_RELAY 04
#define BLIND_CLOSE_RELAY 05

 
#define SWITCH_OPEN 13
#define SWITCH_CLOSE 12


//CONSTANTS
const String HOSTNAME  = "estores_sala"; ///////////////////////////MUDAR O NOME NESTE CASO É PARA OS ESTORES DA SALA

const char * OTA_PASSWORD  = "otapower";
const String MQTT_LOG = "system/log";
const String MQTT_SYSTEM_CONTROL_TOPIC = "system/set";


const char *MQTT_CONTROL_TOPIC = "sala/estores/set"; ///////////////////////////MUDAR O NOME NESTE CASO É PARA OS ESTORES DA SALA
const char *MQTT_STATE_TOPIC = "sala/estores/state"; ///////////////////////////MUDAR O NOME NESTE CASO É PARA OS ESTORES DA SALA


const char* MQTT_SERVER = "192.168.2.19"; ////////////////////// COLOCA ENTRE AS ASPAS O IP DO TEU BROKER MQTT
long debounceDelay = 50;
WiFiClient wclient;
PubSubClient client(MQTT_SERVER,1883,wclient);
Bounce debouncerOpen = Bounce();
Bounce debouncerClose = Bounce();
String notifiedState = "";

//CONTROL FLAGS
bool OTA = false;
bool OTABegin = false;
String lastState = "-";

         
int lastOpenButtonState = LOW;   
int lastCloseButtonState = LOW;  

Timing notifTimer;
void setup() {
  Serial.begin(115200);
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
  /*define o tempo limite até o portal de configuração ficar novamente inátivo,
   útil para quando alteramos a password do AP*/
  wifiManager.setTimeout(AP_TIMEOUT);
  wifiManager.autoConnect(HOSTNAME.c_str());
  client.setCallback(callback);
  
  pinMode(BLIND_OPEN_RELAY,OUTPUT);
  pinMode(BLIND_CLOSE_RELAY,OUTPUT);
  
  pinMode( SWITCH_OPEN,INPUT_PULLUP);
  pinMode( SWITCH_CLOSE,INPUT_PULLUP);
  
  debouncerOpen.attach(SWITCH_OPEN);
  debouncerOpen.interval(5);//DELAY
    
  debouncerClose.attach(SWITCH_CLOSE);
  debouncerClose.interval(5);//DELAY
}


void openTela(){
  digitalWrite(BLIND_CLOSE_RELAY,LOW);
  digitalWrite(BLIND_OPEN_RELAY,HIGH);
  Serial.println("OPEN");
}
void closeTela(){
  digitalWrite(BLIND_OPEN_RELAY,LOW);
  digitalWrite(BLIND_CLOSE_RELAY,HIGH);
  Serial.println("CLOSE");
}

void stopTela(){
  digitalWrite(BLIND_OPEN_RELAY,LOW);
  digitalWrite(BLIND_CLOSE_RELAY,LOW);
  Serial.println("STOP");
}

//Chamada de recepção de mensagem 
void callback(char* topic, byte* payload, unsigned int length) {
  String payloadStr = "";
  for (int i=0; i<length; i++) {
    payloadStr += (char)payload[i];
  }
  Serial.println(payloadStr);
  String topicStr = String(topic);
  if(topicStr.equals(MQTT_SYSTEM_CONTROL_TOPIC)){
    if(payloadStr.equals("OTA_ON_"+String(HOSTNAME))){
      OTA = true;
      OTABegin = true;
    }else if (payloadStr.equals("OTA_OFF_"+String(HOSTNAME))){
      OTA = false;
      OTABegin = false;
    }else if (payloadStr.equals("REBOOT_"+String(HOSTNAME))){
      ESP.restart();
    }
  }else if ( topicStr.equals(MQTT_CONTROL_TOPIC)){
  if(payloadStr.equals("OPEN")){
    openTela();
  }else if (payloadStr.equals("CLOSE")){
    closeTela();
  }else if (payloadStr.equals("STOP")){
    stopTela();
  }
  }
} 
  
bool checkMqttConnection(){
  if (!client.connected()) {
    if (MQTT_AUTH ? client.connect(HOSTNAME.c_str(),MQTT_USERNAME, MQTT_PASSWORD) : client.connect(HOSTNAME.c_str())) {
      //SUBSCRIÇÃO DE TOPICOS
      Serial.println("CONNECTED ON MQTT");
      client.subscribe(MQTT_SYSTEM_CONTROL_TOPIC.c_str());
      client.subscribe(MQTT_CONTROL_TOPIC);
      //Envia uma mensagem por MQTT para o tópico de log a informar que está ligado
      client.publish(MQTT_LOG.c_str(),(String(HOSTNAME)+" CONNECTED").c_str());
    }
  }
  return client.connected();
}


void loop() {
debouncerOpen.update();
debouncerClose.update();

if (WiFi.status() == WL_CONNECTED) {
    if (checkMqttConnection()){
      client.loop();

      //CODIGO
      int realStateOpen = debouncerOpen.read();
      int realStateClose = debouncerClose.read();
      
      if(realStateOpen){
        if(realStateOpen != lastOpenButtonState){
          lastOpenButtonState = realStateOpen;
          client.publish(MQTT_STATE_TOPIC,"OPEN",true);
          openTela();
        }
        
      }

      if(realStateClose){
        if(realStateClose != lastCloseButtonState){
          lastCloseButtonState = realStateClose;
          client.publish(MQTT_STATE_TOPIC,"CLOSE",true);
          closeTela();
        }
        
      }

      if(realStateOpen == LOW && realStateClose == LOW ){
        if(realStateClose != lastCloseButtonState && realStateOpen != lastOpenButtonState){
          lastCloseButtonState = realStateClose;
          lastOpenButtonState = realStateOpen;
          client.publish(MQTT_STATE_TOPIC,"STOP",true);
          stopTela();
        }
        
       }


      
      if(OTA){
        if(OTABegin){
          setupOTA();
          OTABegin= false;
        }
        ArduinoOTA.handle();
      }
    }
  }
}

void setupOTA(){
  if (WiFi.status() == WL_CONNECTED && checkMqttConnection()) {
    client.publish(MQTT_LOG.c_str(),(String(HOSTNAME)+" OTA IS SETUP").c_str());
    ArduinoOTA.setHostname(HOSTNAME.c_str());
    ArduinoOTA.setPassword((const char *)OTA_PASSWORD);
    ArduinoOTA.begin();
    client.publish(MQTT_LOG.c_str(),(String(HOSTNAME)+" OTA IS READY").c_str());
  }  
}

