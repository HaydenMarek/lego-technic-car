#pragma once

#include <Arduino.h>

#include "Config.h"

enum class CommandType : uint8_t {
  Invalid,
  Ping,
  Stop,
  Drive,
};

struct DriverCommand {
  CommandType type = CommandType::Invalid;
  int16_t throttle = 0;
};

class Protocol final {
 public:
  explicit Protocol(Stream& transport);

  bool poll(DriverCommand& command);

  void sendReady();
  void sendPong();
  void sendStopAcknowledgement();
  void sendDriveAcknowledgement(const DriverCommand& command);
  void sendError();

 private:
  static DriverCommand parse(const char* packet);
  static bool parseNumber(const char*& cursor, char delimiter, int16_t& value);

  Stream& transport_;
  char buffer_[Config::ProtocolBufferSize]{};
  size_t length_ = 0;
  bool overflowed_ = false;
};
