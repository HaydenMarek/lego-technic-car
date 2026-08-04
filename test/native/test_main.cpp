#include <assert.h>
#include <stdint.h>

#include <string>

#include "CurrentMonitor.h"
#include "CurrentProtection.h"
#include "MotorDriver.h"
#include "Protocol.h"
#include "Vehicle.h"
#include "Watchdog.h"

#ifndef TECHNIC_RC_EXPECT_MONITOR_COMMANDS
#error "Native tests require TECHNIC_RC_EXPECT_MONITOR_COMMANDS"
#endif

#ifndef TECHNIC_RC_EXPECT_CURRENT_PROTECTION
#error "Native tests require TECHNIC_RC_EXPECT_CURRENT_PROTECTION"
#endif

namespace {

// Mirrors Vehicle::shapeThrottle so the drive tests stay correct for any
// configured Config::ThrottleCurveExponent.
int16_t curveShape(int16_t value) {
  if (value == 0) {
    return 0;
  }
  const int16_t mag = value < 0 ? static_cast<int16_t>(-value) : value;
  int32_t shaped = mag;
  for (uint8_t i = 1; i < Config::ThrottleCurveExponent; ++i) {
    shaped = shaped * mag / 100;
  }
  const int16_t result = static_cast<int16_t>(shaped);
  return value < 0 ? static_cast<int16_t>(-result) : result;
}

class FakeStream final : public Stream {
 public:
  void receive(const char* value) { input_ += value; }

  int available() override {
    return static_cast<int>(input_.size() - readPosition_);
  }

  int read() override {
    if (readPosition_ == input_.size()) {
      return -1;
    }
    return input_[readPosition_++];
  }

  size_t write(uint8_t value) override {
    output_ += static_cast<char>(value);
    return 1;
  }

  const std::string& output() const { return output_; }

 private:
  std::string input_;
  std::string output_;
  size_t readPosition_ = 0;
};

void protocolAcceptsIntentCommandsAndRejectsMalformedInput() {
  FakeStream stream;
  Protocol protocol(stream);
  DriverCommand command;

  stream.receive("PING\r\nMODE\nD,-100\nD,101\nD,1,2\n");

  assert(protocol.poll(command));
  assert(command.type == CommandType::Ping);

  assert(protocol.poll(command));
  assert(command.type == CommandType::Mode);

  assert(protocol.poll(command));
  assert(command.type == CommandType::Drive);
  assert(command.throttle == -100);

  assert(protocol.poll(command));
  assert(command.type == CommandType::Invalid);

  assert(protocol.poll(command));
  assert(command.type == CommandType::Invalid);
  assert(!protocol.poll(command));
}

void protocolRecoversAfterOverflow() {
  FakeStream stream;
  Protocol protocol(stream);
  DriverCommand command;

  stream.receive(
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"
      "STOP\n");

  assert(protocol.poll(command));
  assert(command.type == CommandType::Invalid);
  assert(protocol.poll(command));
  assert(command.type == CommandType::Stop);
}

void protocolWritesStableReplies() {
  FakeStream stream;
  Protocol protocol(stream);
  const DriverCommand drive{CommandType::Drive, 50};

  protocol.sendReady();
  protocol.sendPong();
  protocol.sendMode();
  protocol.sendStopAcknowledgement();
  protocol.sendDriveAcknowledgement(drive);
  protocol.sendError();

  std::string expected = "READY\nPONG\n";
  if constexpr (Config::EnableBts7960Outputs) {
    expected += "MODE,BTS7960\n";
  } else {
    expected += "MODE,BENCH\n";
  }
  expected += "ACK,STOP\nACK,D,50\nERR\n";
  assert(stream.output() == expected);
}

void configuredBuildProfileMatchesExpectedUsbMonitorAccess() {
  const bool expected = TECHNIC_RC_EXPECT_MONITOR_COMMANDS != 0;
  assert(Config::EnableMonitorCommands == expected);

  const bool expectedProtection =
      TECHNIC_RC_EXPECT_CURRENT_PROTECTION != 0;
  assert(Config::EnableCurrentProtection == expectedProtection);
}

void currentMonitorReportsWindowPeaksOnlyForBts7960Build() {
  FakeStream diagnostics;
  CurrentMonitor monitor(diagnostics);
  monitor.begin(UINT32_MAX - 50U);
  CurrentSenseSample sample;

  analogValues[A0] = 100;
  analogValues[A1] = 200;
  assert(monitor.update(UINT32_MAX - 45U, sample) ==
         Config::EnableBts7960Outputs);

  analogValues[A0] = 300;
  analogValues[A1] = 150;
  assert(monitor.update(UINT32_MAX - 40U, sample) ==
         Config::EnableBts7960Outputs);

  if constexpr (Config::EnableBts7960Outputs) {
    assert(sample.leftRaw == 300);
    assert(sample.rightRaw == 150);
    assert(monitor.leftPeakRaw() == 300);
    assert(monitor.rightPeakRaw() == 200);

    // The reporting interval crosses millis() rollover safely.
    assert(monitor.update(50U, sample));
    assert(diagnostics.output() ==
           "CURRENT,L_IS_RAW,300,R_IS_RAW,200\n");
    assert(monitor.leftPeakRaw() == 0);
    assert(monitor.rightPeakRaw() == 0);
  } else {
    assert(diagnostics.output().empty());
    assert(monitor.leftPeakRaw() == 0);
    assert(monitor.rightPeakRaw() == 0);
  }
}

void currentProtectionConfigurationMatchesWheelspinCalibration() {
  if constexpr (!Config::EnableCurrentProtection) {
    return;
  }

  assert(Config::CurrentLimitLeftRaw == 112);
  assert(Config::CurrentLimitRightRaw == 55);
  assert(Config::CurrentLimitTripSamples == 3);
  assert(Config::CurrentLimitFoldbackStep == 20);
  assert(Config::CurrentLimitMinimumPower == 20);
  assert(Config::CurrentLimitRecoveryStep == 5);
  assert(Config::CurrentLimitRecoverySamples == 20);
  assert(Config::CurrentLimitEmergencyTripSamples == 10);
  assert(Config::CurrentSenseSampleIntervalMs == 5);
}

void currentProtectionFoldsBackAfterPersistenceAndRecoversSlowly() {
  if constexpr (!Config::EnableCurrentProtection) {
    return;
  }

  FakeStream diagnostics;
  MotorDriver motorDriver(diagnostics);
  motorDriver.begin(0);
  motorDriver.setTarget(100);
  motorDriver.update(200);
  assert(motorDriver.applied() == 100);

  CurrentProtection protection(motorDriver);

  // A sample immediately below both inclusive channel limits must reset
  // persistence.
  const CurrentSenseSample belowLeft{
      static_cast<uint16_t>(Config::CurrentLimitLeftRaw - 1U),
      static_cast<uint16_t>(Config::CurrentLimitRightRaw - 1U),
  };
  const CurrentSenseSample atLeft{
      Config::CurrentLimitLeftRaw,
      static_cast<uint16_t>(Config::CurrentLimitRightRaw - 1U),
  };

  for (uint8_t i = 1; i < Config::CurrentLimitTripSamples; ++i) {
    assert(protection.evaluate(atLeft) == CurrentProtectionEvent::None);
    assert(protection.consecutiveOverLimitSamples() == i);
  }
  assert(protection.evaluate(belowLeft) == CurrentProtectionEvent::None);
  assert(protection.consecutiveOverLimitSamples() == 0);
  assert(motorDriver.applied() == 100);

  for (uint8_t i = 1; i < Config::CurrentLimitTripSamples; ++i) {
    assert(protection.evaluate(atLeft) == CurrentProtectionEvent::None);
  }
  assert(protection.evaluate(atLeft) ==
         CurrentProtectionEvent::Foldback);
  assert(!protection.faultLatched());
  assert(protection.allowsDrive());
  assert(protection.foldbackFrom() == 100);
  assert(protection.foldbackTo() == 80);
  assert(protection.powerLimit() == 80);
  assert(motorDriver.target() == 80);
  assert(motorDriver.applied() == 80);

  // Repeated full-throttle commands remain clamped by the live power limit.
  motorDriver.setTarget(100);
  assert(motorDriver.target() == 80);

  protection.printFoldback(diagnostics);
  const std::string expectedFoldback =
      "CURRENT_LIMIT,FOLDBACK,FROM,100,TO,80,OUTPUT,100,L_IS_RAW," +
      std::to_string(Config::CurrentLimitLeftRaw) +
      ",R_IS_RAW," +
      std::to_string(Config::CurrentLimitRightRaw - 1U) + "\n";
  assert(diagnostics.output().find(expectedFoldback) != std::string::npos);

  // Twenty safe samples restore only five percentage points, preventing an
  // immediate jump back to the current that caused foldback.
  for (uint8_t i = 1; i < Config::CurrentLimitRecoverySamples; ++i) {
    assert(protection.evaluate(belowLeft) ==
           CurrentProtectionEvent::None);
    assert(protection.powerLimit() == 80);
  }
  assert(protection.evaluate(belowLeft) ==
         CurrentProtectionEvent::None);
  assert(protection.powerLimit() == 85);

  // STOP clears either a foldback restriction or a latched emergency fault.
  assert(protection.clearFault());
  assert(!protection.faultLatched());
  assert(protection.allowsDrive());
  assert(protection.powerLimit() == 100);
  assert(!protection.clearFault());
}

void currentProtectionTripsPersistentOverloadAtMinimumPower() {
  if constexpr (!Config::EnableCurrentProtection) {
    return;
  }

  FakeStream diagnostics;
  MotorDriver motorDriver(diagnostics);
  motorDriver.begin(0);
  motorDriver.setTarget(100);
  motorDriver.update(200);
  assert(motorDriver.applied() == 100);

  CurrentProtection protection(motorDriver);
  const CurrentSenseSample atRight{
      static_cast<uint16_t>(Config::CurrentLimitLeftRaw - 1U),
      Config::CurrentLimitRightRaw,
  };

  uint8_t expectedLimit = 100;
  while (expectedLimit > Config::CurrentLimitMinimumPower) {
    for (uint8_t i = 1; i < Config::CurrentLimitTripSamples; ++i) {
      assert(protection.evaluate(atRight) ==
             CurrentProtectionEvent::None);
    }
    assert(protection.evaluate(atRight) ==
           CurrentProtectionEvent::Foldback);
    expectedLimit =
        expectedLimit > Config::CurrentLimitFoldbackStep
            ? static_cast<uint8_t>(
                  expectedLimit - Config::CurrentLimitFoldbackStep)
            : Config::CurrentLimitMinimumPower;
    if (expectedLimit < Config::CurrentLimitMinimumPower) {
      expectedLimit = Config::CurrentLimitMinimumPower;
    }
    assert(protection.powerLimit() == expectedLimit);
  }

  assert(protection.powerLimit() == Config::CurrentLimitMinimumPower);
  assert(motorDriver.applied() == Config::CurrentLimitMinimumPower);

  for (uint8_t i = 1;
       i < Config::CurrentLimitEmergencyTripSamples;
       ++i) {
    assert(protection.evaluate(atRight) ==
           CurrentProtectionEvent::None);
    assert(!protection.faultLatched());
  }
  assert(protection.evaluate(atRight) ==
         CurrentProtectionEvent::Trip);
  assert(protection.faultLatched());
  assert(!protection.allowsDrive());
  assert(protection.tripOutput() == Config::CurrentLimitMinimumPower);
  assert(protection.tripSample().rightRaw ==
         Config::CurrentLimitRightRaw);
  assert(motorDriver.target() == 0);
  assert(motorDriver.applied() == 0);

  protection.printTrip(diagnostics);
  const std::string expectedTrip =
      "CURRENT_LIMIT,TRIPPED,OUTPUT,20,L_IS_RAW," +
      std::to_string(Config::CurrentLimitLeftRaw - 1U) +
      ",R_IS_RAW," +
      std::to_string(Config::CurrentLimitRightRaw) + "\n";
  assert(diagnostics.output().find(expectedTrip) != std::string::npos);

  // Safe readings cannot auto-clear a latched hardware-fault fallback.
  assert(protection.evaluate({0, 0}) == CurrentProtectionEvent::None);
  assert(protection.faultLatched());
  assert(protection.clearFault());
  assert(protection.powerLimit() == 100);
  assert(protection.allowsDrive());
}

void motorPowerLimitClampsLiveOutputInBothDirections() {
  FakeStream diagnostics;
  MotorDriver motorDriver(diagnostics);
  motorDriver.begin(0);
  motorDriver.setTarget(100);
  motorDriver.update(200);
  assert(motorDriver.applied() == 100);

  motorDriver.setPowerLimit(60);
  assert(motorDriver.powerLimit() == 60);
  assert(motorDriver.target() == 60);
  assert(motorDriver.applied() == 60);

  motorDriver.setTarget(-100);
  assert(motorDriver.target() == -60);

  motorDriver.setPowerLimit(100);
  motorDriver.setTarget(-100);
  assert(motorDriver.target() == -100);
}

void motorAccelerationAndDecelerationReachFullScaleIn200Ms() {
  FakeStream diagnostics;
  MotorDriver motorDriver(diagnostics);
  motorDriver.begin(0);

  assert(Config::MotorRampIntervalMs == 20);
  assert(Config::MotorAccelStep == 10);
  assert(Config::MotorDecelStep == Config::MotorAccelStep);

  motorDriver.setTarget(100);
  motorDriver.update(19);
  assert(motorDriver.applied() == 0);

  for (uint32_t tick = 1; tick <= 10; ++tick) {
    motorDriver.update(tick * 20);
    assert(motorDriver.applied() == static_cast<int16_t>(tick * 10));
  }
  assert(motorDriver.applied() == 100);

  motorDriver.setTarget(0);
  for (uint32_t tick = 1; tick <= 10; ++tick) {
    motorDriver.update(200 + tick * 20);
    assert(motorDriver.applied() ==
           static_cast<int16_t>(100 - tick * 10));
  }
  assert(motorDriver.applied() == 0);
}

void vehicleAppliesThrottleAndStops() {
  FakeStream diagnostics;
  MotorDriver motorDriver(diagnostics);
  Vehicle vehicle(motorDriver);
  motorDriver.begin(0);

  // The vehicle applies the throttle response curve before the motor driver
  // sees the target, so the effective target is the shaped value.
  const int16_t forward = curveShape(50);
  const int16_t reverse = curveShape(-50);

  vehicle.setThrottle(50);
  assert(vehicle.throttle() == forward);
  assert(motorDriver.target() == forward);
  assert(motorDriver.applied() == 0);

  // Acceleration advances by 10 percentage points per 20 ms.
  motorDriver.update(19);
  assert(motorDriver.applied() == 0);  // no tick yet

  motorDriver.update(20);
  assert(motorDriver.applied() == 10);  // first accel tick

  motorDriver.update(200);
  assert(motorDriver.applied() == forward);

  motorDriver.update(400);
  assert(motorDriver.applied() == forward);  // steady

  // Reversal: decelerate to zero at the configured 10%/20 ms rate, then hold
  // at zero for the 60 ms reversal dwell before ramping the opposite way.
  vehicle.setThrottle(-50);
  motorDriver.update(500);  // batched update reaches zero and starts dwell
  assert(motorDriver.applied() == 0);
  assert(motorDriver.target() == reverse);

  // Reversal dwell: still held at zero at 540 ms (within the 60 ms window).
  motorDriver.update(540);
  assert(motorDriver.applied() == 0);

  // Dwell expired; the next tick accelerates backward at 10%/20 ms.
  motorDriver.update(560);
  assert(motorDriver.applied() == -10);

  motorDriver.update(760);
  assert(motorDriver.applied() == reverse);

  // STOP forces an immediate coast regardless of dynamic braking.
  vehicle.stop();
  assert(vehicle.throttle() == 0);
  assert(motorDriver.target() == 0);
  assert(motorDriver.applied() == 0);
}

void dynamicBrakingEngagesAtNeutralButNotAfterStop() {
  // Only meaningful when dynamic braking and at least one bridge are enabled.
  if constexpr (!Config::EnableDynamicBraking ||
                !Config::EnableBts7960Outputs) {
    return;
  }

  FakeStream diagnostics;
  MotorDriver motorDriver(diagnostics);
  Vehicle vehicle(motorDriver);

  motorDriver.begin(0);
  assert(!motorDriver.brakingActive());  // startup coasts

  // setThrottle shapes through the response curve; the applied value is the
  // shaped target, not the raw 50.
  const int16_t drive = curveShape(50);
  vehicle.setThrottle(50);
  motorDriver.update(200);
  assert(motorDriver.applied() == drive);
  assert(!motorDriver.brakingActive());  // driving, not braking

  // Driver neutral: decelerate to zero, then the bridge shorts to brake.
  vehicle.setThrottle(0);
  motorDriver.update(400);
  assert(motorDriver.applied() == 0);
  assert(motorDriver.brakingActive());

  // STOP releases the brake into a coast for safety.
  vehicle.stop();
  assert(motorDriver.applied() == 0);
  assert(!motorDriver.brakingActive());
}

void vehicleAppliesThrottleResponseCurve() {
  // The default curve (Config::ThrottleCurveExponent == 2) is quadratic:
  // output = sign(in) * |in|^2 / 100, so half trigger gives a quarter output
  // and full trigger still reaches the limit. Linear (exp == 1) is a passthrough.
  FakeStream diagnostics;
  MotorDriver motorDriver(diagnostics);
  Vehicle vehicle(motorDriver);
  motorDriver.begin(0);

  assert(curveShape(0) == 0);
  assert(curveShape(100) == 100);
  assert(curveShape(-100) == -100);
  assert(curveShape(50) == (Config::ThrottleCurveExponent == 1 ? 50 : 25));
  assert(curveShape(-50) == (Config::ThrottleCurveExponent == 1 ? -50 : -25));

  // setThrottle forwards the shaped value to both motor channels.
  vehicle.setThrottle(50);
  assert(vehicle.throttle() == curveShape(50));
  assert(motorDriver.target() == curveShape(50));

  vehicle.setThrottle(0);
  assert(vehicle.throttle() == 0);
  assert(motorDriver.target() == 0);
}

void watchdogTimesOutOnceAndHandlesClockRollover() {
  Watchdog watchdog(500);
  watchdog.begin(0);
  assert(watchdog.isTimedOut());
  assert(!watchdog.update(1000));

  watchdog.refresh(100);
  assert(!watchdog.isTimedOut());
  assert(!watchdog.update(100));
  assert(!watchdog.update(600));
  assert(watchdog.update(601));
  assert(watchdog.isTimedOut());
  assert(!watchdog.update(700));

  watchdog.refresh(UINT32_MAX - 100U);
  assert(!watchdog.update(50));
  assert(watchdog.update(500));
}

}  // namespace

int main() {
  protocolAcceptsIntentCommandsAndRejectsMalformedInput();
  protocolRecoversAfterOverflow();
  protocolWritesStableReplies();
  configuredBuildProfileMatchesExpectedUsbMonitorAccess();
  currentMonitorReportsWindowPeaksOnlyForBts7960Build();
  currentProtectionConfigurationMatchesWheelspinCalibration();
  currentProtectionFoldsBackAfterPersistenceAndRecoversSlowly();
  currentProtectionTripsPersistentOverloadAtMinimumPower();
  motorPowerLimitClampsLiveOutputInBothDirections();
  motorAccelerationAndDecelerationReachFullScaleIn200Ms();
  vehicleAppliesThrottleAndStops();
  dynamicBrakingEngagesAtNeutralButNotAfterStop();
  vehicleAppliesThrottleResponseCurve();
  watchdogTimesOutOnceAndHandlesClockRollover();
  return 0;
}
