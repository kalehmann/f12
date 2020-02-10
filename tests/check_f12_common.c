#include <check.h>
#include <stdlib.h>
#include "../src/common.h"
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

TCase *f12_common_case(void)
{
	TCase *tc_f12_common;

	tc_f12_common = tcase_create("f12 common");
	tcase_add_test(tc_f12_common, test_f12__f12_format_bytes);

	return tc_f12_common;
}
