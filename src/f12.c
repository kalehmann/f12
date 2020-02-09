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
#include "error.h"
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
		free(boot_name_8_3);

		return F12_FILE_NOT_FOUND;
	}

	sibolo_set_8_3_name(boot_name_8_3);
	free(boot_name_8_3);

	return lf12_install_bootloader(fp, f12_meta, boot_simple_bootloader);
}

static int open_image(FILE * fp, struct lf12_metadata **f12_meta, char **output)
{
	enum lf12_error err;

	if (NULL == fp) {
		return print_error(NULL, NULL, output,
				   _("Error opening image: %s\n"),
				   strerror(errno));
	}

	if (NULL == f12_meta) {
		return EXIT_SUCCESS;
	}

	if (F12_SUCCESS != (err = lf12_read_metadata(fp, f12_meta))) {
		return print_error(fp, *f12_meta, output,
				   _("Error loading image: %s\n"),
				   lf12_strerror(err));
	}

	return EXIT_SUCCESS;
}

static enum lf12_error
recursive_del_entry(FILE * fp,
		    struct lf12_metadata *f12_meta,
		    struct lf12_directory_entry *entry,
		    struct f12_del_arguments *args, char **output)
{
	struct lf12_directory_entry *child;
	enum lf12_error err;
	char *entry_path;
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
		esprintf(output, "%s%s\n", *output, entry_path);
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

int f12_del(struct f12_del_arguments *args, char **output)
{
	struct lf12_directory_entry *entry;
	enum lf12_error err;
	struct lf12_metadata *f12_meta = NULL;
	FILE *fp = NULL;
	struct lf12_path *path;
	int res;

	fp = fopen(args->device_path, "r+");
	if (EXIT_SUCCESS != (res = open_image(fp, &f12_meta, output))) {
		return res;
	}

	err = lf12_parse_path(args->path, &path);
	if (F12_SUCCESS != err) {
		return print_error(fp, f12_meta, output, "%s\n",
				   lf12_strerror(err));
	}

	entry = lf12_entry_from_path(f12_meta->root_dir, path);
	lf12_free_path(path);
	if (NULL == entry) {
		return print_error(fp, f12_meta, output,
				   _("The file %s was not found on the "
				     "device\n"), args->path);
	}

	if (lf12_is_directory(entry) && entry->child_count > 2
	    && !args->recursive) {
		return print_error(fp, f12_meta, output, _("Error: %s\n"),
				   lf12_strerror(F12_IS_DIR));
	}

	err = recursive_del_entry(fp, f12_meta, entry, args, output);
	if (err != F12_SUCCESS) {
		lf12_free_path(path);

		return print_error(fp, f12_meta, output,
				   _("Error while deletion: %s\n"),
				   lf12_strerror(err));
	}

	lf12_free_metadata(f12_meta);
	fclose(fp);

	return EXIT_SUCCESS;
}

int f12_get(struct f12_get_arguments *args, char **output)
{
	struct lf12_directory_entry *entry;
	enum lf12_error err;
	struct lf12_metadata *f12_meta = NULL;
	FILE *fp = NULL;
	int res;
	struct lf12_path *src_path;

	fp = fopen(args->device_path, "r+");
	if (EXIT_SUCCESS != (res = open_image(fp, &f12_meta, output))) {
		return res;
	}

	err = lf12_parse_path(args->path, &src_path);
	if (F12_SUCCESS != err) {
		return print_error(fp, f12_meta, output, "%s\n",
				   lf12_strerror(err));
	}

	entry = lf12_entry_from_path(f12_meta->root_dir, src_path);
	if (NULL == entry) {
		lf12_free_path(src_path);

		return print_error(fp, f12_meta, output,
				   _("The file %s was not found on the "
				     "device\n"), args->path);
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
	struct lf12_metadata *f12_meta = NULL;
	char *formatted_size, *formatted_used_bytes;
	FILE *fp = NULL;
	int res;

	fp = fopen(args->device_path, "r+");
	if (EXIT_SUCCESS != (res = open_image(fp, &f12_meta, output))) {
		return res;
	}
	fclose(fp);
	fp = NULL;

	formatted_size = _f12_format_bytes(lf12_get_partition_size(f12_meta));
	formatted_used_bytes = _f12_format_bytes(lf12_get_used_bytes(f12_meta));

	esprintf(output,
		 _("F12 info\n"
		   "  Partition size:\t\t%s\n"
		   "  Used bytes:\t\t\t%s\n"
		   "  Files:\t\t\t%d\n"
		   "  Directories:\t\t\t%d\n"),
		 formatted_size,
		 formatted_used_bytes,
		 lf12_get_file_count(f12_meta->root_dir),
		 lf12_get_directory_count(f12_meta->root_dir));

	free(formatted_size);
	free(formatted_used_bytes);

	if (args->dump_bpb) {
		char *bpb;
		_f12_info_dump_bpb(f12_meta, &bpb);
		esprintf(output, "%s%s", *output, bpb);
		free(bpb);
	}

	lf12_free_metadata(f12_meta);

	return EXIT_SUCCESS;
}

int f12_list(struct f12_list_arguments *args, char **output)
{
	struct lf12_directory_entry *entry;
	enum lf12_error err;
	struct lf12_metadata *f12_meta = NULL;
	FILE *fp = NULL;
	struct lf12_path *path;
	int res;

	fp = fopen(args->device_path, "r+");
	if (EXIT_SUCCESS != (res = open_image(fp, &f12_meta, output))) {
		return res;
	}
	fclose(fp);
	fp = NULL;

	if (args->path == NULL || args->path[0] == '\0') {
		err = _f12_list_entry(f12_meta->root_dir, output, args);
		lf12_free_metadata(f12_meta);
		f12_meta = NULL;
		if (F12_SUCCESS != err) {
			return print_error(fp, f12_meta, output, "%s\n",
					   strerror(err));
		}

		return EXIT_SUCCESS;
	}

	err = lf12_parse_path(args->path, &path);
	if (F12_EMPTY_PATH == err) {
		err = _f12_list_entry(f12_meta->root_dir, output, args);
		lf12_free_metadata(f12_meta);
		f12_meta = NULL;
		if (F12_SUCCESS != err) {
			return print_error(fp, f12_meta, output, "%s\n",
					   strerror(err));
		}

		return EXIT_SUCCESS;
	}
	if (F12_SUCCESS != err) {
		return print_error(fp, f12_meta, output, "%s\n",
				   lf12_strerror(err));
	}

	entry = lf12_entry_from_path(f12_meta->root_dir, path);
	lf12_free_path(path);
	if (NULL == entry) {
		return print_error(fp, f12_meta, output, _("File not found\n"));
	}

	err = _f12_list_entry(entry, output, args);
	lf12_free_metadata(f12_meta);
	f12_meta = NULL;
	if (F12_SUCCESS == err) {
		return EXIT_SUCCESS;
	}

	return print_error(fp, f12_meta, output, "%s\n", lf12_strerror(err));
}

int f12_move(struct f12_move_arguments *args, char **output)
{
	enum lf12_error err;
	struct lf12_metadata *f12_meta = NULL;
	FILE *fp = NULL;
	int res;
	struct lf12_path *src, *dest;
	struct lf12_directory_entry *src_entry, *dest_entry;

	fp = fopen(args->device_path, "r+");
	if (EXIT_SUCCESS != (res = open_image(fp, &f12_meta, output))) {
		return res;
	}

	err = lf12_parse_path(args->source, &src);
	if (F12_EMPTY_PATH == err) {
		return print_error(fp, f12_meta, output,
				   _("Can not move the root directory\n"));
	}
	if (F12_SUCCESS != err) {
		lf12_free_path(src);

		return print_error(fp, f12_meta, output, "%s\n",
				   lf12_strerror(err));
	}

	err = lf12_parse_path(args->destination, &dest);
	if (F12_SUCCESS != err && F12_EMPTY_PATH != err) {
		lf12_free_path(src);

		return print_error(fp, f12_meta, output, "%s\n",
				   lf12_strerror(err));
	}

	switch (lf12_path_get_parent(src, dest)) {
	case F12_PATHS_FIRST:
		esprintf(output,
			 _("Can not move the directory into a child\n"));
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
	lf12_free_path(src);
	if (NULL == src_entry) {
		lf12_free_path(dest);

		return print_error(fp, f12_meta, output,
				   _("File or directory %s not found\n"),
				   args->source);
	}

	if (0 == args->recursive && lf12_is_directory(src_entry)
	    && src_entry->child_count > 2) {
		lf12_free_path(dest);

		return print_error(fp, f12_meta, output, "%s\n",
				   lf12_strerror(F12_IS_DIR));
	}

	dest_entry = lf12_entry_from_path(f12_meta->root_dir, dest);
	lf12_free_path(dest);
	if (NULL == dest_entry) {
		return print_error(fp, f12_meta, output,
				   _("File or directory %s not found\n"),
				   args->destination);
	}

	if (args->verbose) {
		err = _f12_dump_move(src_entry, dest_entry, output);
		if (err != F12_SUCCESS) {
			return print_error(fp, f12_meta, output,
					   _("Error: %s\n"),
					   lf12_strerror(err));
		}
	}
	if (F12_SUCCESS != (err = lf12_move_entry(src_entry, dest_entry))) {
		return print_error(fp, f12_meta, output, _("Error: %s\n"),
				   lf12_strerror(err));
	}

	err = lf12_write_metadata(fp, f12_meta);
	lf12_free_metadata(f12_meta);
	fclose(fp);

	if (F12_SUCCESS != err) {
		esprintf(output, _("Error: %s\n"), lf12_strerror(err));

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int f12_put(struct f12_put_arguments *args, char **output)
{
	enum lf12_error err;
	FILE *fp = NULL, *src = NULL;
	int res;
	struct lf12_metadata *f12_meta = NULL;
	struct lf12_path *dest;
	suseconds_t created = time_usec();
	struct stat sb;

	fp = fopen(args->device_path, "r+");
	if (EXIT_SUCCESS != (res = open_image(fp, &f12_meta, output))) {
		return res;
	}

	if (0 != stat(args->source, &sb)) {
		return print_error(fp, f12_meta, output,
				   _("Can not open source file\n"));
	}

	err = lf12_parse_path(args->destination, &dest);
	if (F12_EMPTY_PATH == err) {
		return print_error(fp, f12_meta, output,
				   _("Can not replace root directory\n"));
	}
	if (F12_SUCCESS != err) {
		return print_error(fp, f12_meta, output, "%s\n",
				   lf12_strerror(err));
	}

	if (S_ISDIR(sb.st_mode)) {
		if (!args->recursive) {
			lf12_free_path(dest);

			return print_error(fp, f12_meta, output, "%s\n",
					   lf12_strerror(F12_IS_DIR));
		}
		res = _f12_walk_dir(fp, args, f12_meta, created, output);
		if (res) {
			lf12_free_path(dest);

			return print_error(fp, f12_meta, output, "%s\n",
					   lf12_strerror(err));
		}
	} else if (S_ISREG(sb.st_mode)) {
		if (NULL == (src = fopen(args->source, "r"))) {
			lf12_free_path(dest);

			return print_error(fp, f12_meta, output,
					   _("Can not open source file\n"));
		}

		err = lf12_create_file(fp, f12_meta, dest, src, created);
		if (F12_SUCCESS != err) {
			lf12_free_path(dest);

			return print_error(fp, f12_meta, output, "%s\n",
					   lf12_strerror(err));
		}
		if (args->verbose) {
			esprintf(output, "%s -> %s\n", args->source,
				 args->destination);
		}
	} else {
		lf12_free_path(dest);

		return print_error(fp, f12_meta, output,
				   _("Source file has unsupported type\n"));
	}

	lf12_free_path(dest);
	err = lf12_write_metadata(fp, f12_meta);
	lf12_free_metadata(f12_meta);
	fclose(fp);
	if (F12_SUCCESS != err) {
		esprintf(output, _("Error: %s\n"), lf12_strerror(err));

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
