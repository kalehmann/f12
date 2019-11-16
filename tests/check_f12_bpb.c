#include <check.h>
#include <inttypes.h>

#include "../src/bpb.h"
#include "tests.h"

START_TEST(test_f12_initialize_bpb)
{
	struct bios_parameter_block bpb = { 0 };
	struct f12_create_arguments args = {
		.volume_label = "TEST VOLUME",
		.volume_size = 1200,
		.sector_size = 1024,
		.reserved_sectors = 2,
		.number_of_fats = 1,
		.root_dir_entries = 112,
		.drive_number = 1,
	};

	initialize_bpb(&bpb, &args);

	ck_assert_int_eq(bpb.SectorSize, 1024);
	ck_assert_int_eq(bpb.SectorsPerCluster, 1);
	ck_assert_int_eq(bpb.SectorsPerTrack, 15);
	ck_assert_int_eq(bpb.NumberOfHeads, 2);
	ck_assert_int_eq(bpb.RootDirEntries, 112);
	ck_assert_int_eq(bpb.MediumByte, 0xf8);
	ck_assert_int_eq(bpb.LogicalSectors, 1200);
	ck_assert_int_eq(bpb.LargeSectors, 1200);
	ck_assert_str_eq(bpb.VolumeLabel, "TEST VOLUME");
	ck_assert_int_eq(bpb.DriveNumber, 1);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_f12_sectors_per_fat)
{
	struct bios_parameter_block bpb;
	uint16_t secs_per_fat;

	bpb.LogicalSectors = bpb.LargeSectors = 2880;
	bpb.SectorSize = 512;
	bpb.SectorsPerCluster = 1;
	bpb.ReservedForBoot = 1;
	bpb.RootDirEntries = 224;
	bpb.NumberOfFats = 2;

	secs_per_fat = sectors_per_fat(&bpb);
	ck_assert_int_eq(secs_per_fat, 9);

	bpb.LogicalSectors = bpb.LargeSectors = 2400;
	bpb.SectorSize = 512;
	bpb.SectorsPerCluster = 1;
	bpb.ReservedForBoot = 1;
	bpb.RootDirEntries = 224;
	bpb.NumberOfFats = 2;

	secs_per_fat = sectors_per_fat(&bpb);
	ck_assert_int_eq(secs_per_fat, 7);

	bpb.LogicalSectors = bpb.LargeSectors = 560;
	bpb.SectorSize = 512;
	bpb.SectorsPerCluster = 4;
	bpb.ReservedForBoot = 1;
	bpb.RootDirEntries = 224;
	bpb.NumberOfFats = 2;

	secs_per_fat = sectors_per_fat(&bpb);
	ck_assert_int_eq(secs_per_fat, 1);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

TCase *f12_bpb_case(void)
{
	TCase *tc_f12_bpb;

	tc_f12_bpb = tcase_create("f12 bpb");
	tcase_add_test(tc_f12_bpb, test_f12_initialize_bpb);
	tcase_add_test(tc_f12_bpb, test_f12_sectors_per_fat);

	return tc_f12_bpb;
}
