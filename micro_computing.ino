// MicroComputing - Otimizado com controle direto do LCD e teclado
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>

// === Configurações de LCD (modo 4 bits via PORTC e PD2) ===
#define LCD_RS PC1
#define LCD_RW PC0
#define LCD_E  PD2
#define LCD_D4 PC2
#define LCD_D5 PC3
#define LCD_D6 PC4
#define LCD_D7 PC5

// === Teclado matricial ===
const uint8_t rowPins[4] = {7, 8, 9, 10};
const uint8_t colPins[4] = {3, 4, 5, 6};

// === Constantes e definições ===
#define MAX_FILES 5
#define MAX_FILENAME 10
#define MAX_CONTENT 128
#define MULTITAP_TIMEOUT 1000

struct File {
  char name[MAX_FILENAME];
  char content[MAX_CONTENT];
  uint8_t length;
};

File files[MAX_FILES];
uint8_t fileCount = 0;

enum State { MENU, NOTEBOOK, SAVE_NAME, HISTORY, READ_FILE };
State state = MENU;

char buffer[MAX_CONTENT] = "";
uint8_t bufferLen = 0;
uint8_t cursorPos = 0;
uint8_t contentOffset = 0;

char filenameInput[MAX_FILENAME] = "";
uint8_t filenameLen = 0;

uint8_t menuIndex = 0;
uint8_t fileIndex = 0;

const char* multitapMap[] = {
  " ",    ".,1",  "ABC2", "DEF3",
  "GHI4", "JKL5", "MNO6", "PQRS7",
  "TUV8", "WXYZ9"
};

char lastKey = 0;
uint8_t tapIndex = 0;
unsigned long lastTapTime = 0;

// === Funções de LCD direto ===
void lcdSendNibble(uint8_t nibble) {
  PORTC = (PORTC & 0xC3) | (nibble << 2); // bits PC2-PC5
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

// === Teclado ===
void keypadInit() {
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], HIGH);
  }
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(colPins[i], INPUT_PULLUP);
  }
}

int getKey() {
  for (uint8_t r = 0; r < 4; r++) {
    digitalWrite(rowPins[r], LOW);
    for (uint8_t c = 0; c < 4; c++) {
      if (!digitalRead(colPins[c])) {
        delay(150);
        while (!digitalRead(colPins[c]));
        digitalWrite(rowPins[r], HIGH);
        return r * 4 + c;
      }
    }
    digitalWrite(rowPins[r], HIGH);
  }
  return -1;
}

// === Utilitários ===
void insertChar(char ch) {
  if (bufferLen < MAX_CONTENT) {
    for (int i = bufferLen; i > cursorPos; i--)
      buffer[i] = buffer[i-1];
    buffer[cursorPos] = ch;
    bufferLen++;
    cursorPos++;
  }
}

void deleteChar() {
  if (cursorPos > 0 && bufferLen > 0) {
    for (int i = cursorPos - 1; i < bufferLen; i++)
      buffer[i] = buffer[i + 1];
    bufferLen--;
    cursorPos--;
  }
}

void updateDisplay() {
  lcdClear();
  lcdSetCursor(0,0);
  switch(state) {
    case MENU:
      lcdPrint("- MicroComputing");
      lcdSetCursor(0,1);
      if (menuIndex == 0) lcdPrint(">Notebook");
      else lcdPrint(">History");
      break;
    case NOTEBOOK:
    case READ_FILE:
      for (int i = 0; i < 16; i++) {
        char ch = (contentOffset + i < bufferLen) ? buffer[contentOffset + i] : ' ';
        lcdData(ch);
      }
      lcdSetCursor(cursorPos - contentOffset, 1);
      lcdData('^');
      break;
    case SAVE_NAME:
      lcdPrint("Name:");
      lcdSetCursor(0,1);
      lcdPrint(filenameInput);
      break;
    case HISTORY:
      if (fileCount == 0) lcdPrint("No files.");
      else {
        lcdSetCursor(0,0); lcdPrint("Files:");
        lcdSetCursor(0,1); lcdPrint(files[fileIndex].name);
      }
      break;
  }
}

// === Setup e Loop ===
void setup() {
  lcdInit();
  keypadInit();
  updateDisplay();
}

void loop() {
  int key = getKey();
  unsigned long now = millis();
  if (state == MENU) {
    if (key == 12) menuIndex = 0;
    if (key == 13) menuIndex = 1;
    if (key == 15) {
      if (menuIndex == 0) {
        state = NOTEBOOK; bufferLen = 0; cursorPos = 0; contentOffset = 0;
      } else {
        state = HISTORY; fileIndex = 0;
      }
    }
  } else if (state == NOTEBOOK) {
    if (key >= 0 && key <= 9) {
      if (key == lastKey && (now - lastTapTime < MULTITAP_TIMEOUT)) {
        tapIndex = (tapIndex + 1) % strlen(multitapMap[key]);
      } else {
        tapIndex = 0;
      }
      insertChar(multitapMap[key][tapIndex]);
      lastKey = key;
      lastTapTime = now;
    } else if (key == 12 && cursorPos > 0) cursorPos--;
    else if (key == 13 && cursorPos < bufferLen) cursorPos++;
    else if (key == 14) deleteChar();
    else if (key == 15) { state = SAVE_NAME; filenameLen = 0; filenameInput[0] = 0; }
  } else if (state == SAVE_NAME) {
    if (key >= 0 && key <= 9 && filenameLen < MAX_FILENAME - 1) {
      filenameInput[filenameLen++] = '0' + key;
      filenameInput[filenameLen] = 0;
    } else if (key == 15 && fileCount < MAX_FILES) {
      strcpy(files[fileCount].name, filenameInput);
      strcpy(files[fileCount].content, buffer);
      files[fileCount].length = bufferLen;
      fileCount++;
      state = MENU;
    }
  } else if (state == HISTORY) {
    if (key == 12 && fileIndex > 0) fileIndex--;
    if (key == 13 && fileIndex < fileCount - 1) fileIndex++;
    if (key == 15 && fileCount > 0) {
      strcpy(buffer, files[fileIndex].content);
      bufferLen = files[fileIndex].length;
      cursorPos = 0;
      contentOffset = 0;
      state = READ_FILE;
    }
  } else if (state == READ_FILE) {
    if (key == 12 && cursorPos >= 16) cursorPos -= 16;
    if (key == 13 && cursorPos + 16 < bufferLen) cursorPos += 16;
    if (key == 15) state = MENU;
  }
  updateDisplay();
}
