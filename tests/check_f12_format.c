#include <check.h>
#include "../src/format.h"
#include "tests.h"

START_TEST(test_f12_digit_count)
{
	ck_assert_int_eq(2, digit_count(42));
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

TCase *f12_format_case(void)
{
	TCase *tc_f12_format;

	tc_f12_format = tcase_create("f12 format");
	tcase_add_test(tc_f12_format, test_f12_digit_count);

	return tc_f12_format;
}


