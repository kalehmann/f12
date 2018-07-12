#include <check.h>
#include "../src/libfat12/libfat12.h"
#include "tests.h"


START_TEST(test_parse_f12_path)
{
  struct f12_path *path;

  parse_f12_path("TEST", &path);
  ck_assert_mem_eq(path->short_file_name, "TEST", 4);

  free_f12_path(path);

  parse_f12_path("/FOLDER/SUBDIR/FILE.BIN", &path);
  ck_assert_mem_eq(path->short_file_name, "FOLDER", 6);
  ck_assert_mem_eq(path->descendant->short_file_name, "SUBDIR", 6);
  ck_assert_mem_eq(path->descendant->descendant->short_file_name, "FILE", 4);
  ck_assert_mem_eq(path->descendant->descendant->short_file_extension, "BIN", 3);

  free_f12_path(path);
}
END_TEST

START_TEST(test_path_f12_get_parent)
{
  struct f12_path *path_a, *path_b, *path_c;

  parse_f12_path("/TEST/FOO", &path_b);
  parse_f12_path("/TEST/FOO/BAR", &path_a);
  parse_f12_path("/DIR/TEST", &path_c);

  ck_assert_int_eq(PATHS_SECOND, f12_get_parent(path_a, path_b));
  ck_assert_int_eq(PATHS_FIRST, f12_get_parent(path_b, path_a));
  ck_assert_int_eq(PATHS_EQUAL, f12_get_parent(path_a, path_a));
  ck_assert_int_eq(PATHS_UNRELATED, f12_get_parent(path_a, path_c));
  
  free_f12_path(path_a);
  free_f12_path(path_b);
  free_f12_path(path_c);
}
END_TEST

TCase *create_libfat12_path_case(void)
{
  TCase *tc_libfat12_path;

  tc_libfat12_path = tcase_create("libfat12 path");
  tcase_add_test(tc_libfat12_path, test_parse_f12_path);
  tcase_add_test(tc_libfat12_path, test_path_f12_get_parent);
  
  return tc_libfat12_path;
}
