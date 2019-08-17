#include <check.h>

#include "../src/libfat12/libfat12.h"
#include "tests.h"

START_TEST(test_f12_generate_volume_id)
{
	uint32_t volume_id = 0;
	enum f12_error err;

	err = f12_generate_volume_id(&volume_id);
	ck_assert_int_eq(err, F12_SUCCESS);

	ck_assert_uint_ne(0, volume_id);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

TCase *libfat12_metadata_case(void)
{
	TCase *tc_libfat12_metadata;

	tc_libfat12_metadata = tcase_create("libfat12 metadata");
	tcase_add_test(tc_libfat12_metadata, test_f12_generate_volume_id);

	return tc_libfat12_metadata;
}
