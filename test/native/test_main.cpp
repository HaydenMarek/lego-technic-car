#include <assert.h>
#include <stdint.h>

#include <string>

#include "Mixer.h"
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

  stream.receive("PING\r\nD,-100,100\nD,101,0\nD,1,2x\n");

  assert(protocol.poll(command));
  assert(command.type == CommandType::Ping);

  assert(protocol.poll(command));
  assert(command.type == CommandType::Drive);
  assert(command.throttle == -100);
  assert(command.steering == 100);

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
  const DriverCommand drive{CommandType::Drive, 50, -25};

  protocol.sendReady();
  protocol.sendPong();
  protocol.sendStopAcknowledgement();
  protocol.sendDriveAcknowledgement(drive);
  protocol.sendError();

  assert(stream.output() ==
         "READY\nPONG\nACK,STOP\nACK,D,50,-25\nERR\n");
}

void mixerPreservesIntentRatioWithinLimits() {
  MotorTargets targets = Mixer::mix(50, 25);
  assert(targets.left == 75);
  assert(targets.right == 25);

  targets = Mixer::mix(100, 100);
  assert(targets.left == 100);
  assert(targets.right == 0);

  targets = Mixer::mix(80, 40);
  assert(targets.left == 100);
  assert(targets.right == 33);

  targets = Mixer::mix(0, -100);
  assert(targets.left == -100);
  assert(targets.right == 100);
}

void vehicleOwnsMixingAndStopBehavior() {
  FakeStream diagnostics;
  MotorDriver motorDriver(diagnostics);
  Vehicle vehicle(motorDriver);

  motorDriver.begin();
  vehicle.setIntent(50, 25);
  assert(vehicle.throttle() == 50);
  assert(vehicle.steering() == 25);
  assert(motorDriver.targets().left == 75);
  assert(motorDriver.targets().right == 25);

  vehicle.stop();
  assert(vehicle.throttle() == 0);
  assert(vehicle.steering() == 0);
  assert(motorDriver.targets().left == 0);
  assert(motorDriver.targets().right == 0);
}

void watchdogTimesOutOnceAndHandlesClockRollover() {
  Watchdog watchdog(500);
  watchdog.begin(0);
  assert(watchdog.isTimedOut());
  assert(!watchdog.update(1000));

  watchdog.refresh(100);
  assert(!watchdog.isTimedOut());
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
  mixerPreservesIntentRatioWithinLimits();
  vehicleOwnsMixingAndStopBehavior();
  watchdogTimesOutOnceAndHandlesClockRollover();
  return 0;
}

