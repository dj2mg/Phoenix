#ifndef UTCCLOCK_H
#define UTCCLOCK_H

/**
 * @brief Adjusts the RTC to UTC
 */
void AdjustRtcToUtc(void);

int getLastSundayDay(int year, int month);
int getUtcOffsetSecondsForBerlin(time_t t);

#endif // UTCCLOCK_H
