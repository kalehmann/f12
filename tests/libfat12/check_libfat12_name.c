#include <check.h>
#include <stdlib.h>

#include "../../src/libfat12/libfat12.h"
#include "../../src/libfat12/name_p.h"
#include "tests.h"

START_TEST(test_lf12_get_path_length)
{
	struct f12_directory_entry *root_entry;
	struct f12_directory_entry *tmp_entry;
	size_t path_length;

	// Generate a simple hirarchy for the path /TEST/FOO/BAR.BIN
	// Including the null terminator, the total length of the path is 18
	// characters.
	root_entry = calloc(1, sizeof(struct f12_directory_entry));
	ck_assert_ptr_nonnull(root_entry);

	root_entry->children = calloc(1, sizeof(struct f12_directory_entry));
	ck_assert_ptr_nonnull(root_entry->children);
	root_entry->children[0].parent = root_entry;

	tmp_entry = &root_entry->children[0];

	memcpy(tmp_entry->ShortFileName, "TEST    ", 8);
	memcpy(tmp_entry->ShortFileExtension, "   ", 3);

	tmp_entry->children = calloc(1, sizeof(struct f12_directory_entry));
	ck_assert_ptr_nonnull(tmp_entry->children);
	tmp_entry->children[0].parent = tmp_entry;

	tmp_entry = &tmp_entry->children[0];

	memcpy(tmp_entry->ShortFileName, "FOO     ", 8);
	memcpy(tmp_entry->ShortFileExtension, "   ", 3);

	tmp_entry->children = calloc(1, sizeof(struct f12_directory_entry));
	ck_assert_ptr_nonnull(tmp_entry->children);
	tmp_entry->children[0].parent = tmp_entry;

	tmp_entry = &tmp_entry->children[0];

	memcpy(tmp_entry->ShortFileName, "BAR     ", 8);
	memcpy(tmp_entry->ShortFileExtension, "BIN", 3);

	path_length = _lf12_get_path_length(tmp_entry);
	ck_assert_int_eq(18, path_length);

	f12_free_entry(root_entry);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_f12_get_file_name)
{
	struct f12_directory_entry entry;
	memmove(&entry.ShortFileName, "FILE    ", 8);
	memmove(&entry.ShortFileExtension, "BIN", 3);

	char *name = f12_get_file_name(&entry);

	ck_assert_str_eq(name, "FILE.BIN");

	free(name);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_f12_convert_name)
{
	char *name = f12_convert_name("FILE.BIN");

	ck_assert_mem_eq(name, "FILE    BIN", 11);

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
	tcase_add_test(tc_libfat12_name, test_f12_get_file_name);
	tcase_add_test(tc_libfat12_name, test_f12_convert_name);

	return tc_libfat12_name;
}
