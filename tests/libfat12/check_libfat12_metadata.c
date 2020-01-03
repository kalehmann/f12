#define _XOPEN_SOURCE
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <check.h>

#include "../../src/libfat12/libfat12.h"
#include "tests.h"

START_TEST(test_lf12_generate_volume_id)
{
	uint32_t volume_id = 0;
	enum lf12_error err;

	err = lf12_generate_volume_id(&volume_id);
	ck_assert_int_eq(err, F12_SUCCESS);

	ck_assert_uint_ne(0, volume_id);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_generate_entry_timestamp)
{
	void *ret;
	/**
	 * 2018-06-10T16:33:15.1110+00:00
	 *
         * @see https://photojournal.jpl.nasa.gov/catalog/PIA22930
         */
	long utime = 1528648395420000;
	time_t timer = utime / 1000000;

	uint16_t date, time;
	uint8_t msecs;

	lf12_generate_entry_timestamp(utime, &date, &time, &msecs);

	/*
	 * year = 2018 - 1980 = 38
	 * year << 9 = 19456
	 * The months are zero based:
	 * month = 6 
	 * month << 5 = 192
	 * day = 10
	 * 19456 + 192 + 10 = 19658
	 */
	ck_assert_int_eq(date, 19658);
	/*
	 * hour = 16
	 * hour << 11 = 32768
	 * minute = 33
	 * 33 < 5 = 1056
	 * seconds = 15 // 2 = 7
	 * 32768 + 1056 + 7 = 33831 
	 */
	ck_assert_int_eq(time, 33831);
	/*
	 * 0.111 + 15 % 2  = 1.111
	 * 1111 // 10 = 111
	 */
	ck_assert_int_eq(msecs, 142);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_read_entry_timestamp)
{
	struct tm *timeinfo;
	uint16_t p_date = 19658, p_time = 33831;
	uint8_t p_msecs = 142;
	char *restrict buf = malloc(80);
	ck_assert_ptr_nonnull(buf);

	long usecs = lf12_read_entry_timestamp(p_date, p_time, p_msecs);
	time_t timer = usecs / 1000000;

	timeinfo = gmtime(&timer);
	ck_assert_ptr_nonnull(timeinfo);

	strftime(buf, 80, "%Y-%m-%dT%H:%M:%S", timeinfo);

	ck_assert_str_eq("2018-06-10T16:33:15", buf);
	ck_assert_int_eq(1528648395420000, usecs);

	free(buf);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

TCase *libfat12_metadata_case(void)
{
	TCase *tc_libfat12_metadata;

	tc_libfat12_metadata = tcase_create("libfat12 metadata");
	tcase_add_test(tc_libfat12_metadata, test_lf12_generate_volume_id);
	tcase_add_test(tc_libfat12_metadata,
		       test_lf12_generate_entry_timestamp);
	tcase_add_test(tc_libfat12_metadata, test_lf12_read_entry_timestamp);

	return tc_libfat12_metadata;
}
