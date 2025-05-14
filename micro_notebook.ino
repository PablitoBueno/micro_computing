#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>

#define LCD_RS PC1   // A1
#define LCD_RW PC0   // A0 (mantido em GND ou em LOW pelo código)
#define LCD_E  PD2   // D2
#define LCD_D4 PC2   // A2
#define LCD_D5 PC3   // A3
#define LCD_D6 PC4   // A4
#define LCD_D7 PC5   // A5

// pinos do keypad (Arduino)
const uint8_t rowPins[4] = { 7, 8, 9, 10 }; // D7, D8, D9, D10
const uint8_t colPins[4] = { 3, 4, 5, 6 };   // D3, D4, D5, D6

#define MAX_CHARS      16
#define TAP_DELAY      800
#define BLINK_INTERVAL 500

const char alphaNumMap0[] PROGMEM = "ABCD";
const char alphaNumMap1[] PROGMEM = "EFGH";
const char alphaNumMap2[] PROGMEM = "IJKL";
const char alphaNumMap3[] PROGMEM = "MNOP";
const char alphaNumMap4[] PROGMEM = "QRST";
const char alphaNumMap5[] PROGMEM = "UVWX";
const char alphaNumMap6[] PROGMEM = "YZ01";
const char alphaNumMap7[] PROGMEM = "2345";
const char alphaNumMap8[] PROGMEM = "6789";

const char punctMap0[] PROGMEM = ".,?!";
const char punctMap1[] PROGMEM = "-_/:";
const char punctMap2[] PROGMEM = ";'()";

const char* const maps[] PROGMEM = {
  alphaNumMap0, alphaNumMap1, alphaNumMap2,
  alphaNumMap3, alphaNumMap4, alphaNumMap5,
  alphaNumMap6, alphaNumMap7, alphaNumMap8,
  punctMap0,    punctMap1,    punctMap2
};

char buffer[MAX_CHARS+1] = {0};
uint8_t bufLen = 0, cursorPos = 0;
int8_t lastKey = -1;
uint8_t tapIndex = 0, blinkState = 0;
unsigned long lastTapTime = 0, lastBlinkTime = 0;

void lcdPulse(){
    PORTD |=  (1<<LCD_E);
    _delay_us(1);
    PORTD &= ~(1<<LCD_E);
    _delay_us(80);
}

void lcdCmd(uint8_t cmd){
    PORTC &= ~((1<<LCD_RS)|(1<<LCD_RW));
    PORTC = (PORTC & 0xC3) | ((cmd>>4)<<2);
    lcdPulse();
    PORTC = (PORTC & 0xC3) | ((cmd&0x0F)<<2);
    lcdPulse();
    _delay_ms(2);
}

void lcdData(uint8_t d){
    PORTC = (PORTC & ~(1<<LCD_RW)) | (1<<LCD_RS);
    PORTC = (PORTC & 0xC3) | ((d>>4)<<2);
    lcdPulse();
    PORTC = (PORTC & 0xC3) | ((d&0x0F)<<2);
    lcdPulse();
    _delay_ms(2);
}

void lcdInit(){
    DDRC |= (1<<LCD_RS)|(1<<LCD_RW)|(1<<LCD_D4)|(1<<LCD_D5)|(1<<LCD_D6)|(1<<LCD_D7);
    DDRD |= (1<<LCD_E);
    PORTC &= ~(1<<LCD_RW);
    _delay_ms(50);
    lcdCmd(0x33); lcdCmd(0x32);
    lcdCmd(0x28);
    lcdCmd(0x0C);
    lcdCmd(0x06);
    lcdCmd(0x01);
    _delay_ms(2);
}

void keypadInit(){
    DDRD |=  (1<<PD7);
    DDRB |=  (1<<PB0)|(1<<PB1)|(1<<PB2);
    PORTD |= (1<<PD7);
    PORTB |= (1<<PB0)|(1<<PB1)|(1<<PB2);
    DDRD &= ~((1<<PD3)|(1<<PD4)|(1<<PD5)|(1<<PD6));
    PORTD |= (1<<PD3)|(1<<PD4)|(1<<PD5)|(1<<PD6);
}

int8_t getKey(){
    for(int8_t r=0;r<4;r++){
        PORTD |=  (1<<PD7);
        PORTB |=  (1<<PB0)|(1<<PB1)|(1<<PB2);
        if(r==0) PORTD &= ~(1<<PD7);
        if(r==1) PORTB &= ~(1<<PB0);
        if(r==2) PORTB &= ~(1<<PB1);
        if(r==3) PORTB &= ~(1<<PB2);
        _delay_us(5);
        for(int8_t c=0;c<4;c++){
            uint8_t bit = (c<1?PD3:c<2?PD4:c<3?PD5:PD6);
            if(!(PIND & (1<<bit))){
                _delay_ms(50);
                while(!(PIND & (1<<bit)));
                return r*4 + c;
            }
        }
    }
    return -1;
}

void refreshDisplay(){
    unsigned long now = TCNT1;
    if(now - lastBlinkTime >= BLINK_INTERVAL){
        blinkState ^= 1;
        lastBlinkTime = now;
    }
    // 1ª linha: buffer + cursor
    lcdCmd(0x80);
    for(uint8_t i=0;i<16;i++){
        if(i < bufLen){
            lcdData(buffer[i]);
        } else if(i == cursorPos && blinkState){
            lcdData('_');
        } else {
            lcdData(' ');
        }
    }
    // 2ª linha: limpa
    lcdCmd(0xC0);
    for(uint8_t i=0;i<16;i++){
        lcdData(' ');
    }
}

int main(void){
    TCCR1B |= (1<<CS10);
    lcdInit();
    keypadInit();
    TCNT1 = 0;

    // pré-carrega "NOTEBOOK:" na primeira linha
    strcpy(buffer, "NOTEBOOK:");
    bufLen = strlen(buffer);
    cursorPos = bufLen;
    refreshDisplay();

    while(1){
        int8_t k = getKey();
        unsigned long now = TCNT1;
        if(k>=0){
            // ←
            if(k==12 && cursorPos>0){
                cursorPos--;
                lastKey = -1;
            }
            // →
            else if(k==13 && cursorPos<bufLen){
                cursorPos++;
                lastKey = -1;
            }
            // DEL
            else if(k==14 && cursorPos>0){
                memmove(buffer+cursorPos-1, buffer+cursorPos, bufLen-cursorPos);
                bufLen--; cursorPos--;
                buffer[bufLen] = '\0';
                lastKey = -1;
            }
            // SPACE
            else if(k==15 && bufLen<MAX_CHARS){
                memmove(buffer+cursorPos+1, buffer+cursorPos, bufLen-cursorPos);
                buffer[cursorPos++] = ' ';
                bufLen++;
                buffer[bufLen] = '\0';
                lastKey = -1;
            }
            // pontuação
            else if(k>=9 && k<=11){
                uint8_t idx = k - 9;
                const char* ptr = (const char*)pgm_read_ptr(&maps[9+idx]);
                uint8_t len = strlen_P(ptr);
                char ch;
                if(lastKey==k && now-lastTapTime<TAP_DELAY && cursorPos>0){
                    tapIndex = (tapIndex+1)%len;
                    ch = pgm_read_byte(ptr+tapIndex);
                    buffer[cursorPos-1] = ch;
                } else if(bufLen<MAX_CHARS){
                    tapIndex = 0;
                    ch = pgm_read_byte(ptr);
                    memmove(buffer+cursorPos+1, buffer+cursorPos, bufLen-cursorPos);
                    buffer[cursorPos++] = ch;
                    bufLen++;
                    buffer[bufLen] = '\0';
                }
                lastKey = k;
                lastTapTime = now;
            }
            // alfanumérico
            else if(k>=0 && k<=8){
                const char* ptr = (const char*)pgm_read_ptr(&maps[k]);
                uint8_t len = strlen_P(ptr);
                char ch;
                if(lastKey==k && now-lastTapTime<TAP_DELAY && cursorPos>0){
                    tapIndex = (tapIndex+1)%len;
                    ch = pgm_read_byte(ptr+tapIndex);
                    buffer[cursorPos-1] = ch;
                } else if(bufLen<MAX_CHARS){
                    tapIndex = 0;
                    ch = pgm_read_byte(ptr);
                    memmove(buffer+cursorPos+1, buffer+cursorPos, bufLen-cursorPos);
                    buffer[cursorPos++] = ch;
                    bufLen++;
                    buffer[bufLen] = '\0';
                }
                lastKey = k;
                lastTapTime = now;
            }
        }
        if(lastKey>=0 && now-lastTapTime>=TAP_DELAY){
            lastKey = -1;
            tapIndex = 0;
            if(cursorPos<bufLen) cursorPos++;
        }
        refreshDisplay();
    }
    return 0;
}
