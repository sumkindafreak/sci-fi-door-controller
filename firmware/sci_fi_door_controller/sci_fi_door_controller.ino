/*
 Sci-Fi Door Controller v0.2.0
 ESP32-C3 SuperMini + SSD1306 + limit switch + WS2812B
 Standalone controller with Wi-Fi AP web configuration and NVS settings.
*/
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

static const uint8_t PIN_PIXELS=2, PIN_LIMIT_SWITCH=4, PIN_OLED_SDA=5, PIN_OLED_SCL=6;
static const uint8_t OLED_ADDRESS=0x3C;
static const uint16_t SCREEN_WIDTH=128, SCREEN_HEIGHT=64, MAX_PIXELS=1024;
static const int8_t OLED_RESET=-1;
static const uint32_t SWITCH_DEBOUNCE_MS=35;
static const char* FW_VERSION="0.3.0";
static const char* AP_SSID="SCI-FI-DOOR";
static const char* AP_PASSWORD="portal123";

struct Settings {
 uint16_t pixelCount=60;
 uint8_t brightness=100;
 uint16_t openingSpeed=18;
 uint16_t closingSpeed=18;
 uint16_t idleSpeed=35;
 uint8_t red=0, green=170, blue=255;
 uint8_t closedEffect=0; // 0 off, 1 solid, 2 breathe
 uint8_t openEffect=1;   // 0 solid, 1 scanner, 2 breathe
} cfg;

Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire,OLED_RESET);
Adafruit_NeoPixel pixels(MAX_PIXELS,PIN_PIXELS,NEO_GRB+NEO_KHZ800);
WebServer server(80);
Preferences prefs;

enum DoorState { DOOR_CLOSED, DOOR_OPENING, DOOR_OPEN, DOOR_CLOSING };
DoorState doorState=DOOR_CLOSED;
bool rawSwitchState=LOW, stableSwitchState=LOW, previousStableSwitchState=LOW, oledReady=false;
uint32_t switchChangedAt=0, animationChangedAt=0;
uint16_t animationStep=0, idlePosition=0;
float breathePhase=0;

uint16_t activePixels(){ return constrain(cfg.pixelCount,(uint16_t)1,MAX_PIXELS); }
uint32_t configuredColour(uint8_t scale=255){
 return pixels.Color((uint16_t)cfg.red*scale/255,(uint16_t)cfg.green*scale/255,(uint16_t)cfg.blue*scale/255);
}
const char* doorStateName(){
 switch(doorState){case DOOR_CLOSED:return "CLOSED";case DOOR_OPENING:return "OPENING";case DOOR_OPEN:return "OPEN";case DOOR_CLOSING:return "CLOSING";}
 return "UNKNOWN";
}
void clearPixels(){pixels.clear();pixels.show();}
void applyPixelSettings(){pixels.setBrightness(cfg.brightness);clearPixels();}
void loadSettings(){
 prefs.begin("scifidoor",true);
 cfg.pixelCount=prefs.getUShort("pixels",60); cfg.brightness=prefs.getUChar("bright",100);
 cfg.openingSpeed=prefs.getUShort("openSpd",18); cfg.closingSpeed=prefs.getUShort("closeSpd",18); cfg.idleSpeed=prefs.getUShort("idleSpd",35);
 cfg.red=prefs.getUChar("red",0); cfg.green=prefs.getUChar("green",170); cfg.blue=prefs.getUChar("blue",255);
 cfg.closedEffect=prefs.getUChar("closedFx",0); cfg.openEffect=prefs.getUChar("openFx",1); prefs.end();
 cfg.pixelCount=constrain(cfg.pixelCount,(uint16_t)1,MAX_PIXELS); cfg.brightness=constrain(cfg.brightness,(uint8_t)1,(uint8_t)255);
}
void saveSettings(){
 prefs.begin("scifidoor",false);
 prefs.putUShort("pixels",cfg.pixelCount);prefs.putUChar("bright",cfg.brightness);
 prefs.putUShort("openSpd",cfg.openingSpeed);prefs.putUShort("closeSpd",cfg.closingSpeed);prefs.putUShort("idleSpd",cfg.idleSpeed);
 prefs.putUChar("red",cfg.red);prefs.putUChar("green",cfg.green);prefs.putUChar("blue",cfg.blue);
 prefs.putUChar("closedFx",cfg.closedEffect);prefs.putUChar("openFx",cfg.openEffect);prefs.end();
}
void updateDisplay(){
 if(!oledReady)return;
 display.clearDisplay();display.setTextColor(SSD1306_WHITE);display.setTextSize(1);
 display.setCursor(0,0);display.println("SCI-FI DOOR");display.drawLine(0,10,127,10,SSD1306_WHITE);
 display.setCursor(0,15);display.print("DOOR: ");display.println(doorStateName());
 display.setCursor(0,26);display.print("PIXELS: ");display.println(activePixels());
 display.setCursor(0,37);display.print("AP: ");display.println(AP_SSID);
 display.setCursor(0,48);display.print("IP: ");display.println(WiFi.softAPIP());
 display.setCursor(92,0);display.print("v");display.print(FW_VERSION);display.display();
}
void setDoorState(DoorState s){
 if(doorState==s)return;doorState=s;animationStep=0;idlePosition=0;breathePhase=0;animationChangedAt=millis();
 Serial.print("[DOOR] State -> ");Serial.println(doorStateName());updateDisplay();
}
void readLimitSwitch(){
 bool reading=digitalRead(PIN_LIMIT_SWITCH);
 if(reading!=rawSwitchState){rawSwitchState=reading;switchChangedAt=millis();}
 if(millis()-switchChangedAt>=SWITCH_DEBOUNCE_MS&&stableSwitchState!=rawSwitchState){
  previousStableSwitchState=stableSwitchState;stableSwitchState=rawSwitchState;
  Serial.println(stableSwitchState==LOW?"[SWITCH] PRESSED - door closed":"[SWITCH] RELEASED - door opening");
  if(previousStableSwitchState==LOW&&stableSwitchState==HIGH)setDoorState(DOOR_OPENING);
  if(previousStableSwitchState==HIGH&&stableSwitchState==LOW)setDoorState(DOOR_CLOSING);
 }
}
void fillConfigured(uint8_t scale=255){for(uint16_t i=0;i<activePixels();i++)pixels.setPixelColor(i,configuredColour(scale));pixels.show();}
void runBreathe(){
 if(millis()-animationChangedAt<cfg.idleSpeed)return;animationChangedAt=millis();
 breathePhase+=0.08f;if(breathePhase>6.283f)breathePhase=0;
 uint8_t level=(uint8_t)(45.0f+210.0f*((sinf(breathePhase)+1.0f)/2.0f));pixels.clear();fillConfigured(level);
}
void runOpeningEffect(){
 if(millis()-animationChangedAt<cfg.openingSpeed)return;animationChangedAt=millis();pixels.clear();
 int cl=(activePixels()-1)/2,cr=activePixels()/2;
 for(uint8_t tail=0;tail<5;tail++){if(animationStep<tail)continue;int d=animationStep-tail,l=cl-d,r=cr+d;uint8_t level=255-tail*45;
  if(l>=0&&l<activePixels())pixels.setPixelColor(l,configuredColour(level));
  if(r>=0&&r<activePixels())pixels.setPixelColor(r,configuredColour(level));}
 pixels.show();animationStep++;if(animationStep>(activePixels()/2)+6)setDoorState(DOOR_OPEN);
}
void runScanner(){
 if(millis()-animationChangedAt<cfg.idleSpeed)return;animationChangedAt=millis();pixels.clear();
 for(uint8_t tail=0;tail<8;tail++){int index=(int)idlePosition-tail;while(index<0)index+=activePixels();index%=activePixels();uint8_t level=220-tail*25;pixels.setPixelColor(index,configuredColour(level));}
 pixels.show();idlePosition=(idlePosition+1)%activePixels();
}
void runOpenEffect(){if(cfg.openEffect==0){fillConfigured();delay(1);}else if(cfg.openEffect==1)runScanner();else runBreathe();}
void runClosedEffect(){if(cfg.closedEffect==0){return;}if(cfg.closedEffect==1){fillConfigured(80);delay(1);}else runBreathe();}
void runClosingEffect(){
 if(millis()-animationChangedAt<cfg.closingSpeed)return;animationChangedAt=millis();pixels.clear();uint16_t half=(activePixels()+1)/2;
 for(uint16_t d=animationStep;d<half;d++){int l=d,r=activePixels()-1-d;if(l>=0&&l<activePixels())pixels.setPixelColor(l,configuredColour(180));if(r>=0&&r<activePixels())pixels.setPixelColor(r,configuredColour(180));}
 pixels.show();animationStep++;if(animationStep>=half){clearPixels();setDoorState(DOOR_CLOSED);}
}
String option(uint8_t value,uint8_t current,const char* label){return String("<option value='")+value+"'"+(value==current?" selected":"")+">"+label+"</option>";}
String page(){
 String h;h.reserve(7000);
 h+="<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>Sci-Fi Door</title><style>";
 h+="body{margin:0;background:#071017;color:#dffcff;font-family:Arial,sans-serif}main{max-width:720px;margin:auto;padding:24px}.card{background:#0c1b24;border:1px solid #1d7185;border-radius:16px;padding:20px;margin:14px 0;box-shadow:0 0 25px #00c8ff18}h1{color:#65e8ff}label{display:block;margin:14px 0 5px;color:#91ddea}input,select,button{width:100%;box-sizing:border-box;padding:12px;border-radius:9px;border:1px solid #286475;background:#071017;color:white}button{margin-top:18px;background:#0d91aa;font-weight:bold;cursor:pointer}.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}.status{font-size:1.15em;color:#6fffc8}@media(max-width:560px){.grid{grid-template-columns:1fr}}</style></head><body><main>";
 h+="<h1>SCI-FI DOOR</h1><div class='card'><div class='status'>Door: "+String(doorStateName())+"</div><p>Firmware "+String(FW_VERSION)+" &bull; AP "+String(AP_SSID)+" &bull; "+WiFi.softAPIP().toString()+"</p></div>";
 h+="<form class='card' method='POST' action='/save'><h2>Lighting</h2><div class='grid'>";
 h+="<div><label>Pixel count</label><input name='pixels' type='number' min='1' max='1024' value='"+String(cfg.pixelCount)+"'></div>";
 h+="<div><label>Brightness (1-255)</label><input name='brightness' type='number' min='1' max='255' value='"+String(cfg.brightness)+"'></div></div>";
 h+="<label>Colour</label><input name='colour' type='color' value='#"+String(cfg.red<16?"0":"")+String(cfg.red,HEX)+String(cfg.green<16?"0":"")+String(cfg.green,HEX)+String(cfg.blue<16?"0":"")+String(cfg.blue,HEX)+"'>";
 h+="<div class='grid'><div><label>Closed effect</label><select name='closedFx'>"+option(0,cfg.closedEffect,"Off")+option(1,cfg.closedEffect,"Dim solid")+option(2,cfg.closedEffect,"Breathe")+option(3,cfg.closedEffect,"Perimeter chase")+option(4,cfg.closedEffect,"Warning pulse")+option(5,cfg.closedEffect,"Reactor")+"</select></div>";
 h+="<div><label>Open effect</label><select name='openFx'>"+option(0,cfg.openEffect,"Solid")+option(1,cfg.openEffect,"Scanner")+option(2,cfg.openEffect,"Breathe")+option(3,cfg.openEffect,"Perimeter chase")+option(4,cfg.openEffect,"Warning pulse")+option(5,cfg.openEffect,"Reactor")+option(6,cfg.openEffect,"Energy shimmer")+option(7,cfg.openEffect,"Dual sweep")+"</select></div></div>";
 h+="<h2>Timing</h2><div class='grid'><div><label>Opening step ms</label><input name='openSpd' type='number' min='5' max='500' value='"+String(cfg.openingSpeed)+"'></div><div><label>Closing step ms</label><input name='closeSpd' type='number' min='5' max='500' value='"+String(cfg.closingSpeed)+"'></div><div><label>Idle step ms</label><input name='idleSpd' type='number' min='10' max='1000' value='"+String(cfg.idleSpeed)+"'></div></div>";
 h+="<button type='submit'>SAVE SETTINGS</button></form><div class='card'><form method='POST' action='/preview'><button>PREVIEW OPEN EFFECT</button></form></div></main></body></html>";return h;
}
uint8_t hexByte(String s){return (uint8_t)strtoul(s.c_str(),nullptr,16);}
void handleSave(){
 cfg.pixelCount=constrain(server.arg("pixels").toInt(),1,(int)MAX_PIXELS);cfg.brightness=constrain(server.arg("brightness").toInt(),1,255);
 cfg.openingSpeed=constrain(server.arg("openSpd").toInt(),5,500);cfg.closingSpeed=constrain(server.arg("closeSpd").toInt(),5,500);cfg.idleSpeed=constrain(server.arg("idleSpd").toInt(),10,1000);
 cfg.closedEffect=constrain(server.arg("closedFx").toInt(),0,5);cfg.openEffect=constrain(server.arg("openFx").toInt(),0,7);
 String c=server.arg("colour");if(c.length()==7){cfg.red=hexByte(c.substring(1,3));cfg.green=hexByte(c.substring(3,5));cfg.blue=hexByte(c.substring(5,7));}
 saveSettings();applyPixelSettings();animationStep=0;idlePosition=0;breathePhase=0;updateDisplay();
 Serial.println("[CONFIG] Settings saved to NVS");server.sendHeader("Location","/");server.send(303);
}
void handlePreview(){setDoorState(DOOR_OPENING);server.sendHeader("Location","/");server.send(303);}
void startWeb(){
 WiFi.mode(WIFI_AP);WiFi.softAP(AP_SSID,AP_PASSWORD);
 Serial.print("[WIFI] AP ");Serial.print(AP_SSID);Serial.print(" IP ");Serial.println(WiFi.softAPIP());
 server.on("/",HTTP_GET,[](){server.send(200,"text/html",page());});
 server.on("/save",HTTP_POST,handleSave);server.on("/preview",HTTP_POST,handlePreview);
 server.onNotFound([](){server.sendHeader("Location","/");server.send(302);});server.begin();Serial.println("[WEB] Configuration server ready");
}
void setup(){
 Serial.begin(115200);delay(250);Serial.println("\n================================\n SCI-FI DOOR CONTROLLER v0.3.0\n================================");
 loadSettings();pinMode(PIN_LIMIT_SWITCH,INPUT_PULLUP);rawSwitchState=digitalRead(PIN_LIMIT_SWITCH);stableSwitchState=rawSwitchState;previousStableSwitchState=stableSwitchState;switchChangedAt=millis();
 pixels.begin();applyPixelSettings();Wire.begin(PIN_OLED_SDA,PIN_OLED_SCL);oledReady=display.begin(SSD1306_SWITCHCAPVCC,OLED_ADDRESS);
 if(oledReady){display.setRotation(2);Serial.println("[OLED] Online");}else Serial.println("[OLED] WARNING: not detected");
 startWeb();doorState=stableSwitchState==LOW?DOOR_CLOSED:DOOR_OPEN;updateDisplay();
 Serial.print("[CONFIG] pixels=");Serial.print(activePixels());Serial.print(" brightness=");Serial.println(cfg.brightness);Serial.println("[SYSTEM] Ready");
}
void loop(){
 server.handleClient();readLimitSwitch();
 switch(doorState){case DOOR_CLOSED:runClosedEffect();break;case DOOR_OPENING:runOpeningEffect();break;case DOOR_OPEN:runOpenEffect();break;case DOOR_CLOSING:runClosingEffect();break;}
}
