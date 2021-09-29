//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2021-08-11 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
// ci-test=yes board=328p aes=no

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

#define SIMPLE_CC1101_INIT
#define NO_RTC
#define HIDE_IGNORE_MSG
#define NO_CRC

#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <LowPower.h>
#include <AskSinPP.h>
#include <Register.h>
#include <Button.h>

#define I_WAKEUP A4
#define I_DATA   A3
#define O_AWAKE  A2
#define O_CLK    A1
#define O_RST    A5

#define NUM_CHANNELS       4

#include "MSP430State.h"

using namespace as;


#define LED_PIN           8
#define LED_PIN2          9
#define CONFIG_BUTTON_PIN A0

#define CC1101_PWR        5

#define PEERS_PER_CHANNEL 10
#define POWEROFF_DELAY_MILLIS 3500

const struct DeviceInfo PROGMEM devinfo = {
  {0x00, 0x5F, 0x01},     // Device ID
  "JPSCI30012",           // Device Serial
  {0x00, 0x5F},           // Device Model
  0x22,                   // Firmware Version
  as::DeviceType::Swi,    // Device Type
  {0x00, 0x00}            // Info Bytes
};

typedef AvrSPI<10, 11, 12, 13> SPIType;
typedef Radio<SPIType, 2, CC1101_PWR> RadioType;
typedef DualStatusLed<LED_PIN, LED_PIN2> LedType;
typedef AskSin<LedType, IrqInternalBatt, RadioType> Hal;

DEFREGISTER(Reg0, DREG_CYCLICINFOMSG, MASTERID_REGS, DREG_TRANSMITTRYMAX)
class SCList0 : public RegList0<Reg0> {
  public:
    SCList0(uint16_t addr) : RegList0<Reg0>(addr) {}
    void defaults () {
      clear();
      cycleInfoMsg(true);
      transmitDevTryMax(6);
    }
};

DEFREGISTER(Reg1, CREG_AES_ACTIVE, CREG_MSGFORPOS, CREG_EVENTDELAYTIME, CREG_TRANSMITTRYMAX)
class SCList1 : public RegList1<Reg1> {
  public:
    SCList1 (uint16_t addr) : RegList1<Reg1>(addr) {}
    void defaults () {
      clear();
      msgForPosA(1); // CLOSED
      msgForPosB(2); // OPEN
      //aesActive(false);
      eventDelaytime(0);
      transmitTryMax(6);
    }
};

typedef MSPStateChannel<Hal, SCList0, SCList1, DefList4, PEERS_PER_CHANNEL> ChannelType;
typedef StateDevice<Hal, ChannelType, NUM_CHANNELS, SCList0> SCType;

Hal hal;
SCType sdev(devinfo, 0x20);
ConfigButton<SCType> cfgBtn(sdev);

void setup () {
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
  sdev.init(hal);

  hal.battery.init();
  hal.battery.low(21);
  hal.battery.critical(19);

  while (hal.battery.current() == 0);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  sdev.initDone();

  contactISR(sdev, I_WAKEUP);
}

class PowerOffAlarm : public Alarm {
  private:
    bool    timerActive;
  public:
    PowerOffAlarm () : Alarm(0), timerActive(false) {}
    virtual ~PowerOffAlarm () {}

    void activateTimer(bool en) {
      if (en == true && timerActive == false) {
        sysclock.cancel(*this);
        set(millis2ticks(POWEROFF_DELAY_MILLIS));
        sysclock.add(*this);
      } else if (en == false) {
        sysclock.cancel(*this);
      }
      timerActive = en;
    }

    virtual void trigger(__attribute__((unused)) AlarmClock& clock) {
      powerOff();
    }

    void powerOff() {
      sdev.check();  //re-check before sleep
      hal.led.ledOff();
      hal.radio.setIdle();
      DPRINTLN("DEEPSLEEP");_delay_ms(200);
      LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    }

} pwrOffAlarm;

bool isAnyChannelActive() {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (sdev.channel(i+1).isActive()) {
      return true;
    }
  }
  return false;
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  bool act  = isAnyChannelActive();

  if (act == true && worked == false && poll == false) {
    hal.sleep();
  } else {
    pwrOffAlarm.activateTimer(act == false && hal.activity.stayAwake() == false && worked == false && poll == false );
  }
}
