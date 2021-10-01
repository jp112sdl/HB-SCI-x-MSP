// 2021-08-11 jp112sdl Creative Commons - CC-BY-NC-SA http://creativecommons.org/licenses/by-nc-sa/4.0/de/

#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>

#define OUT_AVR_WAKE_PIN     BIT0  //output, wake up the AVR
#define OUT_AVR_DATA         BIT1  //output, send data to the AVR
#define IN_AVR_IS_AWAKE_PIN  BIT2  //input, to check, if the AVR is not sleeping
#define IN_AVR_CLK_PIN       BIT3  //input, the clock for bitwise data sending on OUT_AVR_DATA pin
#define IN_SW1_PIN           BIT4  //input, contact to monitor
#define IN_SW2_PIN           BIT5  //input, contact to monitor
#define IN_SW3_PIN           BIT6  //input, contact to monitor
#define IN_SW4_PIN           BIT7  //input, contact to monitor

#define CYCLE_TIME           86400 * 4UL // every 24 hours (seconds * 4 -> 250ms tick interval)

uint32_t cycleCount      = CYCLE_TIME;
uint8_t  contactState    = 0;
uint8_t  lastContactState= 0xAA;

void sleep_1_9ms(uint8_t cnt) { uint8_t i; for (i = cnt; i > 0; i--) { WDTCTL = WDT_ADLY_1_9; __bis_SR_register(LPM3_bits + GIE); } }
void sleep_16ms (uint8_t cnt) { uint8_t i; for (i = cnt; i > 0; i--) { WDTCTL = WDT_ADLY_16;  __bis_SR_register(LPM3_bits + GIE); } }
void sleep_250ms(uint8_t cnt) { uint8_t i; for (i = cnt; i > 0; i--) { WDTCTL = WDT_ADLY_250; __bis_SR_register(LPM3_bits + GIE); } }

bool isAVRAwake()     { return ( P1IN & IN_AVR_IS_AWAKE_PIN ); }
bool AVRClockStrobe() { return ( P1IN & IN_AVR_CLK_PIN      ); }

void configurePins() {
    P1DIR &= ~IN_AVR_IS_AWAKE_PIN;
    P1DIR &= ~IN_AVR_CLK_PIN;
    P1DIR &= ~IN_SW1_PIN;
    P1DIR &= ~IN_SW2_PIN;
    P1DIR &= ~IN_SW3_PIN;
    P1DIR &= ~IN_SW4_PIN;
    P1REN |= IN_SW1_PIN | IN_SW2_PIN | IN_SW3_PIN | IN_SW4_PIN;
    P1OUT |= IN_SW1_PIN | IN_SW2_PIN | IN_SW3_PIN | IN_SW4_PIN;

    P1IE  |=  IN_AVR_IS_AWAKE_PIN;
    P1IES &= ~IN_AVR_IS_AWAKE_PIN;
    P1IFG &= ~IN_AVR_IS_AWAKE_PIN;
}

void idlePins() {
    P1DIR = 0xFF;    /*P1REN = 0;*/    P1OUT = 0;
    P2DIR = 0xFF;    /*P2REN = 0;*/    P2OUT = 0;
#ifdef __MSP430_HAS_PORT3_R__
    P3DIR = 0xFF;    /*P3REN = 0;*/    P3OUT = 0;
#endif
}

uint8_t readPins() {
  sleep_1_9ms(2); // wait ~ 2ms
  uint8_t result = 0;
  if (P1IN & IN_SW1_PIN) result |= 0b00000001;
  if (P1IN & IN_SW2_PIN) result |= 0b00000010;
  if (P1IN & IN_SW3_PIN) result |= 0b00000100;
  if (P1IN & IN_SW4_PIN) result |= 0b00001000;
  return result;
}

void wakeupAVR() {
    P1OUT  |=  OUT_AVR_WAKE_PIN;
    sleep_16ms(1);
    P1OUT  &= ~OUT_AVR_WAKE_PIN;
}

int main(void) {
  BCSCTL1 |= DIVA_0;        // ACLK/1
  //BCSCTL3 |= XCAP_2;        // 9pF load capacitor
  WDTCTL   = WDT_ADLY_250;  // WDT 250ms interval timer
  IE1     |= WDTIE;         // Enable WDT interrupt

  idlePins();

  sleep_250ms(8);           //wait 2 seconds after first init or reset

  while(1) {
    cycleCount--;
    configurePins();
    contactState  = 0;
    contactState |= readPins();

    if ((contactState != lastContactState) || (cycleCount == 0)) {
      lastContactState = contactState;
      if (cycleCount == 0) contactState |= 0b10000000;

      cycleCount          = CYCLE_TIME;
      bool lastStrobe     = false;
      uint8_t  clk_count  = 8;
      uint16_t exit_count = 5;  //prevent endless loop if AVR does not respond

      while (!isAVRAwake() && exit_count > 0) {
        sleep_1_9ms(2);
        wakeupAVR();
        exit_count--;
      }

      exit_count = 1000;
      while (isAVRAwake() && exit_count > 0 && clk_count > 0) {
        exit_count--;
        bool strobe = AVRClockStrobe();
        if (strobe != lastStrobe && strobe == true) { clk_count--; } //on rising edge
        lastStrobe = strobe;
             if (clk_count == 7) {  if (contactState & 0b00000001)  { P1OUT  |=  OUT_AVR_DATA; } else { P1OUT  &= ~OUT_AVR_DATA; } }
        else if (clk_count == 6) {  if (contactState & 0b00000010)  { P1OUT  |=  OUT_AVR_DATA; } else { P1OUT  &= ~OUT_AVR_DATA; } }
        else if (clk_count == 5) {  if (contactState & 0b00000100)  { P1OUT  |=  OUT_AVR_DATA; } else { P1OUT  &= ~OUT_AVR_DATA; } }
        else if (clk_count == 4) {  if (contactState & 0b00001000)  { P1OUT  |=  OUT_AVR_DATA; } else { P1OUT  &= ~OUT_AVR_DATA; } }
        else if (clk_count == 3) {  if (contactState & 0b00010000)  { P1OUT  |=  OUT_AVR_DATA; } else { P1OUT  &= ~OUT_AVR_DATA; } }
        else if (clk_count == 2) {  if (contactState & 0b00100000)  { P1OUT  |=  OUT_AVR_DATA; } else { P1OUT  &= ~OUT_AVR_DATA; } }
        else if (clk_count == 1) {  if (contactState & 0b01000000)  { P1OUT  |=  OUT_AVR_DATA; } else { P1OUT  &= ~OUT_AVR_DATA; } }
        else if (clk_count == 0) {  if (contactState & 0b10000000)  { P1OUT  |=  OUT_AVR_DATA; } else { P1OUT  &= ~OUT_AVR_DATA; } }
        else                                                                                          { P1OUT  &= ~OUT_AVR_DATA; }
      }

    } else {
      idlePins();
      sleep_250ms(2);
    }
  }

}

#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer (void) { __bic_SR_register_on_exit(LPM3_bits + GIE); }

#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void){
  P1IFG &= ~IN_AVR_IS_AWAKE_PIN;
  lastContactState = 0xAA;
}
