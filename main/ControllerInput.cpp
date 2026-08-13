#include "ControllerInput.h"

#include <stdio.h>

#include <Bluepad32.h>

#include "Config.h"

ControllerInput* ControllerInput::instance_ = nullptr;

namespace {

int16_t clampPercent(int32_t value) {
  return value < -100 ? -100 : (value > 100 ? 100 : static_cast<int16_t>(value));
}

int16_t triggerIntent(int32_t throttle, int32_t brake) {
  const int32_t right = throttle < 0 ? 0 : (throttle > Config::TriggerMaximum
      ? Config::TriggerMaximum : throttle);
  const int32_t left = brake < 0 ? 0 : (brake > Config::TriggerMaximum
      ? Config::TriggerMaximum : brake);
  return clampPercent((right - left) * 100 / Config::TriggerMaximum);
}

}  // namespace

void ControllerInput::begin() {
  instance_ = this;
  BP32.setup(&ControllerInput::connectedCallback,
             &ControllerInput::disconnectedCallback, true);
  BP32.enableVirtualDevice(false);
  BP32.enableBLEService(false);
}

bool ControllerInput::update(ControllerFrame& frame) {
  BP32.update();
  if (controller_ == nullptr || !controller_->isConnected() ||
      !controller_->hasData() || !controller_->isGamepad()) {
    return false;
  }

  frame.connected = true;
  frame.fresh = true;
  frame.armButton = controller_->a();
  frame.disarmButton = controller_->b();
  frame.throttleIntent = triggerIntent(controller_->throttle(), controller_->brake());
  frame.steeringIntent = clampPercent(
      controller_->axisX() * 100 / Config::StickMaximum);
  return true;
}

ControllerEvent ControllerInput::takeEvent() {
  const ControllerEvent event = event_;
  event_ = ControllerEvent::None;
  return event;
}

const char* ControllerInput::controllerModel() const {
  return controllerModel_;
}

void ControllerInput::connectedCallback(ControllerPtr controller) {
  if (instance_ != nullptr) {
    instance_->connected(controller);
  }
}

void ControllerInput::disconnectedCallback(ControllerPtr controller) {
  if (instance_ != nullptr) {
    instance_->disconnected(controller);
  }
}

void ControllerInput::connected(ControllerPtr controller) {
  // v2 deliberately accepts one controller only. The selected controller's
  // exact model, VID, PID, and firmware remain bring-up records.
  if (controller_ == nullptr) {
    controller_ = controller;
    snprintf(controllerModel_, sizeof(controllerModel_), "%s",
             controller->getModelName().c_str());
    const ControllerProperties properties = controller->getProperties();
    vendorId_ = properties.vendor_id;
    productId_ = properties.product_id;
    event_ = ControllerEvent::Connected;
  }
}

void ControllerInput::disconnected(ControllerPtr controller) {
  if (controller_ == controller) {
    controller_ = nullptr;
    snprintf(controllerModel_, sizeof(controllerModel_), "%s", "unknown");
    vendorId_ = 0;
    productId_ = 0;
    event_ = ControllerEvent::Disconnected;
  }
}
