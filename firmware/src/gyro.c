#include "gyro.h"

#include <stdio.h>

#include <hardware/gpio.h>
#include <hardware/i2c.h>
#include <pico/stdlib.h>

#define GYRO_I2C i2c0
#define PIN_SDA 4
#define PIN_SCL 5
#define I2C_HZ 400000
#define I2C_TIMEOUT_US 2000 // a 3-byte read takes ~100 us at 400 kHz

// MPU6500 registers (InvenSense register map RM-MPU-6500A).
#define REG_SMPLRT_DIV 0x19
#define REG_CONFIG 0x1A
#define REG_GYRO_CONFIG 0x1B
#define REG_GYRO_ZOUT_H 0x47
#define REG_PWR_MGMT_1 0x6B
#define REG_WHO_AM_I 0x75

#define DLPF_41HZ 3            // gyro low-pass 41 Hz: we sample at 50 Hz
#define FS_1000DPS (2 << 3)    // +/-1000 deg/s
#define COUNTS_PER_DPS 32.8f   // at +/-1000 deg/s

#define CAL_SAMPLES 200
#define CAL_EVERY_MS 5
#define CAL_MAX_SPREAD_DPS 5.0f // more = the car moved during calibration

static uint8_t addr;
static bool ok;
static float offset_dps;

static bool write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_write_timeout_us(GYRO_I2C, addr, buf, 2, false, I2C_TIMEOUT_US) == 2;
}

static bool read_regs(uint8_t reg, uint8_t *out, size_t n) {
    // nostop = true: repeated start between the register address and the read.
    if (i2c_write_timeout_us(GYRO_I2C, addr, &reg, 1, true, I2C_TIMEOUT_US) != 1) {
        return false;
    }
    return i2c_read_timeout_us(GYRO_I2C, addr, out, n, false, I2C_TIMEOUT_US) == (int)n;
}

static bool read_raw_dps(float *dps) {
    uint8_t b[2];
    if (!read_regs(REG_GYRO_ZOUT_H, b, 2)) {
        return false;
    }
    int16_t raw = (int16_t)((b[0] << 8) | b[1]); // big-endian, two's complement
    *dps = (float)raw / COUNTS_PER_DPS;
    return true;
}

// WHO_AM_I values with the same gyro registers.
static bool known_chip(uint8_t id) {
    return id == 0x70    // MPU6500
           || id == 0x71 // MPU9250
           || id == 0x73 // MPU9255
           || id == 0x68; // MPU6050
}

static bool find_chip(void) {
    const uint8_t addrs[2] = {0x68, 0x69}; // AD0 low / high
    for (int i = 0; i < 2; i++) {
        addr = addrs[i];
        uint8_t id;
        if (read_regs(REG_WHO_AM_I, &id, 1)) {
            if (known_chip(id)) {
                printf("gyro: WHO_AM_I 0x%02x at 0x%02x\n", id, addr);
                return true;
            }
            printf("gyro: unknown WHO_AM_I 0x%02x at 0x%02x\n", id, addr);
        }
    }
    return false;
}

static bool calibrate(void) {
    float sum = 0.0f, lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < CAL_SAMPLES; i++) {
        float v;
        if (!read_raw_dps(&v)) {
            return false;
        }
        sum += v;
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
        sleep_ms(CAL_EVERY_MS);
    }
    offset_dps = sum / CAL_SAMPLES;
    printf("gyro: offset %.2f deg/s, spread %.2f\n", (double)offset_dps, (double)(hi - lo));
    if (hi - lo > CAL_MAX_SPREAD_DPS) {
        printf("gyro: WARNING car moved during calibration. Reboot standing still.\n");
    }
    return true;
}

bool gyro_init(void) {
    i2c_init(GYRO_I2C, I2C_HZ);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    // Weak internal pull-ups. Most modules have their own (to 3.3 V here).
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);

    ok = false;
    if (!find_chip()) {
        printf("gyro: not found, drift assist off\n");
        return false;
    }
    bool setup = write_reg(REG_PWR_MGMT_1, 0x80); // reset
    sleep_ms(100);
    setup = setup && write_reg(REG_PWR_MGMT_1, 0x01)     // wake, PLL clock
            && write_reg(REG_CONFIG, DLPF_41HZ)
            && write_reg(REG_SMPLRT_DIV, 0)
            && write_reg(REG_GYRO_CONFIG, FS_1000DPS);
    sleep_ms(50); // let the gyro settle
    if (!setup || !calibrate()) {
        printf("gyro: setup failed, drift assist off\n");
        return false;
    }
    ok = true;
    return true;
}

bool gyro_ok(void) { return ok; }

bool gyro_read_yaw(float *dps) {
    float v;
    if (!ok || !read_raw_dps(&v)) {
        return false;
    }
    *dps = v - offset_dps;
    return true;
}
