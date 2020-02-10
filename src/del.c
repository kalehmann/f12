#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "f12.h"
#include "libfat12/libfat12.h"

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
