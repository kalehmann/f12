#include <check.h>
#include <stdlib.h>

#include "../../src/libfat12/libfat12.h"
#include "../../src/libfat12/path_p.h"
#include "tests.h"

START_TEST(test_lf12_build_path)
{
	char *input_parts[] = {
		"TEST",
		"FOO",
		"BAR.BIN"
	};
	enum lf12_error err;
	struct lf12_path *path = malloc(sizeof(struct lf12_path));

	ck_assert_ptr_nonnull(path);
	err = _lf12_build_path(input_parts, 3, path);

	ck_assert_str_eq(path->name, "TEST       ");
	ck_assert_str_eq(path->short_file_name, "TEST    ");
	ck_assert_str_eq(path->short_file_extension, "   ");
	ck_assert_ptr_nonnull(path->descendant);
	ck_assert_ptr_null(path->ancestor);

	path = path->descendant;

	ck_assert_str_eq(path->name, "FOO        ");
	ck_assert_str_eq(path->short_file_name, "FOO     ");
	ck_assert_str_eq(path->short_file_extension, "   ");
	ck_assert_ptr_nonnull(path->descendant);
	ck_assert_ptr_nonnull(path->ancestor);

	path = path->descendant;

	ck_assert_str_eq(path->name, "BAR     BIN");
	ck_assert_str_eq(path->short_file_name, "BAR     ");
	ck_assert_str_eq(path->short_file_extension, "BIN");
	ck_assert_ptr_null(path->descendant);
	ck_assert_ptr_nonnull(path->ancestor);

	lf12_free_path(path);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_split_input)
{
	char **input_parts;
	int part_count;
	enum lf12_error err;

	err = _lf12_split_input("TEST/FOO/BAR", &input_parts, &part_count);

	ck_assert_int_eq(err, F12_SUCCESS);
	ck_assert_int_eq(part_count, 3);
	ck_assert_str_eq(input_parts[0], "TEST");
	ck_assert_str_eq(input_parts[1], "FOO");
	ck_assert_str_eq(input_parts[2], "BAR");
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_parse_path)
{
	struct lf12_path *path;

	lf12_parse_path("TEST", &path);
	ck_assert_mem_eq(path->short_file_name, "TEST", 4);

	lf12_free_path(path);

	lf12_parse_path("/FOLDER/SUBDIR/FILE.BIN", &path);
	ck_assert_mem_eq(path->short_file_name, "FOLDER", 6);
	ck_assert_mem_eq(path->descendant->short_file_name, "SUBDIR", 6);
	ck_assert_mem_eq(path->descendant->descendant->short_file_name, "FILE",
			 4);
	ck_assert_mem_eq(path->descendant->descendant->short_file_extension,
			 "BIN", 3);

	lf12_free_path(path);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_path_get_parent)
{
	struct lf12_path *path_a, *path_b, *path_c;

	lf12_parse_path("/TEST/FOO", &path_b);
	lf12_parse_path("/TEST/FOO/BAR", &path_a);
	lf12_parse_path("/DIR/TEST", &path_c);

	ck_assert_int_eq(F12_PATHS_SECOND,
			 lf12_path_get_parent(path_a, path_b));
	ck_assert_int_eq(F12_PATHS_FIRST, lf12_path_get_parent(path_b, path_a));
	ck_assert_int_eq(F12_PATHS_EQUAL, lf12_path_get_parent(path_a, path_a));
	ck_assert_int_eq(F12_PATHS_UNRELATED,
			 lf12_path_get_parent(path_a, path_c));

	lf12_free_path(path_a);
	lf12_free_path(path_b);
	lf12_free_path(path_c);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

TCase *libfat12_path_case(void)
{
	TCase *tc_libfat12_path;

	tc_libfat12_path = tcase_create("libfat12 path");
	tcase_add_test(tc_libfat12_path, test_lf12_build_path);
	tcase_add_test(tc_libfat12_path, test_lf12_split_input);
	tcase_add_test(tc_libfat12_path, test_lf12_parse_path);
	tcase_add_test(tc_libfat12_path, test_lf12_path_get_parent);

	return tc_libfat12_path;
}
