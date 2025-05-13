#include <Keypad.h>
#include <LiquidCrystal.h>
#include <avr/pgmspace.h>
#include <string.h>

#define MAX_CHARS 16
#define TAP_DELAY 800
#define BLINK_INTERVAL 500

const char map0[] PROGMEM = "ABC2";
const char map1[] PROGMEM = "DEF3";
const char map2[] PROGMEM = "GHI4";
const char map3[] PROGMEM = "JKL5";
const char map4[] PROGMEM = "MNO6";
const char map5[] PROGMEM = "PQRS7";
const char map6[] PROGMEM = "TUV8";
const char map7[] PROGMEM = "WXYZ9";
const char map8[] PROGMEM = ".,?!";
const char map9[] PROGMEM = "-_/;:'";
const char* const maps[] PROGMEM = {
  map0,map1,map2,map3,map4,map5,map6,map7,map8,map9
};

char buffer[MAX_CHARS+1] = {0};
uint8_t bufLen = 0;
uint8_t cursorPos = 0;
int8_t lastKey = -1;
uint8_t tapIndex = 0;
unsigned long lastTapTime = 0;
bool blinkState = false;
unsigned long lastBlinkTime = 0;

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {4,5,6,7};
byte colPins[COLS] = {8,9,10,11};
Keypad kpd = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

LiquidCrystal lcd(A0,A1,A2,A3,A4,A5);

void setup(){
  lcd.begin(16,2);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Notebook:");
}

void loop(){
  unsigned long now = millis();
  if(now - lastBlinkTime >= BLINK_INTERVAL){
    blinkState = !blinkState;
    lastBlinkTime = now;
  }
  char k = kpd.getKey();
  if(k){
    if(k=='*'){
      if(cursorPos>0){
        memmove(buffer+cursorPos-1, buffer+cursorPos, bufLen-cursorPos);
        bufLen--; cursorPos--;
        buffer[bufLen] = '\0';
      }
      lastKey = -1;
    }
    else if(k=='A'){
      if(cursorPos>0) cursorPos--;
      lastKey = -1;
    }
    else if(k=='B'){
      if(cursorPos<bufLen) cursorPos++;
      lastKey = -1;
    }
    else if(k>='0' && k<='9'){
      int idx = k - '0';
      const char* ptr = (const char*)pgm_read_ptr(&maps[idx]);
      uint8_t len = strlen_P(ptr);
      char ch;
      if(lastKey==idx && now - lastTapTime < TAP_DELAY && cursorPos>0){
        tapIndex = (tapIndex+1)%len;
        ch = pgm_read_byte(ptr+tapIndex);
        buffer[cursorPos-1] = ch;
      } else if(bufLen < MAX_CHARS){
        tapIndex = 0;
        ch = pgm_read_byte(ptr);
        memmove(buffer+cursorPos+1, buffer+cursorPos, bufLen-cursorPos);
        buffer[cursorPos++] = ch;
        bufLen++;
        buffer[bufLen] = '\0';
      }
      lastKey = idx;
      lastTapTime = now;
    }
  }
  if(lastKey>=0 && now - lastTapTime >= TAP_DELAY){
    lastKey = -1;
    tapIndex = 0;
    if(cursorPos < bufLen) cursorPos++;
  }
  lcd.setCursor(0,1);
  for(uint8_t i=0; i<MAX_CHARS; i++){
    if(i<bufLen){
      if(i==cursorPos && blinkState) lcd.print('_');
      else lcd.print(buffer[i]);
    } else {
      if(i==cursorPos && blinkState) lcd.print('_');
      else lcd.print(' ');
    }
  }
}
