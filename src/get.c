#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "common.h"
#include "f12.h"
#include "libfat12/libfat12.h"

static int _f12_dump_f12_structure(FILE * fp, struct lf12_metadata *f12_meta,
				   struct lf12_directory_entry *entry,
				   char *dest_path,
				   struct f12_get_arguments *args,
				   char **output)
{
	int verbose = args->verbose, recursive = args->recursive;
	enum lf12_error err;
	struct lf12_directory_entry *child_entry;
	char *entry_path = NULL;
	int res;

	if (!lf12_is_directory(entry)) {
		FILE *dest_fp = fopen(dest_path, "w");
		err = lf12_dump_file(fp, f12_meta, entry, dest_fp);
		if (F12_SUCCESS != err) {
			esprintf(output, "%s\n", lf12_strerror(err));
			fclose(dest_fp);
			return -1;
		}
		fclose(dest_fp);

		return 0;
	}

	if (!recursive) {
		esprintf(output, "%s\n", lf12_strerror(F12_IS_DIR));

		return -1;
	}

	if (0 != mkdir(dest_path, 0777)) {
		if (errno != EEXIST) {
			return -1;
		}
	}

	for (int i = 0; i < entry->child_count; i++) {
		child_entry = &entry->children[i];

		if (lf12_entry_is_empty(child_entry)
		    || lf12_is_dot_dir(child_entry)) {
			continue;
		}

		char *child_name = lf12_get_entry_file_name(child_entry);

		if (NULL == child_name) {
			return -1;
		}

		esprintf(&entry_path, "%s/%s", dest_path, child_name);

		if (verbose) {
			esprintf(output, "%s%s\n", *output, entry_path);
		}

		free(child_name);
		res = _f12_dump_f12_structure(fp, f12_meta, child_entry,
					      entry_path, args, output);
		free(entry_path);

		if (res) {
			return res;
		}
	}

	return 0;
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
