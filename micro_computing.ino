#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>

// === LCD 4-bit pins ===
#define LCD_RS PC1
#define LCD_RW PC0
#define LCD_E  PD2
#define LCD_D4 PC2
#define LCD_D5 PC3
#define LCD_D6 PC4
#define LCD_D7 PC5

// === Keypad pins ===
const uint8_t rowPins[4] = {PD7, PB0, PB1, PB2}; // 7,8,9,10
const uint8_t colPins[4] = {PD3, PD4, PD5, PD6}; // 3,4,5,6

#define MAX_CHARS 16
#define TAP_DELAY 800  // ms

// Multitap maps
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
const char* const maps[] PROGMEM = { map0,map1,map2,map3,map4,map5,map6,map7,map8,map9 };

char buffer[MAX_CHARS+1] = {0};
uint8_t bufLen = 0;
uint8_t cursorPos = 0;
int8_t lastKey = -1;
uint8_t tapIndex = 0;
unsigned long lastTapTime = 0;

// === LCD low-level ===
void lcdPulse() {
    PORTD |= (1<<LCD_E);
    _delay_us(1);
    PORTD &= ~(1<<LCD_E);
    _delay_us(80);
}

void lcdCmd(uint8_t cmd) {
    PORTC &= ~((1<<LCD_RS)|(1<<LCD_RW));
    PORTC = (PORTC & 0xC3) | ((cmd>>4)<<2);
    lcdPulse();
    PORTC = (PORTC & 0xC3) | ((cmd&0x0F)<<2);
    lcdPulse();
    _delay_ms(2);
}

void lcdData(uint8_t d) {
    PORTC = (PORTC & ~(1<<LCD_RW)) | (1<<LCD_RS);
    PORTC = (PORTC & 0xC3) | ((d>>4)<<2);
    lcdPulse();
    PORTC = (PORTC & 0xC3) | ((d&0x0F)<<2);
    lcdPulse();
    _delay_ms(2);
}

void lcdInit() {
    DDRC |= (1<<LCD_RS)|(1<<LCD_RW)|(1<<LCD_D4)|(1<<LCD_D5)|(1<<LCD_D6)|(1<<LCD_D7);
    DDRD |= (1<<LCD_E);
    _delay_ms(50);
    lcdCmd(0x33); lcdCmd(0x32);
    lcdCmd(0x28); lcdCmd(0x0C);
    lcdCmd(0x06); lcdCmd(0x01);
    _delay_ms(2);
}

// === Keypad ===
void keypadInit() {
    DDRD |= (1<<PD7);
    DDRB |= (1<<PB0)|(1<<PB1)|(1<<PB2);
    PORTD |= (1<<PD7);
    PORTB |= (1<<PB0)|(1<<PB1)|(1<<PB2);
    DDRD &= ~((1<<PD3)|(1<<PD4)|(1<<PD5)|(1<<PD6));
    PORTD |= (1<<PD3)|(1<<PD4)|(1<<PD5)|(1<<PD6);
}

int8_t getKey() {
    for(int8_t r=0;r<4;r++){
        PORTD |= (1<<PD7);
        PORTB |= (1<<PB0)|(1<<PB1)|(1<<PB2);
        if(r==0) PORTD &= ~(1<<PD7);
        if(r==1) PORTB &= ~(1<<PB0);
        if(r==2) PORTB &= ~(1<<PB1);
        if(r==3) PORTB &= ~(1<<PB2);
        _delay_us(5);
        for(int8_t c=0;c<4;c++){
            uint8_t bit = (c<1?PD3:c<2?PD4:c<3?PD5:PD6);
            if(!(PIND & (1<<bit))) {
                _delay_ms(50);
                while(!(PIND & (1<<bit)));
                return r*4 + c;
            }
        }
    }
    return -1;
}

void refreshDisplay() {
    lcdCmd(0x80);
    // first line blank
    for(uint8_t i=0;i<16;i++) lcdData(' ');
    lcdCmd(0xC0);
    for(uint8_t i=0;i<16;i++) {
        if(i==cursorPos && i<bufLen) lcdData('_'); // underline current char
        else if(i<bufLen) lcdData(buffer[i]);
        else lcdData(' ');
    }
}

int main(void) {
    lcdInit();
    keypadInit();
    refreshDisplay();
    while(1) {
        int8_t k = getKey();
        unsigned long now = (unsigned long)(TCNT1);
        if(k>=0 && k<10) {
            const char* ptr = (const char*)pgm_read_ptr(&maps[k]);
            uint8_t len = strlen_P(ptr);
            char ch;
            if(k==lastKey && (now-lastTapTime)<TAP_DELAY && cursorPos>0) {
                tapIndex = (tapIndex+1)%len;
                ch = pgm_read_byte(ptr+tapIndex);
                buffer[cursorPos-1]=ch;
            } else if(bufLen<MAX_CHARS) {
                tapIndex=0;
                ch = pgm_read_byte(ptr);
                // insert at cursorPos
                memmove(buffer+cursorPos+1, buffer+cursorPos, bufLen-cursorPos);
                buffer[cursorPos++] = ch;
                bufLen++;
                buffer[bufLen]='\0';
            }
            lastKey = k; lastTapTime = now;
        }
        else if(k==10) { // key 10 => move left
            if(cursorPos>0) cursorPos--;
        }
        else if(k==11) { // key 11 => move right
            if(cursorPos<bufLen) cursorPos++;
        }
        else if(k==12) { // key 12 => delete at cursor
            if(cursorPos>0) {
                memmove(buffer+cursorPos-1, buffer+cursorPos, bufLen-cursorPos);
                bufLen--; cursorPos--;
                buffer[bufLen]='\0';
            }
        }
        if(lastKey>=0 && (now-lastTapTime)>=TAP_DELAY) {
            lastKey=-1; tapIndex=0;
        }
        refreshDisplay();
    }
    return 0;
}
