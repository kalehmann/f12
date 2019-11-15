#include <check.h>
#include <inttypes.h>
#include <stdlib.h>

#include "../src/libfat12/io_p.h"
#include "tests.h"

START_TEST(test_lf12_read_fat_entry)
{
	uint16_t entry;
	// Test File Allocation Table with 4 clusters: 0x123, 0xabc, 0x161, 0x315
	// Note: Little Endian
	char fat[] = {
		0x23,
		0xc1,
		0xab,
		0x61,
		0x51,
		0x31,
	};

	entry = _lf12_read_fat_entry(fat, 0);
	ck_assert_int_eq(entry, 0x123);

	entry = _lf12_read_fat_entry(fat, 1);
	ck_assert_int_eq(entry, 0xabc);

	entry = _lf12_read_fat_entry(fat, 2);
	ck_assert_int_eq(entry, 0x161);

	entry = _lf12_read_fat_entry(fat, 3);
	ck_assert_int_eq(entry, 0x315);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_cluster_offset)
{
	int offset;
	enum f12_error err;
	struct f12_metadata *f12_meta;

	err = f12_create_metadata(&f12_meta);
	ck_assert_int_eq(F12_SUCCESS, err);

	f12_meta->bpb->SectorSize = 1024;
	f12_meta->bpb->SectorsPerCluster = 2;
	f12_meta->bpb->RootDirEntries = 512;
	// One Sector for boot and two FATs à 5 sectors 
	f12_meta->root_dir_offset = 5 * 1024 * 2 + 1024;

	// The offset of cluster 42 is
	// (cluster - 2) * SectorSize * SectorsPerCluster + RootOffset
	// + RootDirEntries * 32
	// = 109568
	offset = _lf12_cluster_offset(42, f12_meta);

	ck_assert_int_eq(109568, offset);

	f12_free_metadata(f12_meta);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_get_cluster_chain_length)
{
	uint16_t chain_length;
	enum f12_error err;
	struct f12_metadata *f12_meta;
	uint16_t fat_entries[] = {
		0xff0,
		0xfff,
		0x3,
		0x4,
		0x5,
		0x6,
		0x9,
		0x0,
		0x0,
		0xa,
		0xfff,
	};

	err = f12_create_metadata(&f12_meta);
	ck_assert_int_eq(F12_SUCCESS, err);

	f12_meta->fat_entries = fat_entries;
	f12_meta->end_of_chain_marker = 0xfff;

	chain_length = _lf12_get_cluster_chain_length(2, f12_meta);
	ck_assert_int_eq(7, chain_length);

	f12_meta->fat_entries = NULL;
	f12_free_metadata(f12_meta);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_get_cluster_size)
{
	size_t cluster_size;
	enum f12_error err;
	struct f12_metadata *f12_meta;

	err = f12_create_metadata(&f12_meta);
	ck_assert_int_eq(F12_SUCCESS, err);

	f12_meta->bpb->SectorSize = 512;
	f12_meta->bpb->SectorsPerCluster = 4;

	cluster_size = _lf12_get_cluster_size(f12_meta);
	ck_assert_int_eq(2048, cluster_size);

	f12_free_metadata(f12_meta);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_get_cluster_chain_size)
{
	size_t chain_size;
	enum f12_error err;
	struct f12_metadata *f12_meta;
	uint16_t fat_entries[] = {
		0xff0,
		0xfff,
		0x3,
		0x4,
		0x5,
		0x9,
		0x0,
		0x0,
		0x0,
		0xfff,
	};

	err = f12_create_metadata(&f12_meta);
	ck_assert_int_eq(F12_SUCCESS, err);

	f12_meta->fat_entries = fat_entries;
	f12_meta->end_of_chain_marker = 0xfff;
	f12_meta->bpb->SectorSize = 512;
	f12_meta->bpb->SectorsPerCluster = 4;

	// 5 clusters * 4 sectors * 512 bytes = 10240
	chain_size = _lf12_get_cluster_chain_size(2, f12_meta);
	ck_assert_int_eq(10240, chain_size);

	f12_meta->fat_entries = NULL;
	f12_free_metadata(f12_meta);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_read_dir_entry)
{
	struct f12_directory_entry entry = { 0 };

	char entry_data[] = {
		// Short file name
		'S', 'E', 'C', 'R', 'E', 'T', ' ', ' ',
		// Short file extension
		'T', 'X', 'T',
		// File attributes
		0x20,
		// User attributes
		0x07,
		// File creation time or first character
		0x42,
		// Creation time or password hash
		0x34, 0x12,
		// Creation date
		0x78, 0x56,
		// Owner id or last access date
		0xab, 0xac,
		// Access rights
		0x41, 0x31,
		// Time of last modification
		0xad, 0xde,
		// Date of last modification
		0xfe, 0xca,
		// First cluster
		0x37, 0x13,
		// File size
		0x21, 0x43, 0x65, 0x87,
	};

	_lf12_read_dir_entry(entry_data, &entry);

	ck_assert_mem_eq(entry.ShortFileName, "SECRET  ", 8);
	ck_assert_mem_eq(entry.ShortFileExtension, "TXT", 3);
	ck_assert_int_eq(entry.FileAttributes, 0x20);
	ck_assert_int_eq(entry.UserAttributes, 0x07);
	ck_assert_int_eq(entry.CreateTimeOrFirstCharacter, 0x42);
	ck_assert_int_eq(entry.PasswordHashOrCreateTime, 0x1234);
	ck_assert_int_eq(entry.CreateDate, 0x5678);
	ck_assert_int_eq(entry.OwnerIdOrLastAccessDate, 0xacab);
	ck_assert_int_eq(entry.AccessRights, 0x3141);
	ck_assert_int_eq(entry.LastModifiedTime, 0xdead);
	ck_assert_int_eq(entry.LastModifiedDate, 0xcafe);
	ck_assert_int_eq(entry.FirstCluster, 0x1337);
	ck_assert_int_eq(entry.FileSize, 0x87654321);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

START_TEST(test_lf12_create_cluster_chain)
{
	enum f12_error err;
	struct f12_metadata *f12_meta;
	uint16_t first_cluster;

	uint16_t fat_entries[] = {
		0xff0,
		0xfff,
		0x3,
		0x4,
		0xfff,
		0x0,
		0x7,
		0xfff,
		0x0,
		0x0,
	};

	err = f12_create_metadata(&f12_meta);
	ck_assert_int_eq(F12_SUCCESS, err);

	f12_meta->entry_count = 10;
	f12_meta->fat_entries = fat_entries;

	first_cluster = _lf12_create_cluster_chain(f12_meta, 4);
	ck_assert_int_eq(0, first_cluster);

	first_cluster = _lf12_create_cluster_chain(f12_meta, 3);
	ck_assert_int_ne(0, first_cluster);

	f12_meta->fat_entries = NULL;
	f12_free_metadata(f12_meta);
}
// *INDENT-OFF*
END_TEST
// *INDENT-ON*

TCase *libfat12_io_case(void)
{
	TCase *tc_libfat12_io;

	tc_libfat12_io = tcase_create("libfat12 io");
	tcase_add_test(tc_libfat12_io, test_lf12_read_fat_entry);
	tcase_add_test(tc_libfat12_io, test_lf12_cluster_offset);
	tcase_add_test(tc_libfat12_io, test_lf12_get_cluster_chain_length);
	tcase_add_test(tc_libfat12_io, test_lf12_get_cluster_size);
	tcase_add_test(tc_libfat12_io, test_lf12_get_cluster_chain_size);
	tcase_add_test(tc_libfat12_io, test_lf12_read_dir_entry);
	tcase_add_test(tc_libfat12_io, test_lf12_create_cluster_chain);

	return tc_libfat12_io;
}
