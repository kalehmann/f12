#include <check.h>
#include <stdlib.h>

#include "../../src/libfat12/libfat12.h"
#include "../../src/libfat12/name_p.h"
#include "tests.h"

START_TEST(test_lf12_get_path_length)
{
	struct lf12_directory_entry *root_entry;
	struct lf12_directory_entry *tmp_entry;
	size_t path_length;

	// Generate a simple hirarchy for the path /TEST/FOO/BAR.BIN
	// Including the null terminator, the total length of the path is 18
	// characters.
	root_entry = calloc(1, sizeof(struct lf12_directory_entry));
	ck_assert_ptr_nonnull(root_entry);

	root_entry->children = calloc(1, sizeof(struct lf12_directory_entry));
	ck_assert_ptr_nonnull(root_entry->children);
	root_entry->children[0].parent = root_entry;

	tmp_entry = &root_entry->children[0];

	memcpy(tmp_entry->ShortFileName, "TEST    ", 8);
	memcpy(tmp_entry->ShortFileExtension, "   ", 3);

	tmp_entry->children = calloc(1, sizeof(struct lf12_directory_entry));
	ck_assert_ptr_nonnull(tmp_entry->children);
	tmp_entry->children[0].parent = tmp_entry;

	tmp_entry = &tmp_entry->children[0];

	memcpy(tmp_entry->ShortFileName, "FOO     ", 8);
	memcpy(tmp_entry->ShortFileExtension, "   ", 3);

	tmp_entry->children = calloc(1, sizeof(struct lf12_directory_entry));
	ck_assert_ptr_nonnull(tmp_entry->children);
	tmp_entry->children[0].parent = tmp_entry;

	tmp_entry = &tmp_entry->children[0];

	memcpy(tmp_entry->ShortFileName, "BAR     ", 8);
	memcpy(tmp_entry->ShortFileExtension, "BIN", 3);

	path_length = _lf12_get_path_length(tmp_entry);
	ck_assert_int_eq(18, path_length);

	lf12_free_entry(root_entry);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_get_file_name)
{
	char *name;
	name = lf12_get_file_name("TEST    ", "DAT");
	ck_assert_str_eq(name, "TEST.DAT");
	free(name);

	name = lf12_get_file_name("SUBDIR 2", "   ");
	ck_assert_str_eq(name, "SUBDIR 2");
	free(name);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_get_entry_file_name)
{
	struct lf12_directory_entry entry;
	char *name;
	memmove(&entry.ShortFileName, "FILE    ", 8);
	memmove(&entry.ShortFileExtension, "BIN", 3);

	name = lf12_get_entry_file_name(&entry);
	ck_assert_str_eq(name, "FILE.BIN");
	free(name);

	memmove(&entry.ShortFileName, "TEXT  2 ", 8);
	memmove(&entry.ShortFileExtension, "T  ", 3);

	name = lf12_get_entry_file_name(&entry);
	ck_assert_str_eq(name, "TEXT  2.T");
	free(name);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_sanitize_file_name_char)
{
	ck_assert_int_eq(_lf12_sanitize_file_name_char('A'), 'A');
	ck_assert_int_eq(_lf12_sanitize_file_name_char('Z'), 'Z');
	ck_assert_int_eq(_lf12_sanitize_file_name_char('4'), '4');
	ck_assert_int_eq(_lf12_sanitize_file_name_char('-'), '-');
	ck_assert_int_eq(_lf12_sanitize_file_name_char('.'), '_');
	ck_assert_int_eq(_lf12_sanitize_file_name_char('q'), 'Q');
	ck_assert_int_eq(_lf12_sanitize_file_name_char('+'), '_');
	ck_assert_int_eq(_lf12_sanitize_file_name_char(' '), ' ');
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_convert_name)
{
	char *name;

	name = lf12_convert_name("FILE.BIN");
	ck_assert_mem_eq(name, "FILE    BIN", 11);
	free(name);

	name = lf12_convert_name("file.bin");
	ck_assert_mem_eq(name, "FILE    BIN", 11);
	free(name);

	name = lf12_convert_name("reallylongfilename.txt");
	ck_assert_mem_eq(name, "REALLYLOTXT", 11);
	free(name);

	name = lf12_convert_name(" file 1  .t x t  ");
	ck_assert_mem_eq(name, "FILE 1  TXT", 11);
	free(name);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

TCase *libfat12_name_case(void)
{
	TCase *tc_libfat12_name;

	tc_libfat12_name = tcase_create("libfat12 filename");
	tcase_add_test(tc_libfat12_name, test_lf12_get_path_length);
	tcase_add_test(tc_libfat12_name, test_lf12_get_file_name);
	tcase_add_test(tc_libfat12_name, test_lf12_get_entry_file_name);
	tcase_add_test(tc_libfat12_name, test_lf12_sanitize_file_name_char);
	tcase_add_test(tc_libfat12_name, test_lf12_convert_name);

	return tc_libfat12_name;
}
