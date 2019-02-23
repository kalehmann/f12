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

TCase *libfat12_name_case(void)
{
        TCase * tc_libfat12_name;

        tc_libfat12_name = tcase_create("libfat12 filename");
        tcase_add_test(tc_libfat12_name, test_f12_get_file_name);
        tcase_add_test(tc_libfat12_name, test_f12_convert_name);

        return tc_libfat12_name;
}


