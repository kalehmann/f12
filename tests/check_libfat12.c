#include <check.h>
#include <stdlib.h>
#include "../src/libfat12/libfat12.h"
#include "libfat12/tests.h"

Suite *libfat12_suite(void)
{
	Suite *s;
	TCase *tc_libfat12_directory, *tc_libfat12_io, *tc_libfat12_metadata,
		*tc_libfat12_name, *tc_libfat12_path;

	s = suite_create("libfat12");
	tc_libfat12_directory = libfat12_directory_case();
	tc_libfat12_io = libfat12_io_case();
	tc_libfat12_metadata = libfat12_metadata_case();
	tc_libfat12_name = libfat12_name_case();
	tc_libfat12_path = libfat12_path_case();
	suite_add_tcase(s, tc_libfat12_directory);
	suite_add_tcase(s, tc_libfat12_io);
	suite_add_tcase(s, tc_libfat12_metadata);
	suite_add_tcase(s, tc_libfat12_name);
	suite_add_tcase(s, tc_libfat12_path);

	return s;
}

int main(void)
{
	int number_failed;
	Suite *s;
	SRunner *sr;
	s = libfat12_suite();
	sr = srunner_create(s);

	srunner_run_all(sr, CK_VERBOSE);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
