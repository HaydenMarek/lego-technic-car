#include <assert.h>
#include <stdint.h>

#include <string>

#include "MotorDriver.h"
#include "Protocol.h"
#include "Vehicle.h"
#include "Watchdog.h"

namespace {

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
  if constexpr (Config::EnableRightBridge) {
    expected += "MODE,BTS7960_DUAL\n";
  } else if constexpr (Config::EnableBts7960Outputs) {
    expected += "MODE,BTS7960_SINGLE\n";
  } else {
    expected += "MODE,BENCH\n";
  }
  expected += "ACK,STOP\nACK,D,50\nERR\n";
  assert(stream.output() == expected);
}

void vehicleAppliesThrottleToBothMotorsAndStops() {
  FakeStream diagnostics;
  MotorDriver motorDriver(diagnostics);
  Vehicle vehicle(motorDriver);
  constexpr bool expectRightOutput =
      !Config::EnableBts7960Outputs || Config::EnableRightBridge;

  motorDriver.begin(0);
  vehicle.setThrottle(50);
  assert(vehicle.throttle() == 50);
  assert(motorDriver.leftTarget() == 50);
  assert(motorDriver.rightTarget() == (expectRightOutput ? 50 : 0));
  assert(motorDriver.leftApplied() == 0);
  assert(motorDriver.rightApplied() == 0);

  // Acceleration is maximally aggressive: 100% per 20 ms, so a single tick
  // snaps to the commanded throttle.
  motorDriver.update(19);
  assert(motorDriver.leftApplied() == 0);

  motorDriver.update(20);
  assert(motorDriver.leftApplied() == 50);
  assert(motorDriver.rightApplied() == (expectRightOutput ? 50 : 0));

  motorDriver.update(200);
  assert(motorDriver.leftApplied() == 50);
  assert(motorDriver.rightApplied() == (expectRightOutput ? 50 : 0));

  // Reversal: decelerate to zero with the faster decel step (10%/20 ms) in one
  // tick, then, with the reversal dwell removed, snap straight to the opposite
  // target on the next tick.
  vehicle.setThrottle(-50);
  motorDriver.update(380);
  assert(motorDriver.leftApplied() == 0);
  assert(motorDriver.rightApplied() == 0);
  assert(motorDriver.leftTarget() == -50);
  assert(motorDriver.rightTarget() == (expectRightOutput ? -50 : 0));

  // No reversal dwell: the next tick accelerates backward with the aggressive
  // accel step, reaching the target within a single 20 ms tick.
  motorDriver.update(400);
  assert(motorDriver.leftApplied() == -50);
  assert(motorDriver.rightApplied() == (expectRightOutput ? -50 : 0));

  motorDriver.update(640);
  assert(motorDriver.leftApplied() == -50);
  assert(motorDriver.rightApplied() == (expectRightOutput ? -50 : 0));

  // STOP forces an immediate coast regardless of dynamic braking.
  vehicle.stop();
  assert(vehicle.throttle() == 0);
  assert(motorDriver.leftTarget() == 0);
  assert(motorDriver.rightTarget() == 0);
  assert(motorDriver.leftApplied() == 0);
  assert(motorDriver.rightApplied() == 0);
}

void dynamicBrakingEngagesAtNeutralButNotAfterStop() {
  // Only meaningful when dynamic braking and at least one bridge are enabled.
  if constexpr (!Config::EnableDynamicBraking ||
                !Config::EnableLeftBridge) {
    return;
  }

  FakeStream diagnostics;
  MotorDriver motorDriver(diagnostics);
  Vehicle vehicle(motorDriver);

  motorDriver.begin(0);
  assert(!motorDriver.brakingActive());  // startup coasts

  vehicle.setThrottle(50);
  motorDriver.update(200);
  assert(motorDriver.leftApplied() == 50);
  assert(!motorDriver.brakingActive());  // driving, not braking

  // Driver neutral: decelerate to zero, then the bridge shorts to brake.
  vehicle.setThrottle(0);
  motorDriver.update(400);
  assert(motorDriver.leftApplied() == 0);
  assert(motorDriver.brakingActive());

  // STOP releases the brake into a coast for safety.
  vehicle.stop();
  assert(motorDriver.leftApplied() == 0);
  assert(!motorDriver.brakingActive());
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
  vehicleAppliesThrottleToBothMotorsAndStops();
  dynamicBrakingEngagesAtNeutralButNotAfterStop();
  watchdogTimesOutOnceAndHandlesClockRollover();
  return 0;
}
