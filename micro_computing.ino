```cpp
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <avr/pgmspace.h>

#define LCD_RS PC1
#define LCD_RW PC0
#define LCD_E  PD2
#define LCD_D4 PC2
#define LCD_D5 PC3
#define LCD_D6 PC4
#define LCD_D7 PC5

const uint8_t rowPins[4] = {7, 8, 9, 10};
const uint8_t colPins[4] = {3, 4, 5, 6};

#define MAX_FILES 5
#define MAX_FILENAME 12
#define MAX_CONTENT 128
#define MULTITAP_TIMEOUT 800

struct File {
  char name[MAX_FILENAME];
  char content[MAX_CONTENT];
  uint8_t length;
};

File files[MAX_FILES];
uint8_t fileCount = 0;

enum State { MENU, NOTEBOOK, SAVE_NAME, HISTORY };
State state = MENU;

char buffer[MAX_CONTENT] = "";
uint8_t bufferLen = 0;
uint8_t cursorPos = 0;

char filenameInput[MAX_FILENAME] = "";
uint8_t filenameLen = 0;

uint8_t fileIndex = 0;

const char* const multitapMap[] PROGMEM = {
    " 0", ".,1", "ABC2", "DEF3", "GHI4", 
    "JKL5", "MNO6", "PQRS7", "TUV8", "WXYZ9"
};

struct {
    uint8_t lastKey;
    uint8_t tapIndex;
    uint32_t lastTapTime;
    bool inMultitap;
} inputState;

void lcdSendNibble(uint8_t nibble) {
  PORTC = (PORTC & 0xC3) | (nibble << 2);
  PORTD |= (1 << LCD_E);
  _delay_us(1);
  PORTD &= ~(1 << LCD_E);
  _delay_us(80);
}

void lcdCommand(uint8_t cmd) {
  PORTC &= ~((1 << LCD_RS) | (1 << LCD_RW));
  lcdSendNibble(cmd >> 4);
  lcdSendNibble(cmd & 0x0F);
  _delay_ms(2);
}

void lcdData(uint8_t data) {
  PORTC = (PORTC & ~(1 << LCD_RW)) | (1 << LCD_RS);
  lcdSendNibble(data >> 4);
  lcdSendNibble(data & 0x0F);
  _delay_ms(2);
}

void lcdInit() {
  DDRC |= 0b111111;
  DDRD |= (1 << LCD_E);
  _delay_ms(50);
  lcdSendNibble(0x03); _delay_ms(5);
  lcdSendNibble(0x03); _delay_ms(5);
  lcdSendNibble(0x03); _delay_us(150);
  lcdSendNibble(0x02);
  lcdCommand(0x28);
  lcdCommand(0x0C);
  lcdCommand(0x06);
  lcdCommand(0x01);
  _delay_ms(2);
}

void lcdClear() {
  lcdCommand(0x01);
  _delay_ms(2);
}

void lcdSetCursor(uint8_t col, uint8_t row) {
  lcdCommand(0x80 | (col + (row == 1 ? 0x40 : 0)));
}

void lcdPrint(const char* str) {
  while (*str) lcdData(*str++);
}

void lcdPrint_P(const char* str) {
    char c;
    while ((c = pgm_read_byte(str++))) {
        lcdData(c);
    }
}

void keypadInit() {
    for(uint8_t i=0; i<4; i++){
        pinMode(rowPins[i], OUTPUT);
        digitalWrite(rowPins[i], HIGH);
    }
    for(uint8_t i=0; i<4; i++){
        pinMode(colPins[i], INPUT_PULLUP);
    }
}

int getKey() {
    for(uint8_t r=0; r<4; r++){
        digitalWrite(rowPins[r], LOW);
        for(uint8_t c=0; c<4; c++){
            if(!digitalRead(colPins[c])){
                _delay_ms(50);
                while(!digitalRead(colPins[c]));
                digitalWrite(rowPins[r], HIGH);
                return r*4 + c;
            }
        }
        digitalWrite(rowPins[r], HIGH);
    }
    return -1;
}

void deleteChar() {
    if(bufferLen == 0 || cursorPos == 0) return;
    
    for(uint8_t i=cursorPos-1; i<bufferLen; i++){
        buffer[i] = buffer[i+1];
    }
    
    bufferLen--;
    cursorPos--;
    buffer[bufferLen] = '\0';
}

void saveFile() {
    if(fileCount < MAX_FILES){
        strcpy(files[fileCount].name, filenameInput);
        strcpy(files[fileCount].content, buffer);
        files[fileCount].length = bufferLen;
        fileCount++;
    }
}

void deleteCurrentFile() {
    if(fileCount == 0) return;
    
    for(uint8_t i=fileIndex; i<fileCount-1; i++){
        files[i] = files[i+1];
    }
    fileCount--;
    if(fileIndex >= fileCount) fileIndex = fileCount-1;
}

void updateDisplay() {
    lcdClear();
    lcdSetCursor(0,0);
    
    switch(state){
        case MENU:
            lcdPrint_P(PSTR("-MicroComputing-"));
            lcdSetCursor(0,1);
            lcdPrint_P(PSTR("1.Editar 2.Historico"));
            break;
            
        case NOTEBOOK:
            for(uint8_t i=0; i<16; i++){
                lcdData((i < bufferLen) ? buffer[i] : ' ');
            }
            lcdSetCursor(0,1);
            for(uint8_t i=0; i<16; i++){
                lcdData((i == cursorPos) ? '^' : ' ');
            }
            break;
            
        case SAVE_NAME:
            lcdPrint_P(PSTR("-MicroComputing-"));
            lcdSetCursor(0,1);
            lcdPrint("Nome:");
            lcdPrint(filenameInput);
            break;
            
        case HISTORY:
            lcdPrint_P(PSTR("-MicroComputing-"));
            lcdSetCursor(0,1);
            if(fileCount == 0){
                lcdPrint_P(PSTR("Sem arquivos"));
            } else {
                lcdPrint(files[fileIndex].name);
                lcdPrint(" ");
                lcdPrint((char)('0'+fileIndex+1));
                lcdPrint("/");
                lcdPrint((char)('0'+fileCount));
            }
            break;
    }
}

void setup() {
    lcdInit();
    keypadInit();
    updateDisplay();
}

void loop() {
    int key = getKey();
    uint32_t now = millis();
    
    if(key != -1){
        switch(state){
            case MENU:
                if(key == 0){
                    buffer[0] = '\0';
                    bufferLen = 0;
                    cursorPos = 0;
                    state = NOTEBOOK;
                }
                else if(key == 1){
                    fileIndex = 0;
                    state = HISTORY;
                }
                break;
                
            case NOTEBOOK:
                if(key == 14){
                    state = SAVE_NAME;
                    filenameInput[0] = '\0';
                    filenameLen = 0;
                }
                else if(key == 15){
                    deleteChar();
                }
                else if(key >= 2 && key <= 11){
                    if(key-2 != inputState.lastKey || now - inputState.lastTapTime > MULTITAP_TIMEOUT){
                        inputState.lastKey = key-2;
                        inputState.tapIndex = 0;
                    } else {
                        inputState.tapIndex++;
                    }
                    
                    const char* map = (const char*)pgm_read_ptr(&multitapMap[key-2]);
                    uint8_t len = strlen_P(map);
                    if(len > 0){
                        inputState.tapIndex %= len;
                        char c = pgm_read_byte(&map[inputState.tapIndex]);
                        
                        if(bufferLen < MAX_CONTENT-1){
                            for(uint8_t i=bufferLen; i>cursorPos; i--){
                                buffer[i] = buffer[i-1];
                            }
                            buffer[cursorPos] = c;
                            bufferLen++;
                            cursorPos++;
                            buffer[bufferLen] = '\0';
                        }
                    }
                    inputState.lastTapTime = now;
                }
                else if(key == 4){
                    if(cursorPos > 0) cursorPos--;
                }
                else if(key == 6){
                    if(cursorPos < bufferLen) cursorPos++;
                }
                break;
                
            case SAVE_NAME:
                if(key == 14){
                    state = NOTEBOOK;
                }
                else if(key == 15){
                    saveFile();
                    state = MENU;
                }
                else if(key == 4){
                    if(filenameLen > 0) filenameInput[--filenameLen] = '\0';
                }
                else if(key >= 2 && key <= 11){
                    if(key-2 != inputState.lastKey || now - inputState.lastTapTime > MULTITAP_TIMEOUT){
                        inputState.lastKey = key-2;
                        inputState.tapIndex = 0;
                    } else {
                        inputState.tapIndex++;
                    }
                    
                    const char* map = (const char*)pgm_read_ptr(&multitapMap[key-2]);
                    uint8_t len = strlen_P(map);
                    if(len > 0 && filenameLen < MAX_FILENAME-1){
                        inputState.tapIndex %= len;
                        filenameInput[filenameLen++] = pgm_read_byte(&map[inputState.tapIndex]);
                        filenameInput[filenameLen] = '\0';
                    }
                    inputState.lastTapTime = now;
                }
                break;
                
            case HISTORY:
                if(key == 4){
                    if(fileIndex > 0) fileIndex--;
                }
                else if(key == 6){
                    if(fileIndex < fileCount-1) fileIndex++;
                }
                else if(key == 0){
                    strcpy(buffer, files[fileIndex].content);
                    bufferLen = files[fileIndex].length;
                    cursorPos = 0;
                    state = NOTEBOOK;
                }
                else if(key == 1){
                    state = MENU;
                }
                else if(key == 14){
                    deleteCurrentFile();
                }
                break;
        }
        updateDisplay();
    }
}
```
