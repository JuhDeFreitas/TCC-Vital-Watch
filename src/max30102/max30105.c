#include "max30105.h"
#include "string.h"

// ---------------- I2C LOW LEVEL ----------------

static void write_reg(max30105_t *dev, uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    i2c_master_write_to_device(dev->port, dev->addr, data, 2, 1000 / portTICK_PERIOD_MS);
}

static uint8_t read_reg(max30105_t *dev, uint8_t reg) {
    uint8_t val = 0;
    i2c_master_write_read_device(dev->port, dev->addr,
                                 &reg, 1,
                                 &val, 1,
                                 1000 / portTICK_PERIOD_MS);
    return val;
}

// ---------------- BIT MASK ----------------

static void bitmask(max30105_t *dev, uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t original = read_reg(dev, reg);
    original &= mask;
    write_reg(dev, reg, original | value);
}

// ---------------- INIT ----------------

bool max30105_init(max30105_t *dev, i2c_port_t port, uint8_t addr) {
    dev->port = port;
    dev->addr = addr;
    dev->sense.head = 0;
    dev->sense.tail = 0;

    uint8_t part_id = read_reg(dev, 0xFF);
    if (part_id != 0x15) {
        return false;
    }

    return true;
}

// ---------------- RESET ----------------

void max30105_soft_reset(max30105_t *dev) {
    bitmask(dev, 0x09, 0xBF, 0x40);

    for (int i = 0; i < 100; i++) {
        uint8_t v = read_reg(dev, 0x09);
        if ((v & 0x40) == 0) break;
    }
}

// ---------------- SETUP (simplificado) ----------------

void max30105_setup(max30105_t *dev, uint8_t powerLevel, uint8_t sampleAverage,
                    uint8_t ledMode, int sampleRate, int pulseWidth, int adcRange) {

    max30105_soft_reset(dev);

    // FIFO rollover - permite que FIFO sobrescreva dados antigos
    bitmask(dev, 0x08, 0xEF, 0x10);

    // LED mode (Registrador 0x09)
    // Bits [2:0]: Mode
    // 000 = Heart Rate only (RED LED)
    // 001 = SpO2 (RED + IR)
    // 011 = SpO2 (RED + IR) mode
    // 111 = Multi-LED (RED + IR + GREEN)
    bitmask(dev, 0x09, 0xF8, (ledMode == 3) ? 0x07 :
                              (ledMode == 2) ? 0x03 : 0x02);

    dev->activeLEDs = ledMode;

    // SPO2 Configuration (Registrador 0x0A)
    // Bits [6:5] = ADC Range
    //   00 = 2048nA
    //   01 = 4096nA (padrão recomendado)
    //   10 = 8192nA
    //   11 = 16384nA
    bitmask(dev, 0x0A, 0x9F, 0x20);  // 4096nA range
    
    // Bits [4:2] = Sample Rate
    //   000 = 50 Hz
    //   001 = 100 Hz (padrão recomendado para SpO2)
    //   010 = 200 Hz
    //   011 = 400 Hz
    //   100 = 800 Hz
    //   101 = 1000 Hz
    //   110 = 1600 Hz
    //   111 = 3200 Hz
    // Nota: 100 Hz é o padrão nativo recomendado pelo algoritmo Maxim
    bitmask(dev, 0x0A, 0xE3, 0x04);  // 100 Hz (valor 2 << 2 = 0x04)
    
    // Bits [1:0] = Pulse Width
    //   00 = 69 µs, ADC resolution = 15 bits
    //   01 = 118 µs, ADC resolution = 16 bits
    //   10 = 215 µs, ADC resolution = 17 bits
    //   11 = 411 µs, ADC resolution = 18 bits (máxima resolução)
    bitmask(dev, 0x0A, 0xFC, 0x03);  // 411 µs, 18 bits

    // LED Power Amplitude
    // Registrador 0x0C = RED LED
    // Registrador 0x0D = IR LED
    // Registrador 0x0E = GREEN LED
    write_reg(dev, 0x0C, powerLevel);  // RED
    write_reg(dev, 0x0D, powerLevel);  // IR
    if (ledMode == 3) {
        write_reg(dev, 0x0E, powerLevel);  // GREEN
    }

    // FIFO clear - Limpar buffer FIFO
    write_reg(dev, 0x04, 0);  // FIFO_WR_PTR
    write_reg(dev, 0x05, 0);  // FIFO_OVF_CTR
    write_reg(dev, 0x06, 0);  // FIFO_RD_PTR
}

// ---------------- FIFO CHECK ----------------

static uint8_t get_write_ptr(max30105_t *dev) {
    return read_reg(dev, 0x04);
}

static uint8_t get_read_ptr(max30105_t *dev) {
    return read_reg(dev, 0x06);
}

uint16_t max30105_check(max30105_t *dev) {

    uint8_t readPtr = get_read_ptr(dev);
    uint8_t writePtr = get_write_ptr(dev);

    if (readPtr == writePtr) return 0;

    int samples = writePtr - readPtr;
    if (samples < 0) samples += 32;

    int bytes = samples * dev->activeLEDs * 3;

    uint8_t reg = 0x07;
    i2c_master_write_to_device(dev->port, dev->addr, &reg, 1, 1000);

    uint8_t buffer[I2C_BUFFER_LENGTH];

    while (bytes > 0) {

        int toRead = (bytes > I2C_BUFFER_LENGTH) ? I2C_BUFFER_LENGTH : bytes;

        i2c_master_read_from_device(dev->port, dev->addr, buffer, toRead, 1000);

        for (int i = 0; i < toRead; i += 3 * dev->activeLEDs) {

            dev->sense.head = (dev->sense.head + 1) % STORAGE_SIZE;

            uint32_t red = (buffer[i] << 16) | (buffer[i+1] << 8) | buffer[i+2];
            red &= 0x3FFFF;

            dev->sense.red[dev->sense.head] = red;

            if (dev->activeLEDs > 1) {
                uint32_t ir = (buffer[i+3] << 16) | (buffer[i+4] << 8) | buffer[i+5];
                ir &= 0x3FFFF;
                dev->sense.IR[dev->sense.head] = ir;
            }
        }

        bytes -= toRead;
    }

    return samples;
}

// ---------------- FIFO HELPERS ----------------

uint8_t max30105_available(max30105_t *dev) {
    int n = dev->sense.head - dev->sense.tail;
    if (n < 0) n += STORAGE_SIZE;
    return (uint8_t)n;
}

uint32_t max30105_get_red(max30105_t *dev) {
    return dev->sense.red[dev->sense.head];
}

uint32_t max30105_get_ir(max30105_t *dev) {
    return dev->sense.IR[dev->sense.head];
}

uint32_t max30105_get_green(max30105_t *dev) {
    return dev->sense.green[dev->sense.head];
}

void max30105_next_sample(max30105_t *dev) {
    if (max30105_available(dev)) {
        dev->sense.tail = (dev->sense.tail + 1) % STORAGE_SIZE;
    }
}