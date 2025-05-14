#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>

#define LCD_RS PC1   // A1
#define LCD_RW PC0   // A0 sempre em LOW
#define LCD_E  PD2   // D2
#define LCD_D4 PC2   // A2
#define LCD_D5 PC3   // A3
#define LCD_D6 PC4   // A4
#define LCD_D7 PC5   // A5

// keypad
const uint8_t rowPins[4] = {7,8,9,10}; // D7-D10
const uint8_t colPins[4] = {3,4,5,6};  // D3-D6

#define MAX_CHARS   32    // 16x2
#define TAP_DELAY   800

const char alphaNumMap0[] PROGMEM = "ABCD";
const char alphaNumMap1[] PROGMEM = "EFGH";
const char alphaNumMap2[] PROGMEM = "IJKL";
const char alphaNumMap3[] PROGMEM = "MNOP";
const char alphaNumMap4[] PROGMEM = "QRST";
const char alphaNumMap5[] PROGMEM = "UVWX";
const char alphaNumMap6[] PROGMEM = "YZ01";
const char alphaNumMap7[] PROGMEM = "2345";
const char alphaNumMap8[] PROGMEM = "6789";
const char punctMap0[]    PROGMEM = ".,?!";
const char punctMap1[]    PROGMEM = "-_/:";
const char punctMap2[]    PROGMEM = ";'()";

const char* const maps[] PROGMEM = {
  alphaNumMap0,alphaNumMap1,alphaNumMap2,
  alphaNumMap3,alphaNumMap4,alphaNumMap5,
  alphaNumMap6,alphaNumMap7,alphaNumMap8,
  punctMap0,   punctMap1,   punctMap2
};

char   buffer[MAX_CHARS+1] = {0};
uint8_t bufLen   = 0;
int8_t lastKey   = -1;
uint8_t tapIndex = 0;

// envia pulso de Enable
static void lcdPulse(){
    PORTD |=  (1<<LCD_E);
    _delay_us(1);
    PORTD &= ~(1<<LCD_E);
    _delay_us(80);
}

// envia comando
static void lcdCmd(uint8_t cmd){
    PORTC &= ~((1<<LCD_RS)|(1<<LCD_RW));
    PORTC = (PORTC & 0xC3) | ((cmd>>4)<<2); lcdPulse();
    PORTC = (PORTC & 0xC3) | ((cmd&0x0F)<<2); lcdPulse();
    _delay_ms(2);
}

// envia dado
static void lcdData(uint8_t d){
    PORTC = (PORTC & ~(1<<LCD_RW)) | (1<<LCD_RS);
    PORTC = (PORTC & 0xC3) | ((d>>4)<<2); lcdPulse();
    PORTC = (PORTC & 0xC3) | ((d&0x0F)<<2); lcdPulse();
    _delay_ms(2);
}

// inicializa LCD
void lcdInit(){
    DDRC |= (1<<LCD_RS)|(1<<LCD_RW)|(1<<LCD_D4)|(1<<LCD_D5)|(1<<LCD_D6)|(1<<LCD_D7);
    DDRD |= (1<<LCD_E);
    PORTC &= ~(1<<LCD_RW);  // garante RW=LOW
    _delay_ms(50);
    lcdCmd(0x33); lcdCmd(0x32);
    lcdCmd(0x28); // 4-bit, 2 linhas
    lcdCmd(0x0C); // display on, cursor off
    lcdCmd(0x06); // auto-increment
    lcdCmd(0x01); // clear
    _delay_ms(2);
}

// configura keypad
void keypadInit(){
    DDRD |=  (1<<PD7);
    DDRB |=  (1<<PB0)|(1<<PB1)|(1<<PB2);
    PORTD |= (1<<PD7);
    PORTB |= (1<<PB0)|(1<<PB1)|(1<<PB2);
    DDRD &= ~((1<<PD3)|(1<<PD4)|(1<<PD5)|(1<<PD6));
    PORTD |= (1<<PD3)|(1<<PD4)|(1<<PD5)|(1<<PD6);
}

// retorna índice da tecla ou -1
int8_t getKey(){
    for(int8_t r=0; r<4; r++){
        PORTD |=  (1<<PD7);
        PORTB |=  (1<<PB0)|(1<<PB1)|(1<<PB2);
        if(r==0) PORTD &= ~(1<<PD7);
        if(r==1) PORTB &= ~(1<<PB0);
        if(r==2) PORTB &= ~(1<<PB1);
        if(r==3) PORTB &= ~(1<<PB2);
        _delay_us(5);
        for(int8_t c=0; c<4; c++){
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

// desenha buffer e cursor
void refreshDisplay(uint8_t blink){
    // 1ª linha
    lcdCmd(0x80);
    for(uint8_t i=0; i<16; i++){
        if(i < bufLen)        lcdData(buffer[i]);
        else if(i==bufLen && blink) lcdData('_');
        else                  lcdData(' ');
    }
    // 2ª linha
    lcdCmd(0xC0);
    for(uint8_t i=16; i<32; i++){
        if(i < bufLen)        lcdData(buffer[i]);
        else if(i==bufLen && blink) lcdData('_');
        else                  lcdData(' ');
    }
}

int main(void){
    lcdInit();
    keypadInit();

    uint8_t blinkState = 0;
    uint16_t blinkTimer = 0;

    while(1){
        // lê tecla
        int8_t k = getKey();
        if(k>=0){
            // alfanumérico e pontuação
            if(k<=11){
                const char* ptr = (const char*)pgm_read_ptr(&maps[k]);
                uint8_t len = strlen_P(ptr);
                char ch = pgm_read_byte(ptr + ((lastKey==k && blinkTimer<TAP_DELAY)? ((tapIndex+1)%len) : 0));
                if(lastKey==k && blinkTimer<TAP_DELAY){
                    tapIndex = (tapIndex+1)%len;
                    buffer[bufLen-1] = ch;
                } else if(bufLen<MAX_CHARS){
                    tapIndex = 0;
                    buffer[bufLen++] = ch;
                }
                lastKey = k;
                blinkTimer = 0;
            }
            // ←
            else if(k==12 && bufLen>0){
                bufLen--; lastKey=-1;
            }
            // →
            else if(k==13 && bufLen<MAX_CHARS){
                bufLen++; lastKey=-1;
            }
            // DEL
            else if(k==14 && bufLen>0){
                buffer[--bufLen]=0; lastKey=-1;
            }
            // SPACE
            else if(k==15 && bufLen<MAX_CHARS){
                buffer[bufLen++]=' '; lastKey=-1;
            }
        }

        // atualiza blinkTimer e pisca a ~250ms
        _delay_ms(10);
        blinkTimer += 10;
        if(blinkTimer >= 150){
            blinkTimer = 0;
            blinkState ^= 1;
        }

        refreshDisplay(blinkState);
    }
    return 0;
}
