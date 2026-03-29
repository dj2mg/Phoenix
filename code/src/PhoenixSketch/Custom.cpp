#include "SDT.h"

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

