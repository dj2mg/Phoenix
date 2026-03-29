#include <gtest/gtest.h>
#include "Arduino.h"
#include "Custom.h"

time_t CreateTime(int year, int month, int day, int hour) {
	struct tm t = { };
	t.tm_year = year - 1900;
	t.tm_mon = month - 1;
	t.tm_mday = day;
	t.tm_hour = hour;

	return mktime(&t);
}

TEST(CustomTest, GetLastSundayDay_March2026) {
	EXPECT_EQ(getLastSundayDay(2026, 3), 29);
}

TEST(CustomTest, GetLastSundayDay_October2026) {
	EXPECT_EQ(getLastSundayDay(2026, 10), 25);
}

TEST(CustomTest, GetLastSundayDay_2027) {
	EXPECT_EQ(getLastSundayDay(2027, 1), 31);
	EXPECT_EQ(getLastSundayDay(2027, 2), 28);
	EXPECT_EQ(getLastSundayDay(2027, 3), 28);
	EXPECT_EQ(getLastSundayDay(2027, 4), 25);
	EXPECT_EQ(getLastSundayDay(2027, 5), 30);
	EXPECT_EQ(getLastSundayDay(2027, 6), 27);
	EXPECT_EQ(getLastSundayDay(2027, 7), 25);
	EXPECT_EQ(getLastSundayDay(2027, 8), 29);
	EXPECT_EQ(getLastSundayDay(2027, 9), 26);
	EXPECT_EQ(getLastSundayDay(2027, 10), 31);
	EXPECT_EQ(getLastSundayDay(2027, 11), 28);
	EXPECT_EQ(getLastSundayDay(2027, 12), 26);
}

TEST(CustomTest, GetLastSundayDay_2028) {
	EXPECT_EQ(getLastSundayDay(2028, 1), 30);
	EXPECT_EQ(getLastSundayDay(2028, 2), 27);
	EXPECT_EQ(getLastSundayDay(2028, 3), 26);
	EXPECT_EQ(getLastSundayDay(2028, 4), 30);
	EXPECT_EQ(getLastSundayDay(2028, 5), 28);
	EXPECT_EQ(getLastSundayDay(2028, 6), 25);
	EXPECT_EQ(getLastSundayDay(2028, 7), 30);
	EXPECT_EQ(getLastSundayDay(2028, 8), 27);
	EXPECT_EQ(getLastSundayDay(2028, 9), 24);
	EXPECT_EQ(getLastSundayDay(2028, 10), 29);
	EXPECT_EQ(getLastSundayDay(2028, 11), 26);
	EXPECT_EQ(getLastSundayDay(2028, 12), 31);
}

TEST(CustomTest, GetUtcOffsetSecondsForBerlin_FullSummerMonths_2Hours) {
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 4, 1, 0)), 7200);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 6, 1, 0)), 7200);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 9, 1, 0)), 7200);
}

TEST(CustomTest, GetUtcOffsetSecondsForBerlin_WinterMonths_1Hour) {
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 1, 1, 0)), 3600);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 2, 1, 0)), 3600);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 11, 1, 0)), 3600);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 12, 1, 0)), 3600);
}

TEST(CustomTest, GetUtcOffsetSecondsForBerlin_March_BeforeTansitionDay_1Hour) {
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 3, 1, 0)), 3600);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 3, 15, 0)), 3600);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 3, 27, 0)), 3600);
}

TEST(CustomTest, GetUtcOffsetSecondsForBerlin_March_AfterTansitionDay_2Hours) {
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 3, 29, 0)), 7200);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 3, 30, 0)), 7200);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 3, 31, 0)), 7200);
}

TEST(CustomTest, GetUtcOffsetSecondsForBerlin_March_TansitionDay_BeforeTansitionHour_1Hour) {
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 3, 28, 0)), 3600);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 3, 28, 1)), 3600);
}

TEST(CustomTest, GetUtcOffsetSecondsForBerlin_March_TansitionDay_AfterTansitionHour_2Hours) {
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 3, 28, 2)), 7200);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 3, 28, 3)), 7200);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 3, 28, 10)), 7200);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2027, 3, 28, 23)), 7200);
}

TEST(CustomTest, GetUtcOffsetSecondsForBerlin_October_BeforeTansitionDay_2Hours) {
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2028, 10, 1, 0)), 7200);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2028, 10, 16, 0)), 7200);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2028, 10, 28, 0)), 7200);
}

TEST(CustomTest, GetUtcOffsetSecondsForBerlin_October_AfterTansitionDay_1Hour) {
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2028, 10, 30, 0)), 3600);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2028, 10, 31, 0)), 3600);
}

TEST(CustomTest, GetUtcOffsetSecondsForBerlin_October_TansitionDay_BeforeTansitionHour_2Hours) {
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2028, 10, 29, 0)), 7200);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2028, 10, 29, 1)), 7200);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2028, 10, 29, 2)), 7200);
}

TEST(CustomTest, GetUtcOffsetSecondsForBerlin_October_TansitionDay_AfterTansitionHour_1Hour) {
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2028, 10, 29, 3)), 3600);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2028, 10, 29, 5)), 3600);
	EXPECT_EQ(getUtcOffsetSecondsForBerlin(CreateTime(2028, 10, 29, 23)), 3600);
}
