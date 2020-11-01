#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h> 
#include <StreamString.h>
#include <IRac.h>
#include <IRsend.h>
#include <IRutils.h>
#include <IRremoteESP8266.h>
 
ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
WiFiClient client;

#ifndef STASSID
#define STASSID "Ricardo"
#define STAPSK  "qualqueruma"
#endif
 
//Configurações-------------------------------------------------------
#define ssid STASSID
#define password STAPSK
 
#define MyApiKey "0b1a7175-bc19-44f4-8111-b595aff9c23f"
 
#define DispositivoID "5f96b102901de20c1b53b386"
 
#define kIrLed 4         //Pino onde o LED IR está conectado
 
#define Protocolo LG2   //Substitua pelo Protocolo Obtido no Teste
 
#define TempMin 16       //Temperatura Mínima do Ar 
#define TempMax 32       //Temperatura Máxima do Ar 
//--------------------------------------------------------------------
 
#define HEARTBEAT_INTERVAL 300000 // 5 Minutes 
 
IRac ac(kIrLed);
 
uint64_t heartbeatTimestamp = 0;
bool isConnected = false;
 
//Variáveis de Controle
bool Turbo = false;   //Modo Turbo, ativado quando uma temperatura menor do que TempMin for definida
bool Swing = false;   
bool State = false;
int Temperature = 21;
String Mode = "Cool"; //Modo de Resfriamento Padrão, para Desumidificador selecionar Modo de Aquecimento no Google Home
String FanSpeed = "medium";
 
void turnOn(String deviceId);
void turnOff(String deviceId);
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void SendCommand(int Temp, bool Turbo, String Mode, bool Swing, String FanSpeed);
 
void setup() 
{
  pinMode(kIrLed, OUTPUT);
   
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
   
  WiFiMulti.addAP(ssid, password);
  Serial.println();
  Serial.print("Conectando a Rede: ");
  Serial.println(ssid);  
 
  //Espera pela conexão WiFi
  while(WiFiMulti.run() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
 
  if(WiFiMulti.run() == WL_CONNECTED) 
  {
    Serial.println("");
    Serial.println("WiFi Conectado");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
 
  //Estabelece conexão com Sinric
  webSocket.begin("iot.sinric.com", 80, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setAuthorization("apikey", MyApiKey);
  webSocket.setReconnectInterval(5000);
}
 
void loop()
{
  webSocket.loop();
   
  if(isConnected) 
  {
      uint64_t now = millis();
      //Mantem a conexão mesmo se houver mudança do IP
      if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) 
      {
          heartbeatTimestamp = now;
          webSocket.sendTXT("H");          
      }
  }   
}
 
 
void turnOn(String deviceId) 
{
  if (deviceId == DispositivoID)
  {  
    Serial.print("Ligar o dispositivo ID: ");
    Serial.println(deviceId);
 
    SendCommand(Temperature, false, Mode, false, "medium");
  } 
}
 
void turnOff(String deviceId) 
{
    if (deviceId == DispositivoID)
    {  
      Serial.print("Desligar o dispositivo ID: ");
      Serial.println(deviceId);
   
      ac.next.power = false;
      ac.sendAc();
    } 
}
 
//Lida com os pedidos recebidos pela Sinric
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) 
{  
  if(type == WStype_TEXT) 
  {
#if ARDUINOJSON_VERSION_MAJOR == 5
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject((char*)payload);
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6        
    DynamicJsonDocument json(1024);
    deserializeJson(json, (char*) payload);      
#endif        
    String deviceId = json ["deviceId"];     
    String action = json ["action"];
     
    if(action == "action.devices.commands.OnOff")  // Liga/Desliga
    {
        String value = json ["value"]["on"];
         
        if(value == "true") 
        {
            turnOn(deviceId);
            Serial.println("\n Ligar");
            Serial.print("\n Temperatura: ");
            Serial.println(Temperature); 
            Serial.print("\n Modo: ");
            Serial.println(Mode); 
            Serial.print("\n");
        } 
        else
        {
            turnOff(deviceId);
            Serial.println("\n Desligar \n");
        }
    }
    else if (action == "action.devices.commands.ThermostatTemperatureSetpoint") //Definir Temperatura
    {
      String value = json ["value"]["thermostatTemperatureSetpoint"];
      int temp = value.toInt();
       
      if(temp < TempMin)
      {
        SendCommand(TempMin, true, Mode, false, "medium");
        Temperature = TempMin;
        Serial.print("\n Temperatura: ");
        Serial.println(TempMin);
        Serial.println(" Turbo Ligado ");
        Serial.print("\n");;
      }
      if(temp > TempMax)
      {
        SendCommand(TempMax, false, Mode, false, "medium");
        Temperature = TempMax;
        Serial.print("\n Temperatura: ");
        Serial.println(TempMax); 
        Serial.print("\n");
      }
      if((temp >= TempMin) && (temp <= TempMax))          
      {
        SendCommand(temp, false, Mode, false, "medium");
        Temperature = temp;
        Serial.print("\n Temperatura: ");
        Serial.println(temp); 
        Serial.print("\n");    
      }
    }
     
    else if (action == "action.devices.commands.ThermostatSetMode")   //Definir modo de operação
    {
      String value = json ["value"]["thermostatMode"];
 
      if(value == "cool")
      {
        Mode = "Cool";
        SendCommand(Temperature, false, Mode, false, "medium");
        Serial.println("\n Modo: Resfriamento \n");
      }
      if(value == "heat")
      {
        Mode = "Dry";
        SendCommand(Temperature, false, Mode,  false, "medium");
        Serial.println("\n Modo: Desumidificar \n");
      }
      if(value == "off")
      {
        turnOff(deviceId);
        Serial.println("\n Desligar \n");
      }
    }
    else if (action == "test") 
    {
        Serial.println("\n TESTE FEITO NO SINRIC \n");
    }
  }
}
 
void SendCommand(int Temp, bool Turbo, String Mode, bool Swing, String FanSpeed)
{
  ac.next.protocol = decode_type_t::Protocolo;      //Protocolo
  ac.next.model = 1;
  ac.next.celsius = true;                           //Use Degrees Celsius
   
  if(FanSpeed == "low")
    ac.next.fanspeed = stdAc::fanspeed_t::kLow;     //Velocidade do Ventilador
  if(FanSpeed == "medium")
    ac.next.fanspeed = stdAc::fanspeed_t::kMedium;
  if(FanSpeed == "high")
    ac.next.fanspeed = stdAc::fanspeed_t::kHigh;
     
  if(Mode == "Cool")
    ac.next.mode = stdAc::opmode_t::kCool;          //Modo de Operação
  if(Mode == "Dry")
    ac.next.mode = stdAc::opmode_t::kDry;   
     
  if(Swing)
    ac.next.swingv = stdAc::swingv_t::kAuto;        //Movimento das Aletas
  else
    ac.next.swingv = stdAc::swingv_t::kOff;  
     
  ac.next.light = false;                             //Define se as luzes do Aparelho ficam acesas
  ac.next.turbo = Turbo;                            //Turbo ON/OFF
  ac.next.degrees = Temp;                           //Temperaura
  ac.next.power = true;                             //Ligado/Desligado
  ac.sendAc();                                      //Envia o Comando
}
