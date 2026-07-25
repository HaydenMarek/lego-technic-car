#include <assert.h>
#include <stdint.h>

#include <string>

#include "MotorDriver.h"
#include "Protocol.h"
#include "Vehicle.h"
#include "Watchdog.h"

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

  // Acceleration is gentle: 5% per 20 ms, so 0..50 takes 200 ms (10 ticks).
  motorDriver.update(19);
  assert(motorDriver.applied() == 0);  // no tick yet

  motorDriver.update(20);
  assert(motorDriver.applied() == 5);  // first accel tick

  motorDriver.update(200);
  assert(motorDriver.applied() == forward);  // reached target after 200 ms

  motorDriver.update(400);
  assert(motorDriver.applied() == forward);  // steady

  // Reversal: decelerate to zero with the faster decel step (10%/20 ms) over
  // 100 ms, then hold at zero for the 60 ms reversal dwell before ramping the
  // opposite way.
  vehicle.setThrottle(-50);
  motorDriver.update(500);  // 100 ms after 400: 5 ticks * 10% = 50 -> 0
  assert(motorDriver.applied() == 0);
  assert(motorDriver.target() == reverse);

  // Reversal dwell: still held at zero at 540 ms (within the 60 ms window).
  motorDriver.update(540);
  assert(motorDriver.applied() == 0);

  // Dwell expired; the next ticks accelerate backward at 5%/20 ms.
  motorDriver.update(560);
  assert(motorDriver.applied() == -5);

  motorDriver.update(760);  // 200 ms of accel after the dwell reaches -50
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
  vehicleAppliesThrottleAndStops();
  dynamicBrakingEngagesAtNeutralButNotAfterStop();
  vehicleAppliesThrottleResponseCurve();
  watchdogTimesOutOnceAndHandlesClockRollover();
  return 0;
}
