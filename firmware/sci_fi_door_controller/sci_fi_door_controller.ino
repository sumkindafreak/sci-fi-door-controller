/*
 Sci-Fi Door Controller v0.1.0
 ESP32-C3 SuperMini + SSD1306 OLED + limit switch + WS2812B
 OLED SDA=5, SCL=6, limit switch GPIO4->GND, pixels GPIO2.
 Door CLOSED = pressed/LOW. Door OPEN = released/HIGH.
*/
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

static const uint8_t PIN_PIXELS=2, PIN_LIMIT_SWITCH=4, PIN_OLED_SDA=5, PIN_OLED_SCL=6;
static const uint8_t OLED_ADDRESS=0x3C;
static const uint16_t SCREEN_WIDTH=128, SCREEN_HEIGHT=64, PIXEL_COUNT=60;
static const int8_t OLED_RESET=-1;
static const uint8_t MASTER_BRIGHTNESS=100;
static const uint32_t SWITCH_DEBOUNCE_MS=35, OPENING_STEP_MS=18, CLOSING_STEP_MS=18, IDLE_STEP_MS=35;

Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire,OLED_RESET);
Adafruit_NeoPixel pixels(PIXEL_COUNT,PIN_PIXELS,NEO_GRB+NEO_KHZ800);

enum DoorState { DOOR_CLOSED, DOOR_OPENING, DOOR_OPEN, DOOR_CLOSING };
DoorState doorState=DOOR_CLOSED;
bool rawSwitchState=LOW, stableSwitchState=LOW, previousStableSwitchState=LOW, oledReady=false;
uint32_t switchChangedAt=0, animationChangedAt=0;
uint16_t animationStep=0, idlePosition=0;

const char* doorStateName(){
 switch(doorState){
  case DOOR_CLOSED:return "CLOSED"; case DOOR_OPENING:return "OPENING";
  case DOOR_OPEN:return "OPEN"; case DOOR_CLOSING:return "CLOSING";
 }
 return "UNKNOWN";
}
void clearPixels(){ pixels.clear(); pixels.show(); }
void updateDisplay(){
 if(!oledReady)return;
 display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
 display.setCursor(0,0); display.println("SCI-FI DOOR"); display.drawLine(0,10,127,10,SSD1306_WHITE);
 display.setCursor(0,16); display.print("DOOR: "); display.println(doorStateName());
 display.setCursor(0,28); display.print("PIXELS: "); display.println(PIXEL_COUNT);
 display.setCursor(0,40); display.print("SWITCH: "); display.println(stableSwitchState==LOW?"PRESSED":"RELEASED");
 display.setCursor(0,52); display.print("FW: 0.1.0"); display.display();
}
void setDoorState(DoorState s){
 if(doorState==s)return; doorState=s; animationStep=0; idlePosition=0; animationChangedAt=millis();
 Serial.print("[DOOR] State -> "); Serial.println(doorStateName()); updateDisplay();
}
void readLimitSwitch(){
 bool reading=digitalRead(PIN_LIMIT_SWITCH);
 if(reading!=rawSwitchState){rawSwitchState=reading;switchChangedAt=millis();}
 if((millis()-switchChangedAt)>=SWITCH_DEBOUNCE_MS && stableSwitchState!=rawSwitchState){
  previousStableSwitchState=stableSwitchState; stableSwitchState=rawSwitchState;
  Serial.print("[SWITCH] "); Serial.println(stableSwitchState==LOW?"PRESSED - door closed":"RELEASED - door opening");
  if(previousStableSwitchState==LOW && stableSwitchState==HIGH)setDoorState(DOOR_OPENING);
  if(previousStableSwitchState==HIGH && stableSwitchState==LOW)setDoorState(DOOR_CLOSING);
  updateDisplay();
 }
}
void runOpeningEffect(){
 if(millis()-animationChangedAt<OPENING_STEP_MS)return; animationChangedAt=millis(); pixels.clear();
 const int centreLeft=(PIXEL_COUNT-1)/2, centreRight=PIXEL_COUNT/2;
 for(uint8_t tail=0;tail<5;tail++){
  if(animationStep<tail)continue; int distance=animationStep-tail;
  int left=centreLeft-distance,right=centreRight+distance; uint8_t level=255-(tail*45);
  uint32_t colour=pixels.Color(level/2,level,level);
  if(left>=0&&left<PIXEL_COUNT)pixels.setPixelColor(left,colour);
  if(right>=0&&right<PIXEL_COUNT)pixels.setPixelColor(right,colour);
 }
 pixels.show(); animationStep++;
 if(animationStep>((PIXEL_COUNT/2)+6))setDoorState(DOOR_OPEN);
}
void runOpenIdleEffect(){
 if(millis()-animationChangedAt<IDLE_STEP_MS)return; animationChangedAt=millis(); pixels.clear();
 for(uint8_t tail=0;tail<8;tail++){
  int index=(int)idlePosition-tail; while(index<0)index+=PIXEL_COUNT; index%=PIXEL_COUNT;
  uint8_t level=220-(tail*25); pixels.setPixelColor(index,pixels.Color(0,level/2,level));
 }
 pixels.show(); idlePosition=(idlePosition+1)%PIXEL_COUNT;
}
void runClosingEffect(){
 if(millis()-animationChangedAt<CLOSING_STEP_MS)return; animationChangedAt=millis(); pixels.clear();
 const uint16_t half=(PIXEL_COUNT+1)/2;
 for(uint16_t distance=animationStep;distance<half;distance++){
  int left=distance,right=PIXEL_COUNT-1-distance;
  if(left>=0&&left<PIXEL_COUNT)pixels.setPixelColor(left,pixels.Color(0,80,160));
  if(right>=0&&right<PIXEL_COUNT)pixels.setPixelColor(right,pixels.Color(0,80,160));
 }
 pixels.show(); animationStep++;
 if(animationStep>=half){clearPixels();setDoorState(DOOR_CLOSED);}
}
void runClosedEffect(){ /* v0.1.0: intentionally dark while closed */ }

void setup(){
 Serial.begin(115200); delay(250);
 Serial.println("\n================================\n SCI-FI DOOR CONTROLLER v0.1.0\n================================");
 pinMode(PIN_LIMIT_SWITCH,INPUT_PULLUP);
 rawSwitchState=digitalRead(PIN_LIMIT_SWITCH); stableSwitchState=rawSwitchState; previousStableSwitchState=stableSwitchState; switchChangedAt=millis();
 pixels.begin(); pixels.setBrightness(MASTER_BRIGHTNESS); clearPixels();
 Wire.begin(PIN_OLED_SDA,PIN_OLED_SCL);
 oledReady=display.begin(SSD1306_SWITCHCAPVCC,OLED_ADDRESS);
 if(oledReady){display.setRotation(2);Serial.println("[OLED] SSD1306 online at 0x3C");}
 else Serial.println("[OLED] WARNING: SSD1306 not detected");
 doorState=(stableSwitchState==LOW)?DOOR_CLOSED:DOOR_OPEN;
 Serial.println(stableSwitchState==LOW?"[BOOT] Door CLOSED":"[BOOT] Door OPEN");
 updateDisplay();
 Serial.print("[PIXELS] GPIO 2, count ");Serial.println(PIXEL_COUNT);Serial.println("[SYSTEM] Ready");
}
void loop(){
 readLimitSwitch();
 switch(doorState){
  case DOOR_CLOSED:runClosedEffect();break; case DOOR_OPENING:runOpeningEffect();break;
  case DOOR_OPEN:runOpenIdleEffect();break; case DOOR_CLOSING:runClosingEffect();break;
 }
}
