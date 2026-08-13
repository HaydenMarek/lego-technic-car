set(srcs
    "app_main.c"
    "ControllerInput.cpp"
    "DriveOutput.cpp"
    "ServoSteering.cpp"
    "VehicleState.cpp"
    "sketch.cpp")

idf_component_register(SRCS "${srcs}"
    INCLUDE_DIRS "."
    REQUIRES bluepad32 bluepad32_arduino arduino btstack)
