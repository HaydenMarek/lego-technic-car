# External documentation

## v2 references

- [Bluepad32 documentation](https://bluepad32.readthedocs.io/en/latest/) —
  controller-host overview and supported-controller information. The original
  ESP32 supports the broadest listed controller set; confirm the installed Xbox
  model and firmware at physical bring-up.
- [Bluepad32 Arduino + ESP32 guidance](https://bluepad32.readthedocs.io/en/latest/plat_arduino/)
  and its [ESP-IDF + Arduino + Bluepad32 template](https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template)
  — the supported PlatformIO integration retained in
  `third_party/bluepad32-template`. It supplies Bluepad32, BTstack, and its
  matching Arduino bridge as a pinned submodule.
- [Arduino-ESP32 LEDC API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html)
  — `ledcAttach()` and `ledcWrite()` configure the ESP32's PWM used for the
  BTS7960 inputs and 50 Hz hobby-servo signal.
- [Infineon BTS 7960B/P datasheet](https://www.infineon.com/assets/row/public/documents/10/57/infineon-bts7960-ds-en.pdf)
  — chip-level PWM and protection reference only. It does not validate an
  IBT-2 clone's wiring, thermal performance, or 3.3 V logic threshold.
- [ESP-IDF Component Manager FAQ](https://docs.espressif.com/projects/idf-component-manager/en/latest/troubleshooting/faq.html)
  — version-control guidance for generated `managed_components/` content and
  the reproducibility role of `dependencies.lock`.

No servo, Technic adapter, or buck converter part number has been selected yet.
Add the selected components' vendor datasheets here during authorized physical
bring-up; their signal voltage, travel, and stall-current values are not safe
to assume from a generic hobby-servo description.

## Historical v1 research

The Pybricks, Technic Hub, and LEGO UART references remain in
[`legacy/v1/EXTERNAL_DOCUMENTATION.md`](legacy/v1/EXTERNAL_DOCUMENTATION.md).
They are historical research, not v2 dependencies.
