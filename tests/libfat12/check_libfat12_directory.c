#include <check.h>
#include <stdlib.h>
#include <string.h>

#include "tests.h"
#include "../../src/libfat12/libfat12.h"

/*
 * |-> SUBDIR1
 *   |-> .
 *   |-> ..
 *   |->DATA0.BIN
 *   |->(empty)
 * |-> SUBDIR2
 *   |-> .
 *   |-> ..  
 *   |->DATA0.BIN
 *   |->(enmpty)
 * |-> SUBDIR3
 *   |-> .
 *   |-> ..
 *   |->DATA0.BIN
 *   |->(empty)
 * |-> FILE1.TXT
 * |-> FILE2.D
 * |-> FILE3.TXT
 * |-> FILE4.D
 * |-> (empty)
 */
struct lf12_directory_entry *dir;

void setup(void)
{
	struct lf12_directory_entry *child;

	dir = calloc(1, sizeof(struct lf12_directory_entry));
	ck_assert_ptr_nonnull(dir);

	dir->FileAttributes = LF12_ATTR_SUBDIRECTORY;
	dir->child_count = 8;
	dir->parent = NULL;
	dir->children = calloc(8, sizeof(struct lf12_directory_entry));
	ck_assert_ptr_nonnull(dir->children);

	for (int i = 0; i < 3; i++) {
		child = &dir->children[i];

		child->FileAttributes = LF12_ATTR_SUBDIRECTORY;
		memmove(&child->ShortFileName, "SUBDIR0 ", 8);
		child->ShortFileName[6] += i + 1;
		memmove(&child->ShortFileExtension, "   ", 3);
		child->parent = dir;
		child->child_count = 4;
		child->children =
			calloc(4, sizeof(struct lf12_directory_entry));
		ck_assert_ptr_nonnull(child->children);
		for (int j = 0; j < 2; j++) {
			child->children[j].parent = child;
			child->children[j].FileAttributes =
				LF12_ATTR_SUBDIRECTORY;
			if (j == 0) {
				memmove(&child->children[j].ShortFileName,
					".       ", 8);
			} else {
				memmove(&child->children[j].ShortFileName,
					"..      ", 8);
			}
			memmove(&child->children[j].ShortFileExtension, "   ",
				3);
		}

		memmove(&child->children[2].ShortFileName, "DATA0   ", 8);
		memmove(&child->children[2].ShortFileExtension, "BIN", 3);
		child->children[2].parent = child;
	}

	for (int i = 3; i < 7; i++) {
		memmove(&dir->children[i].ShortFileName, "FILE0   ", 8);
		dir->children[i].ShortFileName[4] += i - 2;
		dir->children[i].parent = dir;
		if (i % 2) {
			memmove(&dir->children[i].ShortFileExtension, "TXT", 3);
		} else {
			memmove(&dir->children[i].ShortFileExtension, "D  ", 3);
		}
	}
}

void teardown(void)
{
	lf12_free_entry(dir);
}

START_TEST(test_lf12_is_directory)
{
	ck_assert_int_eq(1, lf12_is_directory(&dir->children[0]));
	ck_assert_int_eq(0, lf12_is_directory(&dir->children[3]));
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_is_dot_dir)
{
	ck_assert_int_eq(1, lf12_is_dot_dir(&dir->children[0].children[0]));
	ck_assert_int_eq(1, lf12_is_dot_dir(&dir->children[0].children[1]));
	ck_assert_int_eq(0, lf12_is_dot_dir(&dir->children[0]));
	ck_assert_int_eq(0, lf12_is_dot_dir(&dir->children[3]));
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_entry_is_empty)
{
	ck_assert_int_eq(1, lf12_entry_is_empty(&dir->children[7]));
	ck_assert_int_eq(0, lf12_entry_is_empty(&dir->children[0]));
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_get_child_count)
{
	ck_assert_int_eq(lf12_get_child_count(&dir->children[0]), 3);
	ck_assert_int_eq(lf12_get_child_count(dir), 7);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_get_file_count)
{
	ck_assert_int_eq(lf12_get_file_count(dir), 7);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_get_directory_count)
{
	ck_assert_int_eq(lf12_get_directory_count(dir), 3);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_move_entry)
{
	/* try to move a directory into a file */
	ck_assert_int_eq(lf12_move_entry(&dir->children[0], &dir->children[3]),
			 F12_NOT_A_DIR);

	/* move subdir2 into subdir1 */
	lf12_move_entry(&dir->children[1], &dir->children[0]);
	ck_assert_mem_eq(&dir->children[0].children[3].ShortFileName,
			 "SUBDIR2 ", 8);

	/* move all children of subdir2 (DATA0.BIN) into subdir3 */
	lf12_move_entry(&dir->children[0].children[3].children[0],
			&dir->children[2]);
	ck_assert_mem_eq(&dir->children[2].children[3].ShortFileName,
			 "DATA0   ", 8);

	/* try to move a file into a full directory */
	ck_assert_int_eq(lf12_move_entry(&dir->children[3], &dir->children[0]),
			 F12_DIR_FULL);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

TCase *libfat12_directory_case(void)
{
	TCase *tc_libfat12_directory;

	tc_libfat12_directory = tcase_create("libfat12 directory");
	tcase_add_checked_fixture(tc_libfat12_directory, setup, teardown);
	tcase_add_test(tc_libfat12_directory, test_lf12_is_directory);
	tcase_add_test(tc_libfat12_directory, test_lf12_is_dot_dir);
	tcase_add_test(tc_libfat12_directory, test_lf12_entry_is_empty);
	tcase_add_test(tc_libfat12_directory, test_lf12_get_child_count);
	tcase_add_test(tc_libfat12_directory, test_lf12_get_file_count);
	tcase_add_test(tc_libfat12_directory, test_lf12_get_directory_count);
	tcase_add_test(tc_libfat12_directory, test_lf12_move_entry);

	return tc_libfat12_directory;
}
