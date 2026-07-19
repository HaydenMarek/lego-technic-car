#include "Protocol.h"

#include <limits.h>
#include <string.h>

Protocol::Protocol(Stream& transport) : transport_(transport) {}

bool Protocol::poll(DriverCommand& command) {
  while (transport_.available() > 0) {
    const char received = static_cast<char>(transport_.read());

    if (received == '\r') {
      continue;
    }

    if (received == '\n') {
      if (overflowed_) {
        length_ = 0;
        overflowed_ = false;
        command = DriverCommand{};
        return true;
      }

      if (length_ == 0) {
        continue;
      }

      buffer_[length_] = '\0';
      command = parse(buffer_);
      length_ = 0;
      return true;
    }

    if (overflowed_) {
      continue;
    }

    if (length_ < sizeof(buffer_) - 1U) {
      buffer_[length_++] = received;
    } else {
      length_ = 0;
      overflowed_ = true;
    }
  }

  return false;
}

void Protocol::sendReady() { transport_.println(F("READY")); }

void Protocol::sendPong() { transport_.println(F("PONG")); }

void Protocol::sendStopAcknowledgement() {
  transport_.println(F("ACK,STOP"));
}

void Protocol::sendDriveAcknowledgement(const DriverCommand& command) {
  transport_.print(F("ACK,D,"));
  transport_.print(command.throttle);
  transport_.print(',');
  transport_.println(command.steering);
}

void Protocol::sendError() { transport_.println(F("ERR")); }

DriverCommand Protocol::parse(const char* packet) {
  if (strcmp(packet, "PING") == 0) {
    return {CommandType::Ping, 0, 0};
  }

  if (strcmp(packet, "STOP") == 0) {
    return {CommandType::Stop, 0, 0};
  }

  if (packet[0] != 'D' || packet[1] != ',') {
    return {};
  }

  const char* cursor = packet + 2;
  int16_t throttle = 0;
  int16_t steering = 0;
  if (!parseNumber(cursor, ',', throttle) ||
      !parseNumber(cursor, '\0', steering)) {
    return {};
  }

  if (throttle < Config::IntentMinimum ||
      throttle > Config::IntentMaximum ||
      steering < Config::IntentMinimum ||
      steering > Config::IntentMaximum) {
    return {};
  }

  return {CommandType::Drive, throttle, steering};
}

bool Protocol::parseNumber(const char*& cursor, char delimiter, int16_t& value) {
  bool negative = false;
  if (*cursor == '-') {
    negative = true;
    ++cursor;
  }

  if (*cursor < '0' || *cursor > '9') {
    return false;
  }

  const int32_t limit = negative ? -(static_cast<int32_t>(INT16_MIN))
                                 : INT16_MAX;
  int32_t parsed = 0;
  while (*cursor >= '0' && *cursor <= '9') {
    const int32_t digit = *cursor - '0';
    if (parsed > (limit - digit) / 10) {
      return false;
    }
    parsed = parsed * 10 + digit;
    ++cursor;
  }

  if (*cursor != delimiter) {
    return false;
  }

  if (delimiter != '\0') {
    ++cursor;
  }

  value = static_cast<int16_t>(negative ? -parsed : parsed);
  return true;
}
