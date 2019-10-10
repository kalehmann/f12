/* Use the not posix conform function asprintf */
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>

#include "bpb.h"

void info_dump_bpb(struct f12_metadata *f12_meta, char **output)
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
	 * The number of root dir entries multiplied with 32 is always a multiple of the sector size.
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
	bpb->SectorsPerFat =
		sectorsPerFat(bpb->LogicalSectors, bpb->SectorSize,
			      bpb->SectorsPerCluster);
	bpb->HiddenSectors = 0;
	bpb->LargeSectors = bpb->LogicalSectors;
	bpb->Flags = 0;
	bpb->Signature = 0;
	f12_generate_volume_id(&(bpb->VolumeID));

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

uint16_t
sectorsPerFat(uint16_t sectors,
	      uint16_t sector_size, uint16_t sectors_per_cluster)
{
	unsigned int fat_size = (sectors / sectors_per_cluster) * 3 / 2;
	uint16_t fat_sectors = fat_size / sector_size;
	if (fat_size % sector_size) {
		fat_sectors++;
	}

	return fat_sectors;
}
