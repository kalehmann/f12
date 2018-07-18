#include <check.h>
#include <stdlib.h>
#include <string.h>

#include "tests.h"
#include "../src/libfat12/libfat12.h"

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
struct f12_directory_entry *dir;

void setup(void)
{
  struct f12_directory_entry *child;
  
  dir = malloc(sizeof(struct f12_directory_entry));
  ck_assert_ptr_nonnull(dir);
  memset(dir, 0, sizeof(struct f12_directory_entry));

  dir->child_count = 8;
  dir->children = malloc(sizeof(struct f12_directory_entry) * 8);
  ck_assert_ptr_nonnull(dir->children);
  memset(dir->children, 0, sizeof(struct f12_directory_entry) * 8);

  for (int i=0; i<3; i++) {
    child = &dir->children[i];

    child->FileAttributes = F12_ATTR_SUBDIRECTORY;
    memmove(&child->ShortFileName, "SUBDIR0 ", 8);
    child->ShortFileName[6] += i + 1;
    memmove(&child->ShortFileExtension, "   ", 3);
    child->child_count = 4;
    child->children = malloc(sizeof(struct f12_directory_entry) * 4);
    ck_assert_ptr_nonnull(child->children);
    memset(child->children, 0, sizeof(struct f12_directory_entry) * 4);
    for (int j=0; j<2; j++) {
      child->children[j].FileAttributes = F12_ATTR_SUBDIRECTORY;
      if (j==0) {
	memmove(&child->children[j].ShortFileName, ".       ", 8);
      } else {
	memmove(&child->children[j].ShortFileName, "..      ", 8);
      }
      memmove(&child->children[j].ShortFileExtension, "   ", 3);
    }
    
    memmove(&child->children[2].ShortFileName, "DATA0   ", 8);
    memmove(&child->children[2].ShortFileExtension, "BIN", 3);
  }
 
  for (int i=3; i<7; i++) {
    memmove(&dir->children[i].ShortFileName, "FILE0   ", 8);
    dir->children[i].ShortFileName[4] += i - 2;
    if (i % 2) {
      memmove(&dir->children[i].ShortFileExtension, "TXT", 3);
    } else {
      memmove(&dir->children[i].ShortFileExtension, "D  ", 3);
    }
  }
}

void teardown(void)
{
  f12_free_entry(dir);
}

START_TEST(test_f12_is_directory)
{
  ck_assert_int_eq(1, f12_is_directory(&dir->children[0]));
  ck_assert_int_eq(0, f12_is_directory(&dir->children[3]));

}
END_TEST

START_TEST(test_f12_is_dot_dir)
{
  ck_assert_int_eq(1, f12_is_dot_dir(&dir->children[0].children[0]));
  ck_assert_int_eq(1, f12_is_dot_dir(&dir->children[0].children[1]));
  ck_assert_int_eq(0, f12_is_dot_dir(&dir->children[0]));
  ck_assert_int_eq(0, f12_is_dot_dir(&dir->children[3]));
  
}
END_TEST

START_TEST(test_f12_entry_is_empty)
{
  ck_assert_int_eq(1, f12_entry_is_empty(&dir->children[7]));
  ck_assert_int_eq(0, f12_entry_is_empty(&dir->children[0]));
}
END_TEST


START_TEST(test_f12_get_child_count)
{
  ck_assert_int_eq(f12_get_child_count(&dir->children[0]), 3);
  ck_assert_int_eq(f12_get_child_count(dir), 7);
}
END_TEST


START_TEST(test_f12_get_file_count)
{
  ck_assert_int_eq(f12_get_file_count(dir), 7);
}
END_TEST

START_TEST(test_f12_get_directory_count)
{
  ck_assert_int_eq(f12_get_directory_count(dir), 3);
}
END_TEST

TCase * libfat12_directory_case(void)
{
  TCase *tc_libfat12_directory;

  tc_libfat12_directory = tcase_create("libfat12 directory");
  tcase_add_unchecked_fixture(tc_libfat12_directory, setup, teardown);
  tcase_add_test(tc_libfat12_directory, test_f12_is_directory);
  tcase_add_test(tc_libfat12_directory, test_f12_is_dot_dir);
  tcase_add_test(tc_libfat12_directory, test_f12_entry_is_empty);
  tcase_add_test(tc_libfat12_directory, test_f12_get_child_count);
  tcase_add_test(tc_libfat12_directory, test_f12_get_file_count);
  tcase_add_test(tc_libfat12_directory, test_f12_get_directory_count);
  
  return tc_libfat12_directory;
}
