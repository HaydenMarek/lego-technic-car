#include <Arduino.h>
#include <Bluepad32.h>

#include "Config.h"
#include "ControllerInput.h"
#include "DriveOutput.h"
#include "ServoSteering.h"
#include "VehicleState.h"

namespace {
ControllerInput controllerInput;
DriveOutput driveOutput;
ServoSteering servoSteering;
VehicleStateMachine vehicle;
uint32_t lastDiagnosticMs = 0;

const char* modeName(VehicleMode mode) {
  switch (mode) {
    case VehicleMode::Boot: return "BOOT";
    case VehicleMode::Disconnected: return "DISCONNECTED";
    case VehicleMode::Unarmed: return "UNARMED";
    case VehicleMode::Armed: return "ARMED";
  }
  return "UNKNOWN";
}

const char* reasonName(DisarmReason reason) {
  switch (reason) {
    case DisarmReason::None: return "NONE";
    case DisarmReason::Boot: return "BOOT";
    case DisarmReason::ControllerDisconnected: return "CONTROLLER_DISCONNECTED";
    case DisarmReason::Operator: return "OPERATOR";
    case DisarmReason::WatchdogTimeout: return "WATCHDOG_TIMEOUT";
    case DisarmReason::InvalidArmingInput: return "NEUTRAL_REQUIRED";
  }
  return "UNKNOWN";
}

void apply(const VehicleCommand& command) {
  if (command.mode == VehicleMode::Armed) {
    driveOutput.setThrottle(command.throttle);
    servoSteering.setPercent(command.steering);
  } else {
    // Every unsafe state coasts the bridge and commands the calibrated centre.
    driveOutput.coast();
    servoSteering.center();
  }
}

void reportState(const VehicleCommand& command) {
  if constexpr (Config::EnableSerialDiagnostics) {
    if (command.modeChanged) {
      Serial.printf("STATE,%s,REASON,%s\n", modeName(command.mode),
                    reasonName(command.reason));
    }
  }
}

void processCommand(const VehicleCommand& command) {
  apply(command);
  reportState(command);
}
}  // namespace

void setup() {
  Serial.begin(Config::UsbBaud);
  delay(50);
  if constexpr (Config::EnableSerialDiagnostics) {
    Serial.printf("BOOT,TECHNIC_RC_V2,BLUEPAD32,%s\n", BP32.firmwareVersion());
    Serial.printf("PROFILE,BTS7960,%d,SERVO,%d\n", Config::EnableBts7960Outputs,
                  Config::EnableServoOutput);
  }

  driveOutput.begin();
  servoSteering.begin();
  processCommand(vehicle.bootComplete());
  controllerInput.begin();
}

void loop() {
  ControllerFrame frame;
  const bool freshInput = controllerInput.update(frame);
  const ControllerEvent event = controllerInput.takeEvent();
  if (event == ControllerEvent::Connected) {
    if constexpr (Config::EnableSerialDiagnostics) {
      Serial.printf("CONTROLLER,CONNECTED,MODEL,%s,VID,0x%04x,PID,0x%04x\n",
                    controllerInput.controllerModel(), controllerInput.vendorId(),
                    controllerInput.productId());
    }
    processCommand(vehicle.controllerConnected());
  } else if (event == ControllerEvent::Disconnected) {
    if constexpr (Config::EnableSerialDiagnostics) {
      Serial.println("CONTROLLER,DISCONNECTED");
    }
    processCommand(vehicle.controllerDisconnected());
  }

  const uint32_t nowMs = millis();
  if (freshInput) {
    const VehicleCommand command = vehicle.accept(frame, nowMs);
    processCommand(command);
    if constexpr (Config::EnableSerialDiagnostics) {
      if (static_cast<uint32_t>(nowMs - lastDiagnosticMs) >= 100U) {
        Serial.printf("COMMAND,THROTTLE,%d,STEERING,%d\n", command.throttle,
                      command.steering);
        lastDiagnosticMs = nowMs;
      }
    }
  }
  const VehicleCommand watchdog = vehicle.checkWatchdog(nowMs);
  if (watchdog.modeChanged) {
    processCommand(watchdog);
  }
  delay(1);
}
