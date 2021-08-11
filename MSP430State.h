//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2017-10-19 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2020-01-30 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef __MSP430CONTACTSTATE_H__
#define __MSP430CONTACTSTATE_H__

#include <MultiChannelDevice.h>

enum State { NoPos = 0, PosA, PosB, PosC };

#define STROBE_DLY  1500
#define READ_COUNT  8

namespace as {

template <class HALTYPE, class List0Type, class List1Type, class List4Type, int PEERCOUNT>
class MSPStateChannel : public Channel<HALTYPE, List1Type, EmptyList, List4Type, PEERCOUNT, List0Type> {

   class EventSender : public Alarm {
      public:
        MSPStateChannel& channel;
        uint8_t count, state;

        EventSender (MSPStateChannel& c) : Alarm(0), channel(c), count(0), state(0) {}
        virtual ~EventSender () {}
        virtual void trigger (__attribute__ ((unused)) AlarmClock& clock) {
          SensorEventMsg& msg = (SensorEventMsg&)channel.device().message();
          msg.init(channel.device().nextcount(), channel.number(), count++, state, channel.device().battery().low());
          channel.device().sendPeerEvent(msg, channel);
        }
    };
   EventSender sender;

  public:
    typedef Channel<HALTYPE, List1Type, EmptyList, List4Type, PEERCOUNT, List0Type> BaseChannel;

    MSPStateChannel () : BaseChannel(), sender(*this) {}
    virtual ~MSPStateChannel () {}

    void setup(Device<HALTYPE, List0Type>* dev, uint8_t number, uint16_t addr) {
      BaseChannel::setup(dev, number, addr);
    }

    void sendState (uint8_t posi) {
      uint8_t msg = 0;
      switch ( posi ) {
        case State::PosA:
          msg = this->getList1().msgForPosA();
          break;
        case State::PosB:
          msg = this->getList1().msgForPosB();
          break;
        default:
          break;
      }

      uint8_t newstate = sender.state;
      if ( msg == 1) newstate = 0;
      else if ( msg == 2) newstate = 200;

      if (newstate != sender.state) {
        uint8_t delay = this->getList1().eventDelaytime();
        sender.state = newstate;
        sysclock.cancel(sender);
        if ( delay == 0 ) {
          sender.trigger(sysclock);
        }
        else {
          sender.set(AskSinBase::byteTimeCvtSeconds(delay));
          sysclock.add(sender);
        }
      }
    }

    uint8_t status () const {
      return sender.state;
    }

    bool isActive() {
      return sender.active();
    }

    uint8_t flags () const {
      uint8_t flags =  0x00;
      flags |= this->device().battery().low() ? 0x80 : 0x00;
      return flags;
    }

};

template<class HalType, class ChannelType, int ChannelCount, class List0Type>
class StateDevice : public MultiChannelDevice<HalType, ChannelType, ChannelCount, List0Type> {

  class CheckAlarm : public Alarm {
    public:
    StateDevice& sd;
      CheckAlarm (StateDevice& _sd) : Alarm(0), sd(_sd) {}
      virtual ~CheckAlarm () {}
      virtual void trigger(__attribute__((unused)) AlarmClock& clock) {
        sd.check();
      }
  };

  private:
    bool mspRead() {
      digitalWrite(O_CLK, HIGH);
      _delay_us(STROBE_DLY);
      bool res = digitalRead(I_DATA);
      digitalWrite(O_CLK, LOW);
      _delay_us(STROBE_DLY);
      return res;
  }
  private:
    volatile bool canInterrupt;
    bool cycleEnabled;

  protected:
    CheckAlarm ca;
  public:
    typedef MultiChannelDevice<HalType, ChannelType, ChannelCount, List0Type> DevType;
    StateDevice(const DeviceInfo& info, uint16_t addr) : DevType(info, addr), canInterrupt(true), cycleEnabled(true), ca(*this) {}
    virtual ~StateDevice () {}


    void irq() {
      sysclock.cancel(ca);
      if (canInterrupt) {
        digitalWrite(O_AWAKE, HIGH);
        sysclock.add(ca);
      }
    }

    void resetMSP() {
      pinMode(O_RST, OUTPUT);
      digitalWrite(O_RST, LOW);
      _delay_ms(100);
      pinMode(O_RST, INPUT);
    }

    void initPins() {
      pinMode(I_WAKEUP, INPUT );
      pinMode(I_DATA  , INPUT );
      pinMode(O_RST,    INPUT ); //init as high impedance input first
      pinMode(O_CLK   , OUTPUT);
      pinMode(O_AWAKE , OUTPUT);
      resetMSP();
    }

    void check() {
      if (canInterrupt == true) {
      canInterrupt = false;
      uint8_t mspState = 0;

      for (uint8_t i = 0; i < READ_COUNT; i++) {
        while (digitalRead(I_WAKEUP) == HIGH);
        if (mspRead()) bitSet(mspState, i);
      }

      digitalWrite(O_AWAKE, LOW);

      delay(20);

      DPRINT("OUT: ");DHEXLN(mspState);

      this->channel(1).sendState(mspState & 0b00000001 ? State::PosB : State::PosA);
      this->channel(2).sendState(mspState & 0b00000010 ? State::PosB : State::PosA);
      this->channel(3).sendState(mspState & 0b00000100 ? State::PosB : State::PosA);

      if ( (mspState & 0b10000000) && cycleEnabled) {
        DPRINTLN("send cyclic");
        this->channel(1).changed(true);
      }

      canInterrupt = true;
      }
    }

    virtual void configChanged () {
      cycleEnabled = this->getList0().cycleInfoMsg();
    }
};


#define contactISR(dev,pin) class __##pin##ISRHandler { \
    public: \
      static void isr () { dev.irq(); } \
  }; \
    dev.initPins(); \
    enableInterrupt(pin,__##pin##ISRHandler::isr,RISING);

}

#endif
