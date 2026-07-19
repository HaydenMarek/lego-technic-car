#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define F(value) value

constexpr uint8_t LOW = 0;
constexpr uint8_t HIGH = 1;
constexpr uint8_t OUTPUT = 1;

inline void digitalWrite(uint8_t, uint8_t) {}
inline void pinMode(uint8_t, uint8_t) {}
inline void analogWrite(uint8_t, int) {}

template <typename T>
constexpr T max(T left, T right) {
  return left > right ? left : right;
}

class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t value) = 0;

  size_t print(const char* value) {
    size_t written = 0;
    while (*value != '\0') {
      written += write(static_cast<uint8_t>(*value++));
    }
    return written;
  }

  size_t print(char value) { return write(static_cast<uint8_t>(value)); }

  size_t print(int value) {
    char buffer[16]{};
    snprintf(buffer, sizeof(buffer), "%d", value);
    return print(buffer);
  }

  size_t println(const char* value) {
    return print(value) + print('\n');
  }

  size_t println(int value) { return print(value) + print('\n'); }
};

class Stream : public Print {
 public:
  virtual int available() = 0;
  virtual int read() = 0;
};
