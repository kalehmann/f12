#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "f12.h"
#include "libfat12/libfat12.h"

enum lf12_error _f12_dump_move(struct lf12_directory_entry *src,
			       struct lf12_directory_entry *dest, char **output)
{
	enum lf12_error err;
	char *tmp = NULL, *file_name = NULL, *dest_path = NULL, *src_path =
		NULL;
	struct lf12_directory_entry *tmp_entry = src, *child = NULL;
	int i = 0;
	size_t src_offset = 0;

	err = lf12_get_entry_path(dest, &dest_path);
	if (F12_SUCCESS != err) {
		return err;
	}

	if (!lf12_is_directory(tmp_entry)) {
		err = lf12_get_entry_path(src, &src_path);
		if (err != F12_SUCCESS) {
			free(dest_path);

			return err;
		}
		file_name = lf12_get_entry_file_name(src);

		if (*output) {
			esprintf(output, "%s%s -> %s/%s\n", *output,
				 src_path, dest_path, file_name);
		} else {
			esprintf(output, "%s -> %s/%s\n", src_path, dest_path,
				 file_name);
		}
		free(file_name);
		free(src_path);
		free(dest_path);

		return F12_SUCCESS;
	}

	err = lf12_get_entry_path(src, &tmp);
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

		if (lf12_is_dot_dir(child) || lf12_entry_is_empty(child)) {
			i++;
			continue;
		}
		err = lf12_get_entry_path(child, &src_path);
		if (err != F12_SUCCESS) {
			free(dest_path);

			return err;
		}
		if (*output) {
			esprintf(output, "%s%s", *output, src_path);
		} else {
			esprintf(output, "%s", src_path);
		}

		file_name = lf12_get_entry_file_name(src);
		esprintf(output, "%s -> %s/%s%s\n", *output,
			 dest_path, file_name, src_path + src_offset);
		free(file_name);
		free(src_path);

		if (lf12_is_directory(child)
		    && lf12_get_child_count(child) > 2
		    && !lf12_is_dot_dir(child)) {
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
