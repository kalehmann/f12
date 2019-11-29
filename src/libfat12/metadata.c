#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "libfat12.h"

enum lf12_error lf12_create_root_dir_meta(struct lf12_metadata *f12_meta)
{
	enum lf12_error err;
	struct bios_parameter_block *bpb = f12_meta->bpb;
	size_t fat_size = bpb->SectorsPerFat * bpb->SectorSize;
	uint16_t cluster_count =
		bpb->LogicalSectors / bpb->SectorsPerCluster + 2;
	struct lf12_directory_entry *root_dir = NULL;
	struct lf12_directory_entry *root_entries = NULL;

	root_dir = calloc(1, sizeof(struct lf12_directory_entry));
	if (NULL == root_dir) {
		return F12_ALLOCATION_ERROR;
	}

	root_entries =
		calloc(bpb->RootDirEntries,
		       sizeof(struct lf12_directory_entry));
	if (NULL == root_entries) {
		free(root_dir);

		return F12_ALLOCATION_ERROR;
	}

	memset(root_dir->ShortFileName, ' ', 8);
	memset(root_dir->ShortFileExtension, ' ', 3);
	root_dir->parent = NULL;
	root_dir->FileAttributes |= LF12_ATTR_SUBDIRECTORY;
	root_dir->children = root_entries;
	root_dir->child_count = bpb->RootDirEntries;

	f12_meta->root_dir = root_dir;

	for (int i = 0; i < bpb->RootDirEntries; i++) {
		root_entries[i].parent = root_dir;
	}

	f12_meta->entry_count = cluster_count;
	f12_meta->fat_entries = calloc(cluster_count, sizeof(uint16_t));
	if (NULL == f12_meta->fat_entries) {
		free(root_entries);
		free(root_dir);

		return F12_ALLOCATION_ERROR;
	}
	f12_meta->fat_id = ((uint16_t) bpb->MediumByte) | 0xf00;
	f12_meta->fat_entries[0] = f12_meta->fat_id;
	f12_meta->end_of_chain_marker = 0xfff;
	f12_meta->fat_entries[1] = 0xfff;

	return F12_SUCCESS;
}

size_t lf12_get_partition_size(struct lf12_metadata *f12_meta)
{
	struct bios_parameter_block *bpb = f12_meta->bpb;

	return bpb->SectorSize * bpb->LogicalSectors;
}

size_t lf12_get_used_bytes(struct lf12_metadata *f12_meta)
{
	struct bios_parameter_block *bpb = f12_meta->bpb;
	uint16_t cluster_count = bpb->LogicalSectors / bpb->SectorsPerCluster;

	size_t used_bytes = bpb->SectorSize *
		(bpb->ReservedForBoot + bpb->NumberOfFats * bpb->SectorsPerFat)
		+ bpb->RootDirEntries * 32;

	for (int i = 2; i < cluster_count; i++) {
		if (f12_meta->fat_entries[i])
			used_bytes++;
	}

	return used_bytes;
}

enum lf12_error lf12_generate_volume_id(uint32_t * volume_id)
{
	struct timeval now;

	if (0 != gettimeofday(&now, NULL)) {
		lf12_save_errno();
		*volume_id = 0;

		return F12_UNKNOWN_ERROR;
	}

	/*
	 * Use the 16 least significant bits of the current microseconds since
	 * the epoch as the 16 most significant bits of the volume id and the
	 * 16 least significant bits of the current seconds since the epoch as
	 * the 16 least significant bites of the volume id.
	 * The id is guaranteed to be at least 1.
	 */
	*volume_id = ((uint32_t) now.tv_usec << 16) |
		((uint32_t) now.tv_sec & 0xffff) | 1;

	return F12_SUCCESS;
}

void lf12_free_metadata(struct lf12_metadata *f12_meta)
{
	free(f12_meta->bpb);
	free(f12_meta->fat_entries);
	if (f12_meta->root_dir) {
		lf12_free_entry(f12_meta->root_dir);
	}
	free(f12_meta->root_dir);
	free(f12_meta);
}

enum lf12_error lf12_create_metadata(struct lf12_metadata **f12_meta)
{
	*f12_meta = calloc(1, sizeof(struct lf12_metadata));
	if (NULL == *f12_meta) {
		return F12_ALLOCATION_ERROR;
	}

	(*f12_meta)->bpb = calloc(1, sizeof(struct bios_parameter_block));
	if (NULL == (*f12_meta)->bpb) {
		free(*f12_meta);

		return F12_ALLOCATION_ERROR;
	}

	return F12_SUCCESS;
}

void lf12_generate_entry_timestamp(long usecs, uint16_t * date, uint16_t * time,
				   uint8_t * msecs)
{
	char seconds, minutes, hours, day, month, year;
	time_t timer = usecs / 1000000;
	int milliseconds = (usecs / 1000) % 1000;
	struct tm *timeinfo = gmtime(&timer);

	if (timeinfo->tm_sec % 2) {
		milliseconds += 1000;
	}
	// Shrink to 5 bits and resolution of two seconds per bit
	seconds = (timeinfo->tm_sec / 2) & 0x1f;
	// 6 bits
	minutes = timeinfo->tm_min & 0x3f;
	// 5 bits
	hours = timeinfo->tm_hour & 0x1f;
	// 5 bits
	day = timeinfo->tm_mday & 0x1f;
	// 4 bits
	month = (timeinfo->tm_mon + 1) & 0xf;
	// Allign the year to 1980 and fit into 7 bits
	year = (timeinfo->tm_year - 80) & 0x7f;

	*date = 0 | ((uint16_t) day);
	*date |= ((uint16_t) month) << 5;
	*date |= ((uint16_t) year) << 9;

	*time = 0 | ((uint16_t) seconds);
	*time |= ((uint16_t) minutes) << 5;
	*time |= ((uint16_t) hours) << 11;

	*msecs = milliseconds / 10;
}

long lf12_read_entry_timestamp(uint16_t date, uint16_t time, uint8_t msecs)
{
	time_t timer = 0;
	struct tm timeinfo = { 0 };

	int h, m, s, gmtoff;

	h = (time >> 11) & 0x1f;
	m = (time >> 5) & 0x3f;
	s = (time & 0x1f) * 2;

	timeinfo.tm_year = (date >> 9) + 80;
	timeinfo.tm_mon = ((date >> 5) & 0xf) - 1;
	timeinfo.tm_mday = date & 0x1f;

	timeinfo.tm_hour = h;
	timeinfo.tm_min = m;
	timeinfo.tm_sec = s;

	/*
	 * Call mktime to complete the timeinfo structure, e.g. set the tm_isdst
	 * and __tm_gmtoff fields.
	 * This potentially modifies the time related fields of the structure,
	 * if the date falls in a daylight saving time (DST) period of the
	 * local time.
	 *
	 * That behavior is not desired. This function should return a timestamp
	 * with the exact time stored in the entry in local time.
	 */
	timer = mktime(&timeinfo);
	if (-1 == timer) {
		return -1;
	}

	/*
	 * Set the time related fields of the timeinfo structure again to avoid
	 * the mentioned problem and call mktime again to produce the desired
	 * timestamp.
	 */
	gmtoff = timeinfo.tm_gmtoff;
	timeinfo.tm_hour = h + gmtoff / 3600;
	timeinfo.tm_min = m + (gmtoff % 3600) / 60;
	timeinfo.tm_sec = s + gmtoff % 60;

	timer = mktime(&timeinfo);
	if (-1 == timer) {
		return -1;
	}

	return timer * 1000000 + msecs * 10000;
}
