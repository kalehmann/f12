#include <check.h>
#include "../src/libfat12/libfat12.h"
#include "tests.h"


START_TEST(test_f12_parse_path)
{
  struct f12_path *path;

  f12_parse_path("TEST", &path);
  ck_assert_mem_eq(path->short_file_name, "TEST", 4);

  f12_free_path(path);

  f12_parse_path("/FOLDER/SUBDIR/FILE.BIN", &path);
  ck_assert_mem_eq(path->short_file_name, "FOLDER", 6);
  ck_assert_mem_eq(path->descendant->short_file_name, "SUBDIR", 6);
  ck_assert_mem_eq(path->descendant->descendant->short_file_name, "FILE", 4);
  ck_assert_mem_eq(path->descendant->descendant->short_file_extension, "BIN", 3);

  f12_free_path(path);
}
END_TEST

START_TEST(test_f12_path_get_parent)
{
  struct f12_path *path_a, *path_b, *path_c;

  f12_parse_path("/TEST/FOO", &path_b);
  f12_parse_path("/TEST/FOO/BAR", &path_a);
  f12_parse_path("/DIR/TEST", &path_c);

  ck_assert_int_eq(PATHS_SECOND, f12_path_get_parent(path_a, path_b));
  ck_assert_int_eq(PATHS_FIRST, f12_path_get_parent(path_b, path_a));
  ck_assert_int_eq(PATHS_EQUAL, f12_path_get_parent(path_a, path_a));
  ck_assert_int_eq(PATHS_UNRELATED, f12_path_get_parent(path_a, path_c));
  
  f12_free_path(path_a);
  f12_free_path(path_b);
  f12_free_path(path_c);
}
END_TEST

TCase *create_libfat12_path_case(void)
{
  TCase *tc_libfat12_path;

  tc_libfat12_path = tcase_create("libfat12 path");
  tcase_add_test(tc_libfat12_path, test_f12_parse_path);
  tcase_add_test(tc_libfat12_path, test_f12_path_get_parent);
  
  return tc_libfat12_path;
}
