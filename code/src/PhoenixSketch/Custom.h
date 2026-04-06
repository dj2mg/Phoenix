#ifndef CUSTOM_H
#define CUSTOM_H

#define RXSW0BIT	(TXVFOBIT + 1)
#define RXSW3BIT	(TXVFOBIT + 4)

/**
 * @brief Adjusts the RTC to UTC
 */
void AdjustRtcToUtc(void);

int getLastSundayDay(int year, int month);
int getUtcOffsetSecondsForBerlin(time_t t);

/* Display name of antennas */
extern char* antennaName[4];

/**
 * @brief Initialize the RX switch board hardware and GPIO control
 * @return ESUCCESS on success, ENOI2C if I2C communication fails
 * @note Configures MCP23008 GPIO expander rx input control
 */
errno_t InitializeRXSWBoard(void);

/**
 * @brief Updates the current RX input
 */
void SelectRxInput(int8_t band, uint8_t antenna);

#endif // #define CUSTOM_H
