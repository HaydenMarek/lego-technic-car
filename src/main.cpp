#include <Arduino.h>
#include <SoftwareSerial.h>

#include "Config.h"
#include "MotorDriver.h"
#include "Protocol.h"
#include "Vehicle.h"
#include "Watchdog.h"

namespace {

class Application final {
 public:
  Application()
      : hubSerial_(Config::HubRxPin, Config::HubTxPin),
        hubProtocol_(hubSerial_),
        monitorProtocol_(Serial),
        motorDriver_(Serial),
        vehicle_(motorDriver_),
        watchdog_(Config::FailsafeTimeoutMs) {}

  void begin() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.begin(Config::UsbBaud);
    hubSerial_.begin(Config::HubBaud);

    motorDriver_.begin();
    vehicle_.stop();
    watchdog_.begin(millis());

    Serial.println(F("Vehicle controller ready"));
    Serial.println(F("Commands: PING | STOP | D,<throttle>,<steering>"));
    hubProtocol_.sendReady();
  }

  void update() {
    process(hubProtocol_);
    if (Config::EnableMonitorCommands) {
      process(monitorProtocol_);
    }

    if (watchdog_.update(millis())) {
      vehicle_.stop();
      Serial.println(F("FAILSAFE: vehicle stopped"));
    }

    digitalWrite(LED_BUILTIN, watchdog_.isTimedOut() ? LOW : HIGH);
  }

 private:
  void process(Protocol& protocol) {
    DriverCommand command;
    while (protocol.poll(command)) {
      switch (command.type) {
        case CommandType::Ping:
          watchdog_.refresh(millis());
          protocol.sendPong();
          break;

        case CommandType::Stop:
          vehicle_.stop();
          watchdog_.refresh(millis());
          protocol.sendStopAcknowledgement();
          break;

        case CommandType::Drive:
          vehicle_.setIntent(command.throttle, command.steering);
          watchdog_.refresh(millis());
          protocol.sendDriveAcknowledgement(command);
          break;

        case CommandType::Invalid:
          protocol.sendError();
          break;
      }
    }
  }

  SoftwareSerial hubSerial_;
  Protocol hubProtocol_;
  Protocol monitorProtocol_;
  MotorDriver motorDriver_;
  Vehicle vehicle_;
  Watchdog watchdog_;
};

Application& application() {
  static Application instance;
  return instance;
}

}  // namespace

void setup() { application().begin(); }

void loop() { application().update(); }

