#include <check.h>
#include <stdlib.h>

#include "../src/libfat12/libfat12.h"
#include "../src/list.h"
#include "tests.h"

/*
 * |-> BIN            | 
 *   |-> COPY         | 1302 bytes
 *   |-> EDIT         | 39  KiB    (40042 bytes)
 *   |-> MOVE         | 455 bytes
 *   |-> DEL          | 768 bytes
 * |-> DEVICES        |
 *   |-> 0            | 1440 KiB   (1474560 bytes)
 *   |-> 80           | 100 MiB   (104857600 bytes)
 * |-> PICTURES       |
 *   |-> IMG_001.BMP  | 731 KiB
 *   |-> IMG_002.BMP  | 839 KiB
 *   |-> IMG_003.BMP  | 644 KiB
 *   |-> (empty)      |
 * |-> SYSTEM         |
 *   |-> (empty)      |
 *   |-> (empty)      |
 *   |-> (empty)      |
 *   |-> (empty)      |
 * |-> KERNEL.BIN     | 62 KiB     (63500 bytes)
 * |-> BOOT.CFG       | 2500 bytes
 * |-> (empty)        |
 * |-> (empty)        |
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

	for (int i = 0; i < 6; i++) {
		child = &dir->children[i];

		if (i < 4) {
			child->FileAttributes = LF12_ATTR_SUBDIRECTORY;
		}
		child->parent = dir;
		child->child_count = 4;
		child->children =
			calloc(4, sizeof(struct lf12_directory_entry));
	}

	memmove(&dir->children[0].ShortFileName, "BIN     ", 8);
	memmove(&dir->children[0].ShortFileExtension, "   ", 3);
	memmove(&dir->children[1].ShortFileName, "DEVICES ", 8);
	memmove(&dir->children[1].ShortFileExtension, "   ", 3);
	memmove(&dir->children[2].ShortFileName, "PICTURES", 8);
	memmove(&dir->children[2].ShortFileExtension, "   ", 3);
	memmove(&dir->children[3].ShortFileName, "SYSTEM  ", 8);
	memmove(&dir->children[3].ShortFileExtension, "   ", 3);
	memmove(&dir->children[4].ShortFileName, "KERNEL  ", 8);
	memmove(&dir->children[4].ShortFileExtension, "BIN", 3);
	dir->children[4].FileSize = 63500;
	memmove(&dir->children[5].ShortFileName, "BOOT    ", 8);
	memmove(&dir->children[5].ShortFileExtension, "CFG", 3);
	dir->children[5].FileSize = 2500;

	child = &dir->children[0];
	memmove(&child->children[0].ShortFileName, "COPY    ", 8);
	memmove(&child->children[0].ShortFileExtension, "   ", 3);
	child->children[0].FileSize = 1302;
	child->children[0].parent = child;
	memmove(&child->children[1].ShortFileName, "EDIT    ", 8);
	memmove(&child->children[1].ShortFileExtension, "   ", 3);
	child->children[1].FileSize = 40042;
	child->children[1].parent = child;
	memmove(&child->children[2].ShortFileName, "MOVE    ", 8);
	memmove(&child->children[2].ShortFileExtension, "   ", 3);
	child->children[2].FileSize = 455;
	child->children[2].parent = child;
	memmove(&child->children[3].ShortFileName, "DEL     ", 8);
	memmove(&child->children[3].ShortFileExtension, "   ", 3);
	child->children[3].FileSize = 768;
	child->children[3].parent = child;

	child = &dir->children[1];
	memmove(&child->children[0].ShortFileName, "0       ", 8);
	memmove(&child->children[0].ShortFileExtension, "   ", 3);
	child->children[0].FileSize = 1474560;
	child->children[0].parent = child;
	memmove(&child->children[1].ShortFileName, "80      ", 8);
	memmove(&child->children[1].ShortFileExtension, "   ", 3);
	child->children[1].FileSize = 104857600;
	child->children[1].parent = child;

	child = &dir->children[0];
	memmove(&child->children[0].ShortFileName, "IMG_001 ", 8);
	memmove(&child->children[0].ShortFileExtension, "BMP", 3);
	child->children[0].FileSize = 731;
	child->children[0].parent = child;
	memmove(&child->children[1].ShortFileName, "IMG_002 ", 8);
	memmove(&child->children[1].ShortFileExtension, "BMP", 3);
	child->children[1].FileSize = 839;
	child->children[1].parent = child;
	memmove(&child->children[2].ShortFileName, "IMG_003 ", 8);
	memmove(&child->children[2].ShortFileExtension, "BMP", 3);
	child->children[2].FileSize = 644;
	child->children[2].parent = child;
}

void teardown(void)
{
	lf12_free_entry(dir);
}

START_TEST(test_f12_list_width)
{
	enum lf12_error err;
	size_t width;

	err = list_width(dir, 4, 2, &width, 1);

	ck_assert_int_eq(err, F12_SUCCESS);
	// The longest lines are
	// "  |-> IMG_001.BMP"
	// "  |-> IMG_002.BMP"
	// "  |-> IMG_003.BMP"
	// with a length of 17 characters each.
	ck_assert_int_eq(width, 17);

	err = list_width(dir, 4, 2, &width, 0);

	ck_assert_int_eq(err, F12_SUCCESS);
	// The longest line in the top level directory is
	// "|-> KERNEL.BIN"
	// with a length of 14 characters.
	ck_assert_int_eq(width, 14);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_f12_list_size_len)
{
	size_t len;
	struct lf12_directory_entry *devices = &dir->children[1];

	len = list_size_len(dir, 1);

	// The longest formated file lengths are
	// "1302 bytes" for BIN/COPY and
	// "2500 bytes" for BOOT.CFG with 10 bytes each;
	ck_assert_int_eq(len, 10);

	len = list_size_len(devices, 0);
	// Device "0" has the longest formatted size
	// "1440 KiB  " with 10 bytes.
	// Note the additional two spaces after KiB. The format_bytes function
	// adds them to let every suffix (bytes, KiB, MiB, GiB) have the same
	// length. This makes it easier to print the sizes aligned in a table.
	ck_assert_int_eq(len, 10);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

TCase *f12_list_case(void)
{
	TCase *tc_f12_list;

	tc_f12_list = tcase_create("f12 list");
	tcase_add_checked_fixture(tc_f12_list, setup, teardown);
	tcase_add_test(tc_f12_list, test_f12_list_width);
	tcase_add_test(tc_f12_list, test_f12_list_size_len);

	return tc_f12_list;
}
