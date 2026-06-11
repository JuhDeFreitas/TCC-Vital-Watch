/**
 * @file max30102_config.h
 * @brief Configurações centralizadas para MAX30102
 */

#ifndef MAX30102_CONFIG_H
#define MAX30102_CONFIG_H

#include <stdint.h>

// ==================== I2C CONFIGURATION ====================
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_SDA_IO       8
#define I2C_MASTER_SCL_IO       9
#define I2C_MASTER_FREQ_HZ      100000
#define MAX30102_I2C_ADDR       0x57

// ==================== SENSOR CONFIGURATION ====================
#define MAX30102_LED_POWER      0x1F        // 0-255, max power
#define MAX30102_SAMPLE_AVG     4           // 1, 2, 4, 8, 16, 32
#define MAX30102_LED_MODE       2           // 0=RED, 1=RED+IR, 2=RED+IR+GREEN
#define MAX30102_SAMPLE_RATE    100         // Hz (25, 50, 100, 200, 400, 800, 1000, 1600, 3200)
#define MAX30102_PULSE_WIDTH    411         // µs (69, 118, 215, 411)
#define MAX30102_ADC_RANGE      4096        // ADC range (2048, 4096, 8192, 16384)

// ==================== ALGORITHM CONFIGURATION ====================
#define SPO2_BUFFER_SIZE        100         // samples
#define SPO2_UPDATE_INTERVAL    25          // update every N samples
#define SPO2_MIN_VALID          50          // Minimum valid SpO2
#define SPO2_MAX_VALID          100         // Maximum valid SpO2
#define HR_MIN_VALID            30          // Minimum valid HR (BPM)
#define HR_MAX_VALID            200         // Maximum valid HR (BPM)

// ==================== FIFO CONFIGURATION ====================
#define MAX30102_FIFO_SIZE      32          // FIFO buffer size
#define MAX30102_SAMPLES_PER_FIFO 1         // Samples per FIFO read

// ==================== REGISTERS ====================
#define MAX30102_INT_STATUS     0x00
#define MAX30102_INT_ENABLE     0x02
#define MAX30102_FIFO_WR_PTR    0x04
#define MAX30102_FIFO_OVF_CTR   0x05
#define MAX30102_FIFO_RD_PTR    0x06
#define MAX30102_FIFO_DATA      0x07
#define MAX30102_MODE_CONFIG    0x09
#define MAX30102_SPO2_CONFIG    0x0A
#define MAX30102_LED1_PA        0x0C
#define MAX30102_LED2_PA        0x0D
#define MAX30102_TEMP_INT       0x1F
#define MAX30102_TEMP_FRAC      0x20
#define MAX30102_REVISION_ID    0xFE
#define MAX30102_PART_ID        0xFF

// ==================== MODE CONFIG VALUES ====================
#define MODE_HEART_RATE         0x02        // HR only - RED LED
#define MODE_SPO2               0x03        // SpO2 - RED + IR
#define MODE_MULTI_LED          0x07        // Multi-LED mode

// ==================== SPO2 CONFIG VALUES ====================
#define SPO2_SR_25              0x00        // 25 Hz
#define SPO2_SR_50              0x01        // 50 Hz
#define SPO2_SR_100             0x02        // 100 Hz
#define SPO2_SR_200             0x03        // 200 Hz
#define SPO2_SR_400             0x04        // 400 Hz
#define SPO2_SR_800             0x05        // 800 Hz
#define SPO2_SR_1000            0x06        // 1000 Hz
#define SPO2_SR_1600            0x07        // 1600 Hz
#define SPO2_SR_3200            0x08        // 3200 Hz

#define SPO2_PW_69              0x00        // 69 µs
#define SPO2_PW_118             0x01        // 118 µs
#define SPO2_PW_215             0x02        // 215 µs
#define SPO2_PW_411             0x03        // 411 µs

// ==================== DATA VALIDATION ====================
typedef enum {
    SPO2_STATUS_INVALID = 0,
    SPO2_STATUS_VALID = 1,
    SPO2_STATUS_LOW_SIGNAL = 2,
    SPO2_STATUS_POOR_CONTACT = 3
} spo2_status_t;

typedef enum {
    HR_STATUS_INVALID = 0,
    HR_STATUS_VALID = 1,
    HR_STATUS_UNSTABLE = 2
} hr_status_t;

// ==================== RESULT STRUCT ====================
typedef struct {
    int32_t spo2;
    int8_t spo2_valid;
    int32_t heart_rate;
    int8_t hr_valid;
    uint32_t sample_count;
    uint32_t timestamp;
} max30102_result_t;

#endif // MAX30102_CONFIG_H
