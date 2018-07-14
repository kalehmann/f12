#include <check.h>
#include <stdlib.h>
#include "../src/libfat12/libfat12.h"
#include "tests.h"

START_TEST(test_f12_get_file_name)
{
  struct f12_directory_entry entry;
  memmove(&entry.ShortFileName, "FILE    ", 8);
  memmove(&entry.ShortFileExtension, "BIN", 3);

  char *name = f12_get_file_name(&entry);
  
  ck_assert_str_eq(name, "FILE.BIN");

  free(name);
}
END_TEST

START_TEST(test_f12_convert_name)
{
  char *name = f12_convert_name("FILE.BIN");

  ck_assert_mem_eq(name, "FILE    BIN", 11);
  
  free(name);
}
END_TEST

TCase *create_libfat12_filename_case(void)
{
  TCase *tc_libfat12_filename;

  tc_libfat12_filename = tcase_create("libfat12 filename");
  tcase_add_test(tc_libfat12_filename, test_f12_get_file_name);
  tcase_add_test(tc_libfat12_filename, test_f12_convert_name);
  
  return tc_libfat12_filename;
}

Suite *f12_suite(void)
{
  Suite *s;
  TCase *tc_libfat12_filename, *tc_libfat12_path;

  s = suite_create("f12");
  tc_libfat12_filename = create_libfat12_filename_case();
  tc_libfat12_path = create_libfat12_path_case();
  suite_add_tcase(s, tc_libfat12_filename);
  suite_add_tcase(s, tc_libfat12_path);

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
