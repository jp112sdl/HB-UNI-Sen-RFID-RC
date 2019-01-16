//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2018-10-10 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2019-01-16 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER
// dom.GetObject("BidCos-RF.JPRFID0001:1.SUBMIT").State("0x03,0xfa,0x14,0x3c");

#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <SPI.h>
#include <MFRC522.h>
#include <AskSinPP.h>
#include <LowPower.h>

#include <Register.h>
#include <Device.h>
#include <MultiChannelDevice.h>
#include <RFID.h>

#define LED1_PIN              4
#define LED2_PIN              5
#define RFID_READER_CS_PIN    6
#define RFID_READER_RESET_PIN 7
#define CONFIG_BUTTON_PIN     8
#define BUZZER_PIN            15 //A1


#define NUM_CHANNELS          8
// number of available peers per channel
#define PEERS_PER_CHANNEL     10

MFRC522 mfrc522(RFID_READER_CS_PIN, RFID_READER_RESET_PIN);

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
typedef StatusLed<BUZZER_PIN> BuzzerType;
typedef AskSin<LedType, BatterySensor, Radio<RadioSPI, 2> > BaseHal;

class Buzzer: public BuzzerType {
  private:
    bool enabled;
  public:
    Buzzer () : enabled(false) {}
    virtual ~Buzzer () {}

    void Enabled(bool en) {
    	enabled = en;
    }

	bool Enabled() {
		return enabled;
	}

	void Buzz(unsigned long ticks) {
	  if ( enabled == true )
          this->ledOn(ticks);
	}

	void BuzzOn() {
	  if ( enabled == true )
          this->ledOn();
	}

	void BuzzOff() {
      this->ledOff();
	}
};

class Hal: public BaseHal {
  public:
	Buzzer buzzer;
  public:
    void init(const HMID& id) {
      BaseHal::init(id);
      buzzer.init();
    }

    bool runready () {
      return sysclock.runready() || BaseHal::runready();
    }
} hal;

DEFREGISTER(RFIDReg0, MASTERID_REGS, DREG_BUZZER_ENABLED)
class RFIDList0 : public RegList0<RFIDReg0> {
  public:
    RFIDList0 (uint16_t addr) : RegList0<RFIDReg0>(addr) {}

    void defaults () {
      clear();
      buzzerEnabled(true);
    }
};

typedef RFIDChannel<Hal, PEERS_PER_CHANNEL, RFIDList0> RfidChannel;

class RFIDDev : public MultiChannelDevice<Hal, RfidChannel, NUM_CHANNELS, RFIDList0> {
  public:
    typedef MultiChannelDevice<Hal, RfidChannel, NUM_CHANNELS, RFIDList0> DevType;
    RFIDDev (const DeviceInfo& i, uint16_t addr) : DevType(i, addr) {}
    virtual ~RFIDDev () {}
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
    	getHal().buzzer.Enabled(this->getList0().buzzerEnabled());
    }

    void initPins() {
      pinMode(RFID_READER_CS_PIN, OUTPUT);
      pinMode(RFID_READER_RESET_PIN, OUTPUT);
      digitalWrite(RFID_READER_CS_PIN, LOW);
      digitalWrite(RFID_READER_RESET_PIN, LOW);
    }

    bool init(Hal& hal) {
      initPins();
      DevType::init(hal);
      DPRINT(F("Init RFID... "));
      mfrc522.PCD_Init();
      mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
      mfrc522.PCD_DumpVersionToSerial();
      return true;
    }
};

RFIDDev sdev(devinfo, 0x20);
ConfigButton<RFIDDev> cfgBtn(sdev);
RFIDScanner<RFIDDev, RfidChannel, mfrc522, LED2_PIN, LED1_PIN> scanner(sdev);

void setup () {
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
  bool firstinit = sdev.init(hal);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  if( firstinit == true ) {
    for (uint8_t i = 0; i < NUM_CHANNELS; i++)
      //beim Starten alle Chip IDs an die CCU senden
      sdev.rfidChannel(i).sendChipID();
  }
  sdev.initDone();
  sysclock.add(scanner);
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if ( worked == false && poll == false ) {
   hal.activity.savePower<Idle<>>(hal);
  }
}
