//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2018-10-10 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2019-01-16 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

// Steuerung mittels SUBMIT-Commands: dom.GetObject("BidCos-RF.<Device Serial>:<Ch#>.SUBMIT").State("<command>");
// <command> Typen:
// Setzen einer  8 Byte Chip ID:      0x08,0x15,0xca,0xfe,0x00,0x00,0x00,0x00
// Löschen einer Chip ID:             0xcc
// Erzwingen der Chip ID Übertragung: 0xfe
// Invertieren der StandbyLed:        0xff,0x01 (invertiert) oder 0xff,0x00 (nicht invertiert)
// Buzzer EIN (dauerhaft)             0xb1
// Buzzer AUS                         0xb0
// Buzzer Beep-Intervall endlos       0xba,0x01,0x02      (Beep Tondauer = 100ms (0x01); Pausendauer = 200ms (0x02))
// Buzzer Beep-Intervall Anzahl       0xba,0x04,0x04,0x0a (Beep Tondauer = 400ms (0x04); Pausendauer = 400ms (0x04); Anzahl = 10 (0x0a))

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

// #define USE_I2C_READER //not recommended when using 328P - not enough memory, https://github.com/arozcan/MFRC522-I2C-Library

#define USE_WIEGAND  // use a WIEGAND protocol based reader; change CC1101 GDO0 Pin at #define CC1101_GDO0_PIN below
                     // https://github.com/monkeyboard/Wiegand-Protocol-Library-for-Arduino

#define EI_NOTEXTERNAL

#include <EnableInterrupt.h>
#include <SPI.h>
#ifdef USE_I2C_READER
#include <Wire.h>
#include <MFRC522_I2C.h>
#elif defined USE_WIEGAND
#include <Wiegand.h>
#else
#include <MFRC522.h>
#endif

#include <AskSinPP.h>
#include <LowPower.h>

#include <Register.h>
#include <Device.h>
#include <MultiChannelDevice.h>
#include <RFID.h>

#define CC1101_GDO0_PIN       2 //change when using WIEGAND; it needs hw interrupt pins 2 and 3
#define LED1_PIN              9
#define LED2_PIN              4
#define RFID_READER_CS_PIN    7
#define RFID_READER_I2C_ADDR  0x28
#define RFID_READER_RESET_PIN 6
#define CONFIG_BUTTON_PIN     8
#define STANDBY_LED_PIN       14 //A0
#define BUZZER_PIN            17 //A3

#define NUM_CHANNELS          8
// number of available peers per channel
#define PEERS_PER_CHANNEL     10
#define CYCLETIME seconds2ticks(60UL*60) // hourly

#define STANDBY_LED_BLINK_MS     100
#define STANDBY_LED_INTERVAL_S   5

#ifdef USE_I2C_READER
MFRC522 readerDevice(RFID_READER_I2C_ADDR, RFID_READER_RESET_PIN);
#elif !defined USE_WIEGAND
MFRC522 readerDevice(RFID_READER_CS_PIN, RFID_READER_RESET_PIN);
#endif

#ifdef USE_WIEGAND
WIEGAND readerDevice;
#endif

// all library classes are placed in the namespace 'as'
using namespace as;

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
  {0xf3, 0x3c, 0x00},     // Device ID
  "JPRFID0001",           // Device Serial
  {0xf3, 0x3c},           // Device Model
  0x10,                   // Firmware Version
  as::DeviceType::Remote, // Device Type
  {0x00, 0x00}            // Info Bytes
};

/**
   Configure the used hardware
*/
typedef LibSPI<10> RadioSPI;
typedef DualStatusLed<LED2_PIN, LED1_PIN> LedType;
typedef StatusLed<STANDBY_LED_PIN> StandbyLedType;
typedef Buzzer<BUZZER_PIN> BuzzerType;
typedef AskSin<LedType, BatterySensor, Radio<RadioSPI, CC1101_GDO0_PIN>, BuzzerType > BaseHal;


class Hal: public BaseHal {
  public:
    StandbyLedType standbyLed;

    void init(const HMID& id) {
      BaseHal::init(id);
    }

    void standbyLedInvert(bool inv) {
      standbyLed.invert(inv);
    }

    bool runready () {
      return sysclock.runready() || BaseHal::runready();
    }
} hal;

DEFREGISTER(RFIDReg0, MASTERID_REGS, DREG_TRANSMITTRYMAX, DREG_CYCLICINFOMSG, DREG_BUZZER_ENABLED)
class RFIDList0: public RegList0<RFIDReg0> {
public:
  RFIDList0(uint16_t addr) :
    RegList0<RFIDReg0>(addr) {
  }

    void defaults () {
      clear();
      buzzerEnabled(true);
      transmitDevTryMax(2);
      cycleInfoMsg(true);
    }
};

typedef RFIDChannel<Hal, PEERS_PER_CHANNEL, RFIDList0> RfidChannel;

class RFIDDev : public MultiChannelDevice<Hal, RfidChannel, NUM_CHANNELS, RFIDList0> {
  public:
    class StandbyLedAlarm: public Alarm {
      RFIDDev& dev;
  public:
      StandbyLedAlarm(RFIDDev& d) :
          Alarm(seconds2ticks(STANDBY_LED_INTERVAL_S)), dev(d) {
      }
      virtual ~StandbyLedAlarm() {
      }
      void trigger(__attribute__ ((unused)) AlarmClock& clock) {
        tick = (seconds2ticks(5));
        dev.getHal().standbyLed.ledOn(millis2ticks(STANDBY_LED_BLINK_MS));
        clock.add(*this);
      }
    } standbyLedAlarm;
    class CycleInfoAlarm: public Alarm {
      RFIDDev& dev;
    public:
      CycleInfoAlarm(RFIDDev& d) :
          Alarm(CYCLETIME), dev(d) {
      }
      virtual ~CycleInfoAlarm() {
      }

      void trigger(AlarmClock& clock) {
        set(CYCLETIME);
        clock.add(*this);
        dev.rfidChannel(1).changed(true);
      }
    } cycleAlarm;
public:
  typedef MultiChannelDevice<Hal, RfidChannel, NUM_CHANNELS, RFIDList0> DevType;
  RFIDDev(const DeviceInfo& i, uint16_t addr) :
      DevType(i, addr), standbyLedAlarm(*this), cycleAlarm(*this) {
  }
  virtual ~RFIDDev() {
  }

    // return rfid channel from 0 - n-1
    RfidChannel& rfidChannel (uint8_t num) {
      return channel(num + 1);
    }
    // return number of ibutton channels
    uint8_t rfidCount () const {
      return channels();
    }

    void configChanged() {
      DPRINTLN("Config Changed List0");
      buzzer().enabled(this->getList0().buzzerEnabled());
      if (this->getList0().cycleInfoMsg() == true) {
        DPRINTLN("Activate Cycle Msg");
        sysclock.cancel(cycleAlarm);
        cycleAlarm.set(CYCLETIME);
        sysclock.add(cycleAlarm);
        rfidChannel(1).changed(true);
      } else {
        DPRINTLN("Deactivate Cycle Msg");
        sysclock.cancel(cycleAlarm);
      }
    }

    void initPins() {
#ifndef USE_I2C_READER
      pinMode(RFID_READER_CS_PIN, OUTPUT);
      pinMode(RFID_READER_RESET_PIN, OUTPUT);
      digitalWrite(RFID_READER_CS_PIN, LOW);
      digitalWrite(RFID_READER_RESET_PIN, LOW);
#endif
    }

    bool init(Hal& hal) {
      initPins();
      DevType::init(hal);
#ifdef USE_WIEGAND
      DPRINTLN(F("Init WIEGAND Interface... "));
      readerDevice.begin();
#else
      DPRINT(F("Init MFRC522 Reader... "));
      readerDevice.PCD_Init();
      byte readReg = readerDevice.PCD_ReadRegister(readerDevice.VersionReg);
      readerDevice.PCD_SetAntennaGain(readerDevice.RxGain_max);
      DPRINT("Firmware Version: 0x");DHEXLN(readReg);
#endif
      hal.standbyLed.init();
      sysclock.add(standbyLedAlarm);
      return true;
    }
};

RFIDDev sdev(devinfo, 0x20);
ConfigButton<RFIDDev> cfgBtn(sdev);
RFIDScanner<RFIDDev, RfidChannel, readerDevice, LED2_PIN, LED1_PIN> scanner(sdev);

void setup () {
#ifdef USE_I2C_READER
  Wire.begin();
#endif
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
  bool firstinit = sdev.init(hal);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  if ( firstinit == true ) {
    for (uint8_t i = 0; i < NUM_CHANNELS; i++)
      //beim Starten alle Chip IDs an die CCU senden
      sdev.rfidChannel(i).sendChipID();
  }
  sdev.initDone();
  sysclock.add(scanner);
  sdev.buzzer().on(millis2ticks(100),millis2ticks(100),2);
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if ( worked == false && poll == false ) {
    hal.activity.savePower<Idle<>>(hal);
  }
}
