 /* Use the not posix conform function asprintf */
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fts.h>
#include <libgen.h>

#include "f12.h"
#include "libfat12/libfat12.h"

char *format_bytes(size_t bytes)
{
	char *out;

	if (bytes < 10000) {
		asprintf(&out, "%ld bytes", bytes);
	} else if (bytes < 10000000) {
		asprintf(&out, "%ld KiB", bytes / 1024);
	} else if (bytes < 10000000000) {
		asprintf(&out, "%ld MiB", bytes / (1024 * 1024));
	} else {
		asprintf(&out, "%ld GiB", bytes / (1024 * 1024 * 1024));
	}

	return out;
}

static enum f12_error
recursive_del_entry(FILE * fp,
		    struct f12_metadata *f12_meta,
		    struct f12_directory_entry *entry,
		    struct f12_del_arguments *args, char **output)
{
	struct f12_directory_entry *child;
	enum f12_error err;
	char *entry_path, *temp;
	int verbose = args->verbose;

	if (f12_is_directory(entry) && f12_get_child_count(entry) > 2) {
		for (int i = 0; i < entry->child_count; i++) {
			child = &entry->children[i];
			if (f12_entry_is_empty(child) || f12_is_dot_dir(child)) {
				continue;
			}

			err = recursive_del_entry(fp, f12_meta, child, args,
						  output);
			if (F12_SUCCESS != err) {
				return err;
			}
		}
	} else if (verbose) {
		err = f12_get_entry_path(entry, &entry_path);
		if (F12_SUCCESS != err) {
			return err;
		}
		temp = *output;
		if (NULL != *output) {
			asprintf(output, "%s%s\n", *output, entry_path);

		} else {
			asprintf(output, "%s\n", entry_path);
		}
		free(temp);
		free(entry_path);
	}

	return f12_del_entry(fp, f12_meta, entry, args->soft_delete);
}

static enum f12_error
dump_move(struct f12_directory_entry *src,
	  struct f12_directory_entry *dest, char **output)
{
	enum f12_error err;
	char *tmp = NULL, *file_name = NULL, *dest_path = NULL, *src_path =
		NULL;
	struct f12_directory_entry *tmp_entry = src, *child = NULL;
	int i = 0;
	size_t src_offset = 0;

	err = f12_get_entry_path(dest, &dest_path);
	if (F12_SUCCESS != err) {
		return err;
	}

	if (!f12_is_directory(tmp_entry)) {
		err = f12_get_entry_path(src, &src_path);
		if (err != F12_SUCCESS) {
			free(dest_path);

			return err;
		}
		file_name = f12_get_file_name(src);
		if (*output) {
			tmp = *output;
			asprintf(output, "%s%s -> %s/%s\n", *output,
				 src_path, dest_path, file_name);
			free(tmp);
		} else {
			asprintf(output, "%s -> %s/%s\n", src_path, dest_path,
				 file_name);
		}
		free(file_name);
		free(src_path);
		free(dest_path);

		return F12_SUCCESS;
	}

	err = f12_get_entry_path(src, &tmp);
	if (err != F12_SUCCESS) {
		free(dest_path);

		return err;
	}
	src_offset = strlen(tmp);
	free(tmp);

	do {
		if (i >= tmp_entry->child_count) {
			i = 0;
			while (&tmp_entry->parent->children[i] != tmp_entry) {
				i++;
			}
			i++;
			tmp_entry = tmp_entry->parent;
		}

		child = &tmp_entry->children[i];

		if (f12_is_dot_dir(child) || f12_entry_is_empty(child)) {
			i++;
			continue;
		}
		err = f12_get_entry_path(child, &src_path);
		if (err != F12_SUCCESS) {
			free(dest_path);

			return err;
		}
		tmp = *output;
		if (tmp) {
			asprintf(output, "%s%s", *output, src_path);
			free(tmp);
		} else {
			asprintf(output, "%s", src_path);
		}

		tmp = *output;
		file_name = f12_get_file_name(src);
		asprintf(output, "%s -> %s/%s%s\n", *output,
			 dest_path, file_name, src_path + src_offset);
		free(file_name);
		free(tmp);
		free(src_path);

		if (f12_is_directory(child)
		    && f12_get_child_count(child) > 2 && !f12_is_dot_dir(child)) {
			tmp_entry = child;
			i = 0;

			continue;
		}

		i++;
	}
	while (i < tmp_entry->child_count || tmp_entry != src);

	free(dest_path);

	return F12_SUCCESS;
}

static int
dump_f12_structure(FILE * fp,
		   struct f12_metadata *f12_meta,
		   struct f12_directory_entry *entry,
		   char *dest_path,
		   struct f12_get_arguments *args, char **output)
{
	int verbose = args->verbose, recursive = args->recursive;
	enum f12_error err;
	struct f12_directory_entry *child_entry;
	char *entry_path, *temp;
	int res;

	if (!f12_is_directory(entry)) {
		FILE *dest_fp = fopen(dest_path, "w");
		err = f12_dump_file(fp, f12_meta, entry, dest_fp);
		if (F12_SUCCESS != err) {
			temp = *output;
			asprintf(output, "%s\n", f12_strerror(err));
			free(temp);
			fclose(dest_fp);
			return -1;
		}
		fclose(dest_fp);

		return 0;
	}

	if (!recursive) {
		temp = *output;
		asprintf(output, "%s\n", f12_strerror(F12_IS_DIR));
		free(temp);

		return -1;
	}

	if (0 != mkdir(dest_path, 0777)) {
		if (errno != EEXIST) {
			return -1;
		}
	}

	for (int i = 0; i < entry->child_count; i++) {
		child_entry = &entry->children[i];

		if (f12_entry_is_empty(child_entry)
		    || f12_is_dot_dir(child_entry)) {
			continue;
		}

		char *child_name = f12_get_file_name(child_entry);

		if (NULL == child_name) {
			return -1;
		}

		asprintf(&entry_path, "%s/%s", dest_path, child_name);

		if (verbose) {
			temp = *output;
			if (NULL != *output) {
				asprintf(output, "%s%s\n", *output, entry_path);

			} else {
				asprintf(output, "%s\n", entry_path);
			}
			free(temp);
		}

		free(child_name);
		res = dump_f12_structure(fp, f12_meta, child_entry,
					 entry_path, args, output);
		free(entry_path);

		if (res) {
			return res;
		}
	}

	return 0;
}

static void info_dump_bpb(struct f12_metadata *f12_meta, char **output)
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

static uint16_t
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

static void
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
	bpb->VolumeID = 0;

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

static int
list_f12_entry(struct f12_directory_entry *entry,
	       char **output, struct f12_list_arguments *args, int depth)
{
	if (f12_entry_is_empty(entry)) {
		return 0;
	}

	char *name = f12_get_file_name(entry);

	if (NULL == name) {
		return -1;
	}

	char *temp = *output;
	asprintf(output, "%s" "%*s|-> %s\n", temp, depth, "", name);
	free(temp);
	free(name);

	if (args->recursive && f12_is_directory(entry)
	    && !f12_is_dot_dir(entry)) {
		for (int i = 0; i < entry->child_count; i++) {
			list_f12_entry(&entry->children[i], output, args,
				       depth + 2);
		}
	}

	return 0;
}

static void
list_root_dir_entries(struct f12_metadata *f12_meta,
		      struct f12_list_arguments *args, char **output)
{
	for (int i = 0; i < f12_meta->root_dir->child_count; i++) {
		list_f12_entry(&f12_meta->root_dir->children[i], output, args,
			       0);
	}
}

static int
walk_dir(FILE * fp,
	 struct f12_put_arguments *args,
	 struct f12_metadata *f12_meta, char **output)
{
	enum f12_error err;
	char *path = args->source;
	char *dest = args->destination;
	char *temp;
	char *src_path;
	char putpath[1024];
	char *const paths[] = { path, NULL };

	FTS *ftsp = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
	FTSENT *ent;

	if (ftsp == NULL) {
		asprintf(output, "fts_open error: %s\n", strerror(errno));
		return -1;
	}

	while (1) {
		ent = fts_read(ftsp);
		if (ent == NULL) {
			if (errno == 0)
				// No more items, leave
				break;
			else {
				asprintf(output, "fts_read error: %s\n",
					 strerror(errno));

				return -1;
			}
		}

		if (ent->fts_info & FTS_F) {
			FILE *src;
			src_path = ent->fts_path + strlen(path);
			memcpy(putpath, dest, strlen(dest));
			memcpy(putpath + strlen(dest), src_path,
			       strlen(src_path) + 1);

			if (args->verbose) {
				temp = *output;
				asprintf(output, "%s%s -> %s\n", *output,
					 src_path, putpath);
				free(temp);
			}

			if (NULL == (src = fopen(ent->fts_path, "r"))) {
				temp = *output;
				asprintf(output,
					 "%s\nCannot open source file %s\n",
					 *output, ent->fts_path);
				free(temp);

				return -1;
			}
			struct f12_path *dest;

			err = f12_parse_path(putpath, &dest);
			if (F12_SUCCESS != err) {
				temp = *output;
				if (NULL != *output) {
					asprintf(output, "%s\n%s\n", *output,
						 f12_strerror(err));
				} else {
					asprintf(output, "%s\n",
						 f12_strerror(err));
				}
				free(temp);

				return -1;
			}

			err = f12_create_file(fp, f12_meta, dest, src);
			f12_free_path(dest);
			if (F12_SUCCESS != err) {
				temp = *output;
				asprintf(output, "%s\nError : %s\n", *output,
					 f12_strerror(err));
				free(temp);

				return -1;
			}
		}
	}

	if (fts_close(ftsp) == -1) {
		temp = *output;
		asprintf(output, "%s\nfts_close error: %s\n", *output,
			 strerror(errno));
		free(temp);

		return -1;
	}

	return 0;
}

int f12_create(struct f12_create_arguments *args, char **output)
{
	FILE *fp;
	enum f12_error err;
	struct bios_parameter_block *bpb;
	struct f12_metadata *f12_meta;
	struct stat sb;

	*output = malloc(1);
	(*output)[0] = '\0';

	if (NULL == (fp = fopen(args->device_path, "w"))) {
		free(*output);
		asprintf(output, "Error opening image: %s\n", strerror(errno));

		return EXIT_FAILURE;
	}

	if (F12_SUCCESS != (err = f12_create_metadata(&f12_meta))) {
		free(*output);
		fclose(fp);

		return err;
	}

	bpb = f12_meta->bpb;
	initialize_bpb(bpb, args);
	f12_meta->root_dir_offset = bpb->SectorSize *
		((bpb->NumberOfFats * bpb->SectorsPerFat) +
		 bpb->ReservedForBoot);

	err = f12_create_root_dir_meta(f12_meta);
	if (F12_SUCCESS != err) {
		f12_free_metadata(f12_meta);
		free(*output);

		return err;
	}

	if (F12_SUCCESS != (err = f12_create_image(fp, f12_meta))) {
		fclose(fp);
		f12_free_metadata(f12_meta);
		free(*output);

		return err;
	}

	if (NULL == args->root_dir_path) {
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_SUCCESS;
	}

	if (0 != stat(args->root_dir_path, &sb)) {
		free(*output);
		asprintf(output, "Can not open source file\n");
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	if (!S_ISDIR(sb.st_mode)) {
		free(*output);
		asprintf(output, "Expected the root dir to be a directory\n");
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	struct f12_put_arguments put_args = {
		.device_path = args->device_path,
		.source = args->root_dir_path,
		.destination = "",
		.verbose = 1,
		.recursive = 1,
	};

	err = walk_dir(fp, &put_args, f12_meta, output);
	if (err) {
		free(*output);
		asprintf(output, "%s\n", f12_strerror(err));
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	err = f12_write_metadata(fp, f12_meta);
	f12_free_metadata(f12_meta);
	fclose(fp);

	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "Error: %s\n", f12_strerror(err));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int f12_del(struct f12_del_arguments *args, char **output)
{
	FILE *fp;
	enum f12_error err;
	struct f12_metadata *f12_meta;

	if (NULL == (fp = fopen(args->device_path, "r+"))) {
		asprintf(output, "Error opening image: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	if (F12_SUCCESS != (err = f12_read_metadata(fp, &f12_meta))) {
		fclose(fp);
		asprintf(output, "Error loading image: %s\n",
			 f12_strerror(err));

		return EXIT_FAILURE;
	}

	struct f12_path *path;

	err = f12_parse_path(args->path, &path);
	if (F12_SUCCESS != err) {
		asprintf(output, "%s\n", f12_strerror(err));
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	struct f12_directory_entry *entry =
		f12_entry_from_path(f12_meta->root_dir, path);
	f12_free_path(path);

	if (NULL == entry) {
		asprintf(output, "The file %s was not found on the device\n",
			 args->path);
		f12_free_metadata(f12_meta);
		fclose(fp);
		return EXIT_FAILURE;
	}

	if (f12_is_directory(entry) && entry->child_count > 2
	    && !args->recursive) {
		asprintf(output, "Error: %s\n", f12_strerror(F12_IS_DIR));
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	err = recursive_del_entry(fp, f12_meta, entry, args, output);

	if (err != F12_SUCCESS) {
		asprintf(output, "Error: %s\n", f12_strerror(err));
		f12_free_path(path);
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	f12_free_metadata(f12_meta);
	fclose(fp);

	return EXIT_SUCCESS;
}

int f12_get(struct f12_get_arguments *args, char **output)
{
	enum f12_error err;
	FILE *fp;
	int res;

	struct f12_metadata *f12_meta;

	if (NULL == (fp = fopen(args->device_path, "r+"))) {
		asprintf(output, "Error opening image: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	if (F12_SUCCESS != (err = f12_read_metadata(fp, &f12_meta))) {
		fclose(fp);
		asprintf(output, "Error loading image: %s\n",
			 f12_strerror(err));
		return EXIT_FAILURE;
	}

	struct f12_path *src_path;

	err = f12_parse_path(args->path, &src_path);
	if (F12_SUCCESS != err) {
		asprintf(output, "%s\n", f12_strerror(err));
		fclose(fp);

		return EXIT_FAILURE;
	}

	struct f12_directory_entry *entry =
		f12_entry_from_path(f12_meta->root_dir,
				    src_path);
	if (NULL == entry) {
		asprintf(output, "The file %s was not found on the device\n",
			 args->path);
		f12_free_path(src_path);
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	res = dump_f12_structure(fp, f12_meta, entry, args->dest, args, output);

	f12_free_path(src_path);
	f12_free_metadata(f12_meta);
	fclose(fp);

	if (res) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int f12_info(struct f12_info_arguments *args, char **output)
{
	enum f12_error err;
	FILE *fp;
	char *formatted_size, *formatted_used_bytes;
	char *device_path = args->device_path;
	struct f12_metadata *f12_meta;

	if (NULL == (fp = fopen(device_path, "r"))) {
		asprintf(output, "Error opening image: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	err = f12_read_metadata(fp, &f12_meta);
	fclose(fp);
	if (F12_SUCCESS != err) {
		asprintf(output, "Error loading image: %s\n",
			 f12_strerror(err));
		return EXIT_FAILURE;
	}

	formatted_size = format_bytes(f12_get_partition_size(f12_meta));
	formatted_used_bytes = format_bytes(f12_get_used_bytes(f12_meta));

	asprintf(output,
		 "F12 info\n"
		 "  Partition size:\t\t%s\n"
		 "  Used bytes:\t\t\t%s\n"
		 "  Files:\t\t\t%d\n"
		 "  Directories:\t\t\t%d\n",
		 formatted_size,
		 formatted_used_bytes,
		 f12_get_file_count(f12_meta->root_dir),
		 f12_get_directory_count(f12_meta->root_dir));

	free(formatted_size);
	free(formatted_used_bytes);

	if (args->dump_bpb) {
		char *bpb, *tmp;
		info_dump_bpb(f12_meta, &bpb);
		asprintf(&tmp, "%s%s", *output, bpb);
		free(bpb);
		free(*output);
		*output = tmp;
	}

	f12_free_metadata(f12_meta);
	return EXIT_SUCCESS;
}

int f12_list(struct f12_list_arguments *args, char **output)
{
	enum f12_error err;
	FILE *fp;
	char *device_path = args->device_path;
	struct f12_metadata *f12_meta;

	*output = malloc(1);

	if (NULL == *output) {
		free(*output);
		asprintf(output, "Allocation error: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	(*output)[0] = 0;

	if (NULL == (fp = fopen(device_path, "r"))) {
		free(*output);
		asprintf(output, "Error opening image: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	err = f12_read_metadata(fp, &f12_meta);
	fclose(fp);
	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "Error loading image: %s\n",
			 f12_strerror(err));
		return EXIT_FAILURE;
	}

	if (args->path != 0 && args->path[0] != '\0') {
		struct f12_path *path;
		err = f12_parse_path(args->path, &path);
		if (F12_EMPTY_PATH == err) {
			list_root_dir_entries(f12_meta, args, output);
			f12_free_metadata(f12_meta);

			return EXIT_SUCCESS;
		}
		if (F12_SUCCESS != err) {
			asprintf(output, "%s\n", f12_strerror(err));
			f12_free_metadata(f12_meta);

			return EXIT_FAILURE;
		}

		struct f12_directory_entry *entry =
			f12_entry_from_path(f12_meta->root_dir, path);
		f12_free_path(path);

		if (NULL == entry) {
			free(*output);
			asprintf(output, "File not found\n");
			f12_free_metadata(f12_meta);

			return EXIT_FAILURE;
		}

		if (f12_is_directory(entry)) {
			for (int i = 0; i < entry->child_count; i++) {
				list_f12_entry(&entry->children[i], output,
					       args, 0);
			}
			f12_free_metadata(f12_meta);

			return EXIT_SUCCESS;
		}

		list_f12_entry(entry, output, args, 0);
		f12_free_metadata(f12_meta);

		return EXIT_SUCCESS;
	}

	list_root_dir_entries(f12_meta, args, output);

	f12_free_metadata(f12_meta);

	return EXIT_SUCCESS;
}

int f12_move(struct f12_move_arguments *args, char **output)
{
	FILE *fp;
	enum f12_error err;
	char *device_path = args->device_path;
	struct f12_metadata *f12_meta;
	struct f12_path *src, *dest;
	struct f12_directory_entry *src_entry, *dest_entry;

	*output = malloc(1);

	if (NULL == *output) {
		asprintf(output, "Allocation error: %s\n", strerror(errno));

		return EXIT_FAILURE;
	}

	(*output)[0] = 0;

	if (NULL == (fp = fopen(device_path, "r+"))) {
		free(*output);
		asprintf(output, "Error opening image: %s\n", strerror(errno));

		return EXIT_FAILURE;
	}

	if (F12_SUCCESS != (err = f12_read_metadata(fp, &f12_meta))) {
		fclose(fp);
		free(*output);
		asprintf(output, "Error loading image: %s\n",
			 f12_strerror(err));

		return EXIT_FAILURE;
	}

	err = f12_parse_path(args->source, &src);
	if (F12_EMPTY_PATH == err) {
		free(*output);
		asprintf(output, "Can not move the root directory\n");
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}
	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "%s\n", f12_strerror(err));
		f12_free_path(src);
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	err = f12_parse_path(args->destination, &dest);

	if (F12_SUCCESS != err && F12_EMPTY_PATH != err) {
		free(*output);
		asprintf(output, "%s\n", f12_strerror(err));
		f12_free_path(src);
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	switch (f12_path_get_parent(src, dest)) {
	case F12_PATHS_FIRST:
		free(*output);
		asprintf(output, "Can not move the directory into a child\n");
		f12_free_path(src);
		f12_free_path(dest);
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	case F12_PATHS_EQUAL:
		f12_free_path(src);
		f12_free_path(dest);
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_SUCCESS;
	default:
		break;
	}

	src_entry = f12_entry_from_path(f12_meta->root_dir, src);
	if (NULL == src_entry) {
		free(*output);
		asprintf(output, "File or directory %s not found\n",
			 args->source);
		f12_free_path(src);
		f12_free_path(dest);
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}
	f12_free_path(src);

	if (0 == args->recursive && f12_is_directory(src_entry)
	    && src_entry->child_count > 2) {
		free(*output);
		asprintf(output, "%s\n", f12_strerror(F12_IS_DIR));
		f12_free_path(dest);
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	dest_entry = f12_entry_from_path(f12_meta->root_dir, dest);
	if (NULL == dest_entry) {
		free(*output);
		asprintf(output, "File or directory %s not found\n",
			 args->destination);
		f12_free_path(dest);
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}
	f12_free_path(dest);

	if (args->verbose) {
		err = dump_move(src_entry, dest_entry, output);
		if (err != F12_SUCCESS) {
			free(*output);
			asprintf(output, "Error: %s\n", f12_strerror(err));
			f12_free_metadata(f12_meta);
			fclose(fp);

			return EXIT_FAILURE;
		}
	}
	if (F12_SUCCESS != (err = f12_move_entry(src_entry, dest_entry))) {
		free(*output);
		asprintf(output, "Error: %s\n", f12_strerror(err));
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "Error: %s\n", f12_strerror(err));
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	err = f12_write_metadata(fp, f12_meta);
	f12_free_metadata(f12_meta);
	fclose(fp);

	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "Error: %s\n", f12_strerror(err));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int f12_put(struct f12_put_arguments *args, char **output)
{
	enum f12_error err;
	FILE *fp, *src;
	int res;
	char *device_path = args->device_path;
	struct f12_metadata *f12_meta;
	struct f12_path *dest;

	*output = malloc(1);

	if (NULL == *output) {
		free(*output);
		asprintf(output, "Allocation error: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	(*output)[0] = 0;

	if (NULL == (fp = fopen(device_path, "r+"))) {
		free(*output);
		asprintf(output, "Error opening image: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	if (F12_SUCCESS != (err = f12_read_metadata(fp, &f12_meta))) {
		fclose(fp);
		free(*output);
		asprintf(output, "Error loading image: %s\n",
			 f12_strerror(err));
		return EXIT_FAILURE;
	}

	struct stat sb;

	if (0 != stat(args->source, &sb)) {
		free(*output);
		asprintf(output, "Can not open source file\n");
		f12_free_metadata(f12_meta);

		return EXIT_FAILURE;
	}

	err = f12_parse_path(args->destination, &dest);
	if (F12_EMPTY_PATH == err) {
		free(*output);
		asprintf(output, "Can not replace root directory\n");
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}
	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "%s\n", f12_strerror(err));
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	if (S_ISDIR(sb.st_mode)) {
		if (!args->recursive) {
			free(*output);
			asprintf(output, "%s\n", f12_strerror(F12_IS_DIR));
			f12_free_path(dest);
			f12_free_metadata(f12_meta);
			fclose(fp);

			return EXIT_FAILURE;
		}
		res = walk_dir(fp, args, f12_meta, output);
		if (res) {
			free(*output);
			asprintf(output, "%s\n", f12_strerror(err));
			f12_free_path(dest);
			f12_free_metadata(f12_meta);
			fclose(fp);

			return EXIT_FAILURE;
		}
	} else if (S_ISREG(sb.st_mode)) {
		if (NULL == (src = fopen(args->source, "r"))) {
			free(*output);
			asprintf(output, "Can not open source file\n");
			f12_free_path(dest);
			f12_free_metadata(f12_meta);

			return EXIT_FAILURE;
		}

		err = f12_create_file(fp, f12_meta, dest, src);
		if (F12_SUCCESS != err) {
			free(*output);
			asprintf(output, "%s\n", f12_strerror(err));
			f12_free_path(dest);
			f12_free_metadata(f12_meta);
			fclose(fp);

			return EXIT_FAILURE;
		}
		if (args->verbose) {
			free(*output);
			asprintf(output, "%s -> %s\n", args->source,
				 args->destination);
		}
	} else {
		free(*output);
		asprintf(output, "Source file has unsupported type\n");
		f12_free_path(dest);
		f12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_SUCCESS;
	}

	f12_free_path(dest);
	err = f12_write_metadata(fp, f12_meta);
	f12_free_metadata(f12_meta);
	fclose(fp);
	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "Error: %s\n", f12_strerror(err));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
