/* Use the not posix conform function asprintf */
#define _GNU_SOURCE

#include <errno.h>
#include <fts.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "bpb.h"
#include "f12.h"
#include "filesystem.h"
#include "format.h"
#include "list.h"
#include "libfat12/libfat12.h"
#include "../boot/default/bootcode.h"
#include "../boot/simple_bootloader/simple_bootloader.h"

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
		return F12_FILE_NOT_FOUND;
	}

	sibolo_set_8_3_name(boot_name_8_3);
	free(boot_name_8_3);

	return lf12_install_bootloader(fp, f12_meta, boot_simple_bootloader);
}

static enum lf12_error
recursive_del_entry(FILE * fp,
		    struct lf12_metadata *f12_meta,
		    struct lf12_directory_entry *entry,
		    struct f12_del_arguments *args, char **output)
{
	struct lf12_directory_entry *child;
	enum lf12_error err;
	char *entry_path, *temp;
	int verbose = args->verbose;

	if (lf12_is_directory(entry) && lf12_get_child_count(entry) > 2) {
		for (int i = 0; i < entry->child_count; i++) {
			child = &entry->children[i];
			if (lf12_entry_is_empty(child)
			    || lf12_is_dot_dir(child)) {
				continue;
			}

			err = recursive_del_entry(fp, f12_meta, child, args,
						  output);
			if (F12_SUCCESS != err) {
				return err;
			}
		}
	} else if (verbose) {
		err = lf12_get_entry_path(entry, &entry_path);
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

	return lf12_del_entry(fp, f12_meta, entry, args->soft_delete);
}

static suseconds_t time_usec(void)
{
	struct timeval tv = { 0 };
	time_t timer = 0;

	gettimeofday(&tv, NULL);
	timer = time(NULL);

	return timer * 1000000 + tv.tv_usec;
}

int f12_create(struct f12_create_arguments *args, char **output)
{
	FILE *fp;
	enum lf12_error err;
	struct bios_parameter_block *bpb;
	struct lf12_metadata *f12_meta;
	struct stat sb;
	suseconds_t created = time_usec();

	*output = malloc(1);
	(*output)[0] = '\0';

	if (NULL == (fp = fopen(args->device_path, "w"))) {
		free(*output);
		asprintf(output, "Error opening image: %s\n", strerror(errno));

		return EXIT_FAILURE;
	}

	if (F12_SUCCESS != (err = lf12_create_metadata(&f12_meta))) {
		free(*output);
		fclose(fp);

		return err;
	}

	bpb = f12_meta->bpb;
	_f12_initialize_bpb(bpb, args);
	f12_meta->root_dir_offset = bpb->SectorSize *
		((bpb->NumberOfFats * bpb->SectorsPerFat) +
		 bpb->ReservedForBoot);

	err = lf12_create_root_dir_meta(f12_meta);
	if (F12_SUCCESS != err) {
		lf12_free_metadata(f12_meta);
		free(*output);

		return err;
	}

	if (F12_SUCCESS != (err = lf12_create_image(fp, f12_meta))) {
		fclose(fp);
		lf12_free_metadata(f12_meta);
		free(*output);

		return err;
	}

	if (args->boot_file == NULL) {
		err = lf12_install_bootloader(fp, f12_meta,
					      boot_default_bootcode_bin);
		if (F12_SUCCESS != err) {
			free(*output);
			asprintf(output, "Error: %s\n", lf12_strerror(err));
			lf12_free_metadata(f12_meta);
			fclose(fp);

			return EXIT_FAILURE;
		}
	} else if (args->root_dir_path == NULL) {
		free(*output);
		lf12_free_metadata(f12_meta);
		fclose(fp);
		asprintf(output, "Error: Can not use the boot-file option "
			 "without also specifying a root directory\n");

		return EXIT_FAILURE;
	}

	if (NULL == args->root_dir_path) {
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_SUCCESS;
	}

	if (0 != stat(args->root_dir_path, &sb)) {
		free(*output);
		asprintf(output, "Can not open source file\n");
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	if (!S_ISDIR(sb.st_mode)) {
		free(*output);
		asprintf(output, "Expected the root dir to be a directory\n");
		lf12_free_metadata(f12_meta);
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

	err = _f12_walk_dir(fp, &put_args, f12_meta, created, output);
	if (err) {
		free(*output);
		asprintf(output, "%s\n", lf12_strerror(err));
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	if (args->boot_file) {
		err = install_simple_bootloader(fp, f12_meta, args->boot_file,
						output);
		if (F12_SUCCESS != err) {
			lf12_free_metadata(f12_meta);
			fclose(fp);
			free(*output);

			if (F12_FILE_NOT_FOUND == err) {
				asprintf(output,
					 "Error: Could not find %s in the root"
					 " directory\n", args->boot_file);
			} else {
				asprintf(output, "Error: %s\n",
					 lf12_strerror(err));
			}

			return EXIT_FAILURE;
		}
	}

	err = lf12_write_metadata(fp, f12_meta);
	lf12_free_metadata(f12_meta);
	fclose(fp);
	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "Error: %s\n", lf12_strerror(err));

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int f12_del(struct f12_del_arguments *args, char **output)
{
	FILE *fp;
	enum lf12_error err;
	struct lf12_metadata *f12_meta;

	if (NULL == (fp = fopen(args->device_path, "r+"))) {
		asprintf(output, "Error opening image: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	if (F12_SUCCESS != (err = lf12_read_metadata(fp, &f12_meta))) {
		fclose(fp);
		asprintf(output, "Error loading image: %s\n",
			 lf12_strerror(err));

		return EXIT_FAILURE;
	}

	struct lf12_path *path;

	err = lf12_parse_path(args->path, &path);
	if (F12_SUCCESS != err) {
		asprintf(output, "%s\n", lf12_strerror(err));
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	struct lf12_directory_entry *entry =
		lf12_entry_from_path(f12_meta->root_dir, path);
	lf12_free_path(path);

	if (NULL == entry) {
		asprintf(output, "The file %s was not found on the device\n",
			 args->path);
		lf12_free_metadata(f12_meta);
		fclose(fp);
		return EXIT_FAILURE;
	}

	if (lf12_is_directory(entry) && entry->child_count > 2
	    && !args->recursive) {
		asprintf(output, "Error: %s\n", lf12_strerror(F12_IS_DIR));
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	err = recursive_del_entry(fp, f12_meta, entry, args, output);

	if (err != F12_SUCCESS) {
		asprintf(output, "Error: %s\n", lf12_strerror(err));
		lf12_free_path(path);
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	lf12_free_metadata(f12_meta);
	fclose(fp);

	return EXIT_SUCCESS;
}

int f12_get(struct f12_get_arguments *args, char **output)
{
	enum lf12_error err;
	FILE *fp;
	int res;

	struct lf12_metadata *f12_meta;

	if (NULL == (fp = fopen(args->device_path, "r+"))) {
		asprintf(output, "Error opening image: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	if (F12_SUCCESS != (err = lf12_read_metadata(fp, &f12_meta))) {
		fclose(fp);
		asprintf(output, "Error loading image: %s\n",
			 lf12_strerror(err));
		return EXIT_FAILURE;
	}

	struct lf12_path *src_path;

	err = lf12_parse_path(args->path, &src_path);
	if (F12_SUCCESS != err) {
		asprintf(output, "%s\n", lf12_strerror(err));
		fclose(fp);

		return EXIT_FAILURE;
	}

	struct lf12_directory_entry *entry =
		lf12_entry_from_path(f12_meta->root_dir,
				     src_path);
	if (NULL == entry) {
		asprintf(output, "The file %s was not found on the device\n",
			 args->path);
		lf12_free_path(src_path);
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	res = _f12_dump_f12_structure(fp, f12_meta, entry, args->dest, args,
				      output);

	lf12_free_path(src_path);
	lf12_free_metadata(f12_meta);
	fclose(fp);

	if (res) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int f12_info(struct f12_info_arguments *args, char **output)
{
	enum lf12_error err;
	FILE *fp;
	char *formatted_size, *formatted_used_bytes;
	char *device_path = args->device_path;
	struct lf12_metadata *f12_meta;

	if (NULL == (fp = fopen(device_path, "r"))) {
		asprintf(output, "Error opening image: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	err = lf12_read_metadata(fp, &f12_meta);
	fclose(fp);
	if (F12_SUCCESS != err) {
		asprintf(output, "Error loading image: %s\n",
			 lf12_strerror(err));
		return EXIT_FAILURE;
	}

	formatted_size = _f12_format_bytes(lf12_get_partition_size(f12_meta));
	formatted_used_bytes = _f12_format_bytes(lf12_get_used_bytes(f12_meta));

	asprintf(output,
		 "F12 info\n"
		 "  Partition size:\t\t%s\n"
		 "  Used bytes:\t\t\t%s\n"
		 "  Files:\t\t\t%d\n"
		 "  Directories:\t\t\t%d\n",
		 formatted_size,
		 formatted_used_bytes,
		 lf12_get_file_count(f12_meta->root_dir),
		 lf12_get_directory_count(f12_meta->root_dir));

	free(formatted_size);
	free(formatted_used_bytes);

	if (args->dump_bpb) {
		char *bpb, *tmp;
		_f12_info_dump_bpb(f12_meta, &bpb);
		asprintf(&tmp, "%s%s", *output, bpb);
		free(bpb);
		free(*output);
		*output = tmp;
	}

	lf12_free_metadata(f12_meta);
	return EXIT_SUCCESS;
}

int f12_list(struct f12_list_arguments *args, char **output)
{
	enum lf12_error err;
	FILE *fp;
	char *device_path = args->device_path;
	struct lf12_metadata *f12_meta;

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

	err = lf12_read_metadata(fp, &f12_meta);
	fclose(fp);
	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "Error loading image: %s\n",
			 lf12_strerror(err));
		return EXIT_FAILURE;
	}

	if (args->path != 0 && args->path[0] != '\0') {
		struct lf12_path *path;
		err = lf12_parse_path(args->path, &path);
		if (F12_EMPTY_PATH == err) {
			err = _f12_list_entry(f12_meta->root_dir, output, args);
			lf12_free_metadata(f12_meta);
			if (F12_SUCCESS != err) {
				free(*output);
				asprintf(output, "%s\n", lf12_strerror(err));

				return EXIT_FAILURE;
			}

			return EXIT_SUCCESS;
		}
		if (F12_SUCCESS != err) {
			free(*output);
			asprintf(output, "%s\n", lf12_strerror(err));
			lf12_free_metadata(f12_meta);

			return EXIT_FAILURE;
		}

		struct lf12_directory_entry *entry =
			lf12_entry_from_path(f12_meta->root_dir, path);
		lf12_free_path(path);

		if (NULL == entry) {
			free(*output);
			asprintf(output, "File not found\n");
			lf12_free_metadata(f12_meta);

			return EXIT_FAILURE;
		}

		if (lf12_is_directory(entry)) {
			err = _f12_list_entry(entry, output, args);
			lf12_free_metadata(f12_meta);
			if (F12_SUCCESS != err) {
				free(*output);
				asprintf(output, "%s\n", lf12_strerror(err));

				return EXIT_FAILURE;
			}

			return EXIT_SUCCESS;
		}

		err = _f12_list_entry(entry, output, args);
		lf12_free_metadata(f12_meta);
		if (F12_SUCCESS != err) {
			free(*output);
			asprintf(output, "%s\n", lf12_strerror(err));

			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	err = _f12_list_entry(f12_meta->root_dir, output, args);
	lf12_free_metadata(f12_meta);
	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "%s\n", strerror(err));

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int f12_move(struct f12_move_arguments *args, char **output)
{
	FILE *fp;
	enum lf12_error err;
	char *device_path = args->device_path;
	struct lf12_metadata *f12_meta;
	struct lf12_path *src, *dest;
	struct lf12_directory_entry *src_entry, *dest_entry;

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

	if (F12_SUCCESS != (err = lf12_read_metadata(fp, &f12_meta))) {
		fclose(fp);
		free(*output);
		asprintf(output, "Error loading image: %s\n",
			 lf12_strerror(err));

		return EXIT_FAILURE;
	}

	err = lf12_parse_path(args->source, &src);
	if (F12_EMPTY_PATH == err) {
		free(*output);
		asprintf(output, "Can not move the root directory\n");
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}
	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "%s\n", lf12_strerror(err));
		lf12_free_path(src);
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	err = lf12_parse_path(args->destination, &dest);

	if (F12_SUCCESS != err && F12_EMPTY_PATH != err) {
		free(*output);
		asprintf(output, "%s\n", lf12_strerror(err));
		lf12_free_path(src);
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	switch (lf12_path_get_parent(src, dest)) {
	case F12_PATHS_FIRST:
		free(*output);
		asprintf(output, "Can not move the directory into a child\n");
		lf12_free_path(src);
		lf12_free_path(dest);
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	case F12_PATHS_EQUAL:
		lf12_free_path(src);
		lf12_free_path(dest);
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_SUCCESS;
	default:
		break;
	}

	src_entry = lf12_entry_from_path(f12_meta->root_dir, src);
	if (NULL == src_entry) {
		free(*output);
		asprintf(output, "File or directory %s not found\n",
			 args->source);
		lf12_free_path(src);
		lf12_free_path(dest);
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}
	lf12_free_path(src);

	if (0 == args->recursive && lf12_is_directory(src_entry)
	    && src_entry->child_count > 2) {
		free(*output);
		asprintf(output, "%s\n", lf12_strerror(F12_IS_DIR));
		lf12_free_path(dest);
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	dest_entry = lf12_entry_from_path(f12_meta->root_dir, dest);
	if (NULL == dest_entry) {
		free(*output);
		asprintf(output, "File or directory %s not found\n",
			 args->destination);
		lf12_free_path(dest);
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}
	lf12_free_path(dest);

	if (args->verbose) {
		err = _f12_dump_move(src_entry, dest_entry, output);
		if (err != F12_SUCCESS) {
			free(*output);
			asprintf(output, "Error: %s\n", lf12_strerror(err));
			lf12_free_metadata(f12_meta);
			fclose(fp);

			return EXIT_FAILURE;
		}
	}
	if (F12_SUCCESS != (err = lf12_move_entry(src_entry, dest_entry))) {
		free(*output);
		asprintf(output, "Error: %s\n", lf12_strerror(err));
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "Error: %s\n", lf12_strerror(err));
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	err = lf12_write_metadata(fp, f12_meta);
	lf12_free_metadata(f12_meta);
	fclose(fp);

	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "Error: %s\n", lf12_strerror(err));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int f12_put(struct f12_put_arguments *args, char **output)
{
	enum lf12_error err;
	FILE *fp, *src;
	int res;
	char *device_path = args->device_path;
	struct lf12_metadata *f12_meta;
	struct lf12_path *dest;
	suseconds_t created = time_usec();

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

	if (F12_SUCCESS != (err = lf12_read_metadata(fp, &f12_meta))) {
		fclose(fp);
		free(*output);
		asprintf(output, "Error loading image: %s\n",
			 lf12_strerror(err));
		return EXIT_FAILURE;
	}

	struct stat sb;

	if (0 != stat(args->source, &sb)) {
		free(*output);
		asprintf(output, "Can not open source file\n");
		lf12_free_metadata(f12_meta);

		return EXIT_FAILURE;
	}

	err = lf12_parse_path(args->destination, &dest);
	if (F12_EMPTY_PATH == err) {
		free(*output);
		asprintf(output, "Can not replace root directory\n");
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}
	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "%s\n", lf12_strerror(err));
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_FAILURE;
	}

	if (S_ISDIR(sb.st_mode)) {
		if (!args->recursive) {
			free(*output);
			asprintf(output, "%s\n", lf12_strerror(F12_IS_DIR));
			lf12_free_path(dest);
			lf12_free_metadata(f12_meta);
			fclose(fp);

			return EXIT_FAILURE;
		}
		res = _f12_walk_dir(fp, args, f12_meta, created, output);
		if (res) {
			free(*output);
			asprintf(output, "%s\n", lf12_strerror(err));
			lf12_free_path(dest);
			lf12_free_metadata(f12_meta);
			fclose(fp);

			return EXIT_FAILURE;
		}
	} else if (S_ISREG(sb.st_mode)) {
		if (NULL == (src = fopen(args->source, "r"))) {
			free(*output);
			asprintf(output, "Can not open source file\n");
			lf12_free_path(dest);
			lf12_free_metadata(f12_meta);

			return EXIT_FAILURE;
		}

		err = lf12_create_file(fp, f12_meta, dest, src, created);
		if (F12_SUCCESS != err) {
			free(*output);
			asprintf(output, "%s\n", lf12_strerror(err));
			lf12_free_path(dest);
			lf12_free_metadata(f12_meta);
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
		lf12_free_path(dest);
		lf12_free_metadata(f12_meta);
		fclose(fp);

		return EXIT_SUCCESS;
	}

	lf12_free_path(dest);
	err = lf12_write_metadata(fp, f12_meta);
	lf12_free_metadata(f12_meta);
	fclose(fp);
	if (F12_SUCCESS != err) {
		free(*output);
		asprintf(output, "Error: %s\n", lf12_strerror(err));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
