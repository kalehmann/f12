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
	enum f12_error err;
	struct bios_parameter_block *bpb;
	struct f12_metadata *f12_meta;
	struct stat sb;
	suseconds_t created = time_usec();

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

	err = walk_dir(fp, &put_args, f12_meta, created, output);
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
			err = list_entry(f12_meta->root_dir, output, args);
			f12_free_metadata(f12_meta);
			if (F12_SUCCESS != err) {
				free(*output);
				asprintf(output, "%s\n", f12_strerror(err));

				return EXIT_FAILURE;
			}

			return EXIT_SUCCESS;
		}
		if (F12_SUCCESS != err) {
			free(*output);
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
			err = list_entry(entry, output, args);
			f12_free_metadata(f12_meta);
			if (F12_SUCCESS != err) {
				free(*output);
				asprintf(output, "%s\n", f12_strerror(err));

				return EXIT_FAILURE;
			}

			return EXIT_SUCCESS;
		}

		err = list_entry(entry, output, args);
		f12_free_metadata(f12_meta);
		if (F12_SUCCESS != err) {
			free(*output);
			asprintf(output, "%s\n", f12_strerror(err));

			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	err = list_entry(f12_meta->root_dir, output, args);
	f12_free_metadata(f12_meta);
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
		res = walk_dir(fp, args, f12_meta, created, output);
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

		err = f12_create_file(fp, f12_meta, dest, src, created);
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
