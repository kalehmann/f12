#include <check.h>
#include <stdlib.h>
#include "tests.h"

Suite *f12_suite(void)
{
	Suite *s;
	TCase *tc_f12_create, *tc_f12_common, *tc_f12_list;

	s = suite_create("f12");
	tc_f12_create = f12_create_case();
	tc_f12_common = f12_common_case();
	tc_f12_list = f12_list_case();
	suite_add_tcase(s, tc_f12_create);
	suite_add_tcase(s, tc_f12_common);
	suite_add_tcase(s, tc_f12_list);

	return s;
}

int main(void)
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = f12_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
