// Mock version to enable test harness to compile

#include <cstdint>
#include "Wire.h"

// MCP23X17-specific constants
// (Arduino constants like CHANGE, INPUT_PULLUP, LOW, HIGH are defined in Arduino.h)
#define MCP23XXX_INT_ERR 255

class Adafruit_MCP23X08 {
public:
    Adafruit_MCP23X08() {}
    ~Adafruit_MCP23X08() {}

    bool begin(uint8_t addr = 0, TwoWire *theWire = nullptr) { return true; }
    bool begin_I2C(uint8_t addr = 0){ return true; }
    bool begin_I2C(uint8_t addr, void *theWire){ return true; }
    void enableAddrPins() {}
    void pinMode(uint8_t pin, uint8_t mode) {}
    void digitalWrite(uint8_t pin, uint8_t value) {}
    uint8_t digitalRead(uint8_t pin) { return 0; }
    void writeGPIO(uint8_t value, uint8_t port = 0) { gpioval = value; }
    uint8_t readGPIO(uint8_t port = 0) { return gpioval; }
    void setupInterruptPin(uint8_t pin, uint8_t mode) {}
    uint8_t getLastInterruptPin() { return MCP23XXX_INT_ERR; }
    void clearInterrupts() {}
    void setupInterrupts(bool mirror, bool openDrain, uint8_t polarity) {}
private:
    uint8_t gpioval;
};
