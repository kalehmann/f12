/* Use the not posix conform function asprintf */
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>

#include "bpb.h"

static int ceil_div(int a, int b)
{
	return (a + b - 1) / b;
}

void info_dump_bpb(struct lf12_metadata *f12_meta, char **output)
{
	struct bios_parameter_block *bpb = f12_meta->bpb;

	char *text =
		"\n"
		"Bios Parameter Block\n"
		"  OEMLabel:\t\t\t%s\n"
		"  Sector size:\t\t\t%d bytes\n"
		"  Sectors per cluster:\t\t%d\n"
		"  Reserved sectors:\t\t%d\n"
		"  Number of fats:\t\t%d\n"
		"  Root directory entries:\t%d\n"
		"  Logical sectors:\t\t%d\n"
		"  Medium byte:\t\t\t0x%02x\n"
		"  Sectors per fat:\t\t%d\n"
		"  Sectors per track:\t\t%d\n"
		"  Number of heads:\t\t%d\n"
		"  Hidden sectors:\t\t%d\n"
		"  Logical sectors (large):\t%d\n"
		"  Drive number:\t\t\t0x%02x\n"
		"  Flags:\t\t\t0x%02x\n"
		"  Signature:\t\t\t0x%02x\n"
		"  VolumeID:\t\t\t0x%08x\n"
		"  Volume label:\t\t\t%s\n" "  File system:\t\t\t%s\n";

	asprintf(output, text, bpb->OEMLabel, bpb->SectorSize,
		 bpb->SectorsPerCluster,
		 bpb->ReservedForBoot, bpb->NumberOfFats,
		 bpb->RootDirEntries, bpb->LogicalSectors, bpb->MediumByte,
		 bpb->SectorsPerFat, bpb->SectorsPerTrack,
		 bpb->NumberOfHeads, bpb->HiddenSectors, bpb->LargeSectors,
		 bpb->DriveNumber, bpb->Flags, bpb->Signature, bpb->VolumeID,
		 bpb->VolumeLabel, bpb->FileSystem);
}

void
initialize_bpb(struct bios_parameter_block *bpb,
	       struct f12_create_arguments *args)
{
	size_t size;

	memcpy(&(bpb->OEMLabel), "f12     ", 8);
	if (0 == args->volume_size) {
		args->volume_size = 1440;
	}

	size = args->volume_size * 1024;

	/*
	 * In the following multiple values for the bios parameter block are
	 * guessed on the size of the disk.
	 *
	 * See <https://infogalactic.com/info/Design_of_the_FAT_file_system#media>
	 * for the medium bytes and their corresponding disk sizes.
	 *
	 * The number of root dir entries multiplied with 32 is always a multiple
	 * of the sector size.
	 */
	switch (args->volume_size) {
	case 2880:
		bpb->SectorSize = 512;
		bpb->SectorsPerCluster = 2;
		bpb->SectorsPerTrack = 36;
		bpb->NumberOfHeads = 2;
		bpb->RootDirEntries = 512;
		bpb->MediumByte = 0xf0;
		break;
	case 1440:
		bpb->SectorSize = 512;
		bpb->SectorsPerCluster = 1;
		bpb->SectorsPerTrack = 18;
		bpb->NumberOfHeads = 2;
		bpb->RootDirEntries = 224;
		bpb->MediumByte = 0xf0;
		break;
	case 1232:
		bpb->SectorSize = 1024;
		bpb->SectorsPerCluster = 1;
		bpb->SectorsPerTrack = 8;
		bpb->NumberOfHeads = 2;
		bpb->RootDirEntries = 224;
		bpb->MediumByte = 0xfe;
		break;
	case 1200:
		bpb->SectorSize = 512;
		bpb->SectorsPerCluster = 1;
		bpb->SectorsPerTrack = 15;
		bpb->NumberOfHeads = 2;
		bpb->RootDirEntries = 224;
		bpb->MediumByte = 0xf9;
		break;
	case 720:
		bpb->SectorSize = 512;
		bpb->SectorsPerCluster = 2;
		bpb->SectorsPerTrack = 9;
		bpb->NumberOfHeads = 2;
		bpb->RootDirEntries = 112;
		bpb->MediumByte = 0xf9;
		break;
	case 640:
		bpb->SectorSize = 512;
		bpb->SectorsPerCluster = 1;
		bpb->SectorsPerTrack = 8;
		bpb->NumberOfHeads = 2;
		bpb->RootDirEntries = 112;
		bpb->MediumByte = 0xfb;
	case 360:
		bpb->SectorSize = 512;
		bpb->SectorsPerCluster = 2;
		bpb->SectorsPerTrack = 9;
		bpb->NumberOfHeads = 2;
		bpb->RootDirEntries = 112;
		bpb->MediumByte = 0xfd;
		break;
	case 320:
		// MediumByte 0xfa with a single sided disk is also possible
		bpb->SectorSize = 2048;
		bpb->SectorsPerCluster = 1;
		bpb->SectorsPerTrack = 8;
		bpb->NumberOfHeads = 2;
		bpb->RootDirEntries = 48;
		bpb->MediumByte = 0xff;
		break;
	case 180:
		bpb->SectorSize = 512;
		bpb->SectorsPerCluster = 1;
		bpb->SectorsPerTrack = 9;
		bpb->NumberOfHeads = 1;
		bpb->RootDirEntries = 48;
		bpb->MediumByte = 0xfc;
		break;
	case 160:
		bpb->SectorSize = 512;
		bpb->SectorsPerCluster = 1;
		bpb->SectorsPerTrack = 8;
		bpb->NumberOfHeads = 1;
		bpb->RootDirEntries = 16;
		bpb->MediumByte = 0xfe;
		break;
	default:
		bpb->SectorSize = 512;
		bpb->SectorsPerCluster = 4;
		bpb->SectorsPerTrack = 63;
		bpb->NumberOfHeads = 255;
		bpb->RootDirEntries = 512;
		bpb->MediumByte = 0xf8;
		break;
	}

	if (args->sector_size && bpb->SectorSize != args->sector_size) {
		bpb->SectorSize = args->sector_size;
		bpb->MediumByte = 0xf8;
	}

	if (args->sectors_per_cluster) {
		bpb->SectorsPerCluster = args->sectors_per_cluster;
	}

	if (args->reserved_sectors) {
		bpb->ReservedForBoot = args->reserved_sectors;
	} else {
		bpb->ReservedForBoot = 1;
	}

	if (args->number_of_fats) {
		bpb->NumberOfFats = args->number_of_fats;
	} else {
		bpb->NumberOfFats = 2;
	}

	if (args->root_dir_entries) {
		bpb->RootDirEntries = args->root_dir_entries;
	}

	if (args->drive_number) {
		bpb->DriveNumber = args->drive_number;
	} else {
		bpb->DriveNumber = 0x80;
	}

	bpb->LogicalSectors = size / bpb->SectorSize;
	bpb->HiddenSectors = 0;
	bpb->LargeSectors = bpb->LogicalSectors;
	bpb->Flags = 0;
	bpb->Signature = 0;
	bpb->SectorsPerFat = sectors_per_fat(bpb);
	lf12_generate_volume_id(&(bpb->VolumeID));

	if (NULL != args->volume_label) {
		if (strlen(args->volume_label) < 12) {
			memcpy(&(bpb->VolumeLabel), args->volume_label,
			       strlen(args->volume_label));
			memset(&(bpb->VolumeLabel) +
			       strlen(args->volume_label), ' ',
			       11 - strlen(args->volume_label));
			bpb->VolumeLabel[11] = '\0';
		} else {
			memcpy(&(bpb->VolumeLabel), args->volume_label, 11);
			bpb->VolumeLabel[11] = '\0';
		}
	} else {
		memcpy(&(bpb->VolumeLabel), "NO NAME    ", 12);
	}

	memcpy(&(bpb->FileSystem), "FAT12    ", 9);
}

uint16_t sectors_per_fat(struct bios_parameter_block *bpb)
{
	// Layout of a fat12 formated partition:
	// Boot related stuff, File allocation tables, Root directory, data
	//
	// data_sectors = sectors - boot_sectors - root_sectors - fat_sectors
	//
	// The size of a file allocation table in sectors is
	// boot_sectors is known and root_sectors can be calculated.
	//
	// Calculating the sectors of the file allocation tables is tricky, as
	// the they depend on the clusters of the data sectors.
	//
	// The first step is to calculate the clusters with the file allocation
	// tables included. Note that two clusters are added for the fat id and
	// the end of chain marker.
	//
	// tmp_sectors = sectors - root_dir_sectors - boot_sectors
	// clusters_with_fat = tmp_sectors / sectors_per_cluster + 2
	//
	// The sectors for the file allocation tables can be calculated with
	//
	// cluster_size = sectors_per_cluster * sector_size
	// fat_sectors = 1.5 * clusters_with_fat / sector_size -
	//         1.5 * number_of_fats fat_sectors / cluster_size
	//
	// fat_sectors * (1.5 * number_of_fats / cluster_size + 1) =
	//         1.5 * clusters_with_fat / sector_size
	// fat_sectors = 1.5 * clusters_with_fat / sector_size /
	//         (1.5 * number_of_fats / cluster_size + 1)

	unsigned int sectors = bpb->LargeSectors;
	uint16_t sector_size = bpb->SectorSize;
	uint8_t sectors_per_cluster = bpb->SectorsPerCluster;
	uint16_t boot_sectors = bpb->ReservedForBoot;
	uint16_t root_dir_entries = bpb->RootDirEntries;
	uint8_t number_of_fats = bpb->NumberOfFats;
	uint16_t fat_sectors;
	size_t root_dir_size, cluster_size;
	unsigned int root_dir_sectors, tmp_sectors, clusters_with_fat;

	root_dir_size = root_dir_entries * 32;
	root_dir_sectors = ceil_div(root_dir_size, sector_size);

	tmp_sectors = sectors - root_dir_sectors - boot_sectors;
	clusters_with_fat = ceil_div(tmp_sectors, sectors_per_cluster) + 2;
	cluster_size = sectors_per_cluster * sector_size;

	fat_sectors = ceil_div(1.5 * clusters_with_fat,
			       sector_size * (1.5 * number_of_fats /
					      cluster_size + 1)
		);

	return fat_sectors;
}
