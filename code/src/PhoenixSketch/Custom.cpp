#include "SDT.h"

#include <Adafruit_MCP23X08.h>

#pragma region RTC clock
static time_t getBuildTime(void) {
    struct tm tm = { 0 };
    char month[4];
    int day, year;

    sscanf(__DATE__, "%3s %d %d", month, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

    static const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    tm.tm_mon = (strstr(months, month) - months) / 3;
    tm.tm_mday = day;
    tm.tm_year = year - 1900;
    tm.tm_isdst = -1;

    return mktime(&tm);
}

int getLastSundayDay(int year, int month) {
    int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) 
        days[1] = 29;

    int monthDays = days[month - 1];
    
    // Adjust for Zeller's formula
    int m = month, y = year;
    if (month < 3) {
        m += 12;
        y--;
    }

    // Zeller's formula returns 0 = Saturday, 1 = Sunday, ...
    int lastDayWeekday = (monthDays + ((13 * (m + 1)) / 5) + y + (y / 4) - (y / 100) + (y / 400)) % 7;

    int lastSundayDay = monthDays - (lastDayWeekday + 6) % 7;

    return lastSundayDay;
}

int getUtcOffsetSecondsForBerlin(time_t t) {
    int tYear = year(t);
    int tMonth = month(t);
    int tDay = day(t);
    int tHour = hour(t);

    int lastSunMar = getLastSundayDay(tYear, 3);
    int lastSunOct = getLastSundayDay(tYear, 10);

    bool isDst = (tMonth > 3 && tMonth < 10) ||           // clearly in summer
        (tMonth == 3 && tDay > lastSunMar) ||             // after last Sunday in March
        (tMonth == 3 && tDay == lastSunMar && tHour >= 2) ||
        (tMonth == 10 && tDay < lastSunOct) ||            // before last Sunday in October
        (tMonth == 10 && tDay == lastSunOct && tHour < 3);

    return isDst ? 7200 : 3600;   // CEST or CET in seconds
}

void AdjustRtcToUtc(void) {
    time_t tBuild = getBuildTime();
    time_t utc = now() - getUtcOffsetSecondsForBerlin(tBuild);

    setTime(utc);
}
#pragma endregion

#pragma region Rx switch board

// Define GP bits
#define INPUT_RX1   0x0001
#define INPUT_RX2   0x0002
#define INPUT_TX    0x0004
#define ENABLE_VLF  0x0080

#define RXSW_HARDWARE_MASK  0x000F

static Adafruit_MCP23X08 mcpRXSW; // connected to Wire2
static uint8_t rxswAntenna = 0;
static uint8_t rxswVlfEnable = 0;

/* Antenna names must have same length */
char* antennaName[4] = { "TX ", "RX1", "RX2", "n/a" };

static void UpdateRXSWBoard(void);

/**
 * Set up connection to the Rx switch via the BANDS connector on Wire2
 */
errno_t InitializeRXSWBoard(void) {
    errno_t result;
    if (mcpRXSW.begin_I2C(RXSW_MCP23008_ADDR, &Wire2)) {
        bit_results.RXSW_I2C_present = true;
        Debug("Initializing RXSW board");
        mcpRXSW.enableAddrPins();
        // Set all pins to be outputs
        for (int i = 0; i < 8; i++) {
            mcpRXSW.pinMode(i, OUTPUT);
        }
        result = ESUCCESS;
    }
    else {
        bit_results.RXSW_I2C_present = false;
        Debug("RXSW MCP23008 not found at 0x" + String(RXSW_MCP23008_ADDR, HEX));
        result = ENOI2C;
    }

    UpdateRXSWBoard();

    return result;
}

void UpdateRXSWBoard(void) {
    uint8_t gpioBits = 0;

    switch (rxswAntenna) {
    case 1:
        gpioBits |= INPUT_RX1;
        break;
    case 2:
        gpioBits |= INPUT_RX2;
        break;
    default:
        gpioBits |= INPUT_TX;
        break;
    }

    uint64_t hardwareBits = gpioBits;

    if (rxswVlfEnable) {
        gpioBits |= ENABLE_VLF;
        hardwareBits |= ENABLE_VLF >> 4;
    }

    hardwareRegister = (hardwareRegister & ~((uint64_t)RXSW_HARDWARE_MASK << RXSW0BIT)) | (hardwareBits << RXSW0BIT);
    buffer_add();

    if (bit_results.RXSW_I2C_present)
        mcpRXSW.writeGPIO(gpioBits);
}

void SelectRxInput(int8_t band, uint8_t antenna) {
    uint8_t vlfEnable = band == BAND_10M;

    bool update = false;
    if (vlfEnable != rxswVlfEnable) {
        Serial.print("Updating VLF: ");
        Serial.println(vlfEnable);
        update = true;
    }

    if (antenna != rxswAntenna) {
        Serial.print("Updating RX antenna: ");
        Serial.println(antenna);
        update = true;
    }

    if (update) {
        rxswAntenna = antenna;
        rxswVlfEnable = vlfEnable;

        UpdateRXSWBoard();
    }
}

#pragma endregion

