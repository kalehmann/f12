#include <check.h>
#include <stdlib.h>
#include "../src/format.h"
#include "tests.h"

START_TEST(test_f12__f12_format_bytes)
{
	char *temp = NULL;

	temp = _f12_format_bytes(5);
	ck_assert_str_eq("5 bytes", temp);
	free(temp);

	temp = _f12_format_bytes(50000);
	ck_assert_str_eq("48 KiB  ", temp);
	free(temp);

	temp = _f12_format_bytes(123456789);
	ck_assert_str_eq("117 MiB  ", temp);
	free(temp);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_f12__f12_digit_count)
{
	ck_assert_int_eq(2, _f12_digit_count(42));
	ck_assert_int_eq(5, _f12_digit_count(54321));
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_f12__f12__f12_format_bytes_len)
{
	ck_assert_int_eq(_f12__f12_format_bytes_len(5), 7);

	ck_assert_int_eq(_f12__f12_format_bytes_len(50000), 8);
	ck_assert_int_eq(_f12__f12_format_bytes_len(123456789), 9);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

TCase *f12_format_case(void)
{
	TCase *tc_f12_format;

	tc_f12_format = tcase_create("f12 format");
	tcase_add_test(tc_f12_format, test_f12__f12_format_bytes);
	tcase_add_test(tc_f12_format, test_f12__f12_digit_count);
	tcase_add_test(tc_f12_format, test_f12__f12__f12_format_bytes_len);

	return tc_f12_format;
}
