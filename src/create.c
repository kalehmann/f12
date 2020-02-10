#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "common.h"
#include "f12.h"
#include "libfat12/libfat12.h"
#include "../boot/default/bootcode.h"
#include "../boot/simple_bootloader/simple_bootloader.h"

static int ceil_div(int a, int b)
{
	return (a + b - 1) / b;
}

uint16_t _f12_sectors_per_fat(struct bios_parameter_block *bpb)
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

void
_f12_initialize_bpb(struct bios_parameter_block *bpb,
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
	bpb->SectorsPerFat = _f12_sectors_per_fat(bpb);
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

static enum lf12_error
install_simple_bootloader(FILE * fp,
			  struct lf12_metadata *f12_meta,
			  const char *boot_name, char **output)
{
	struct lf12_directory_entry *root_dir = f12_meta->root_dir;
	unsigned char file_exists = 0;
	char *boot_name_8_3 = lf12_convert_name(boot_name);
	if (NULL == boot_name_8_3) {
		return F12_ALLOCATION_ERROR;
	}

	for (int i = 0; i < root_dir->child_count; i++) {
		if (0 == memcmp(root_dir->children[i].ShortFileName,
				boot_name_8_3, 8) &&
		    0 == memcmp(root_dir->children[i].ShortFileExtension,
				boot_name_8_3 + 8, 3)) {
			file_exists = 1;
		}
	}

	if (!file_exists) {
		free(boot_name_8_3);

		return F12_FILE_NOT_FOUND;
	}

	sibolo_set_8_3_name(boot_name_8_3);
	free(boot_name_8_3);

	return lf12_install_bootloader(fp, f12_meta, boot_simple_bootloader);
}

int f12_create(struct f12_create_arguments *args, char **output)
{
	struct bios_parameter_block *bpb;
	suseconds_t created = time_usec();
	enum lf12_error err;
	FILE *fp = NULL;
	struct lf12_metadata *f12_meta = NULL;
	int ret;
	struct stat sb;

	fp = fopen(args->device_path, "w");
	ret = open_image(fp, NULL, output);
	if (ret != EXIT_SUCCESS) {
		return ret;
	}

	if (F12_SUCCESS != (err = lf12_create_metadata(&f12_meta))) {
		return print_error(fp, f12_meta, output,
				   _("Error while creating the metadata: %s\n"),
				   lf12_strerror(err));
	}

	bpb = f12_meta->bpb;
	_f12_initialize_bpb(bpb, args);
	f12_meta->root_dir_offset = bpb->SectorSize *
		((bpb->NumberOfFats * bpb->SectorsPerFat) +
		 bpb->ReservedForBoot);

	err = lf12_create_root_dir_meta(f12_meta);
	if (F12_SUCCESS != err) {
		return print_error(fp, f12_meta, output,
				   _("Error while creating root directory "
				     "metadata: %s\n"), lf12_strerror(err));
	}

	if (F12_SUCCESS != (err = lf12_create_image(fp, f12_meta))) {
		return print_error(fp, f12_meta, output,
				   _("Error while creating the image: %s\n"),
				   lf12_strerror(err));
	}

	if (args->boot_file == NULL) {
		err = lf12_install_bootloader(fp, f12_meta,
					      boot_default_bootcode_bin);
		if (F12_SUCCESS != err) {
			return print_error(fp, f12_meta, output,
					   _("Error while installing the "
					     "default bootcode: %s\n"),
					   lf12_strerror(err));
		}
	} else if (args->root_dir_path == NULL) {
		return print_error(fp, f12_meta, output,
				   _("Error: Can not use the boot-file option "
				     "without also specifying a root "
				     "directory\n"));
	}

	if (NULL == args->root_dir_path) {
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_SUCCESS;
	}

	if (0 != stat(args->root_dir_path, &sb)) {
		return print_error(fp, f12_meta, output,
				   _("Can not open the root directory %s\n"),
				   args->root_dir_path);
	}

	if (!S_ISDIR(sb.st_mode)) {
		return print_error(fp, f12_meta, output,
				   _("Expected the root dir to be a "
				     "directory\n"));
	}

	struct f12_put_arguments put_args = {
		.device_path = args->device_path,
		.source = args->root_dir_path,
		.destination = "",
		.verbose = args->verbose,
		.recursive = 1,
	};

	err = _f12_walk_dir(fp, &put_args, f12_meta, created, output);
	if (err) {
		return print_error(fp, f12_meta, output, "%s\n",
				   lf12_strerror(err));
	}

	if (args->boot_file) {
		if (strchr(args->boot_file, '/')) {
			return print_error(fp, f12_meta, output,
					   _("Error: The boot file can not be "
					     "in a subdirectory\n"));
		}

		err = install_simple_bootloader(fp, f12_meta, args->boot_file,
						output);
		switch (err) {
		case F12_SUCCESS:
			break;
		case F12_FILE_NOT_FOUND:
			return print_error(fp, f12_meta, output,
					   _("Error: Could not find %s in the "
					     "root directory\n"),
					   args->boot_file);
		default:
			return print_error(fp, f12_meta, output,
					   _
					   ("Error while installing the simple "
					    "bootloader: %s\n"),
					   lf12_strerror(err));
		}
	}

	err = lf12_write_metadata(fp, f12_meta);
	if (F12_SUCCESS != err) {
		return print_error(fp, f12_meta, output,
				   _("Error while writing the metadata to the "
				     "image: %s\n"), lf12_strerror(err));
	}
	lf12_free_metadata(f12_meta);
	fclose(fp);

	return EXIT_SUCCESS;
}
