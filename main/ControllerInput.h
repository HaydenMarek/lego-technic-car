#pragma once

#include <stdint.h>

#include "VehicleState.h"

class Controller;
using ControllerPtr = Controller*;

enum class ControllerEvent : uint8_t { None, Connected, Disconnected };

// Bluepad32 adapter. Keeping its types here prevents controller API details
// from leaking into the vehicle-state and output modules.
class ControllerInput final {
 public:
  void begin();
  bool update(ControllerFrame& frame);
  ControllerEvent takeEvent();
  const char* controllerModel() const;
  uint16_t vendorId() const { return vendorId_; }
  uint16_t productId() const { return productId_; }

 private:
  static void connectedCallback(ControllerPtr controller);
  static void disconnectedCallback(ControllerPtr controller);
  void connected(ControllerPtr controller);
  void disconnected(ControllerPtr controller);
  static ControllerInput* instance_;

  ControllerPtr controller_ = nullptr;
  ControllerEvent event_ = ControllerEvent::None;
  char controllerModel_[64] = "unknown";
  uint16_t vendorId_ = 0;
  uint16_t productId_ = 0;
};
