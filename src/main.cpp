#include <Arduino.h>
#include <SoftwareSerial.h>

#include "Config.h"
#include "CurrentMonitor.h"
#include "CurrentProtection.h"
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
        currentMonitor_(Serial),
        currentProtection_(motorDriver_),
        vehicle_(motorDriver_),
        watchdog_(Config::FailsafeTimeoutMs) {}

  void begin() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Serial.begin(Config::UsbBaud);
    hubSerial_.begin(Config::HubBaud);

    const uint32_t now = millis();
    motorDriver_.begin(now);
    currentMonitor_.begin(now);
    vehicle_.stop();
    watchdog_.begin(now);

    Serial.println(F("Vehicle controller ready"));
    Serial.println(Config::EnableBts7960Outputs
                       ? F("Motor output: BTS7960")
                       : F("Motor output: serial bench mode"));
    Serial.println(Config::EnableCurrentProtection
                       ? F("Current protection: ON")
                       : F("Current protection: OFF"));
    Serial.println(F("Commands: PING | MODE | STOP | D,<throttle>"));
    hubProtocol_.sendReady();
  }

  void update() {
    // Use one timestamp for command refresh, timeout evaluation, and ramping.
    // Sampling again inside process() could produce a refresh time newer than
    // this value and make unsigned timeout arithmetic look like a rollover.
    const uint32_t now = millis();

    process(hubProtocol_, now);
    if (Config::EnableMonitorCommands) {
      process(monitorProtocol_, now);
    }

    if (watchdog_.update(now)) {
      vehicle_.stop();
      Serial.println(F("FAILSAFE: vehicle stopped"));
    }

    motorDriver_.update(now);
    CurrentSenseSample currentSample;
    if (currentMonitor_.update(now, currentSample) &&
        currentProtection_.evaluate(currentSample)) {
      // CurrentProtection has already coasted the bridge. Print only after the
      // safety action so USB serial cannot delay cutoff.
      vehicle_.stop();
      currentProtection_.printTrip(Serial);
    }

    digitalWrite(LED_BUILTIN, watchdog_.isTimedOut() ? LOW : HIGH);
  }

 private:
  void process(Protocol& protocol, uint32_t now) {
    DriverCommand command;
    while (protocol.poll(command)) {
      switch (command.type) {
        case CommandType::Ping:
          protocol.sendPong();
          break;

        case CommandType::Mode:
          protocol.sendMode();
          break;

        case CommandType::Stop:
          vehicle_.stop();
          if (currentProtection_.clearFault()) {
            Serial.println(F("CURRENT_LIMIT,CLEARED"));
          }
          watchdog_.refresh(now);
          protocol.sendStopAcknowledgement();
          break;

        case CommandType::Drive:
          if (currentProtection_.allowsDrive()) {
            vehicle_.setThrottle(command.throttle);
            // The drive ACK is suppressed by default. It travels back over the
            // same half-duplex SoftwareSerial line and blocks receive for ~9 ms
            // at 9600 baud, which prevents reliable 20 ms control frames.
            if constexpr (Config::AcknowledgeDriveCommands) {
              protocol.sendDriveAcknowledgement(command);
            }
          }
          watchdog_.refresh(now);
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
  CurrentMonitor currentMonitor_;
  CurrentProtection currentProtection_;
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
