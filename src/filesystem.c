/* Use the not posix conform function asprintf */
#define _GNU_SOURCE

#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "error.h"
#include "filesystem.h"
#include "libfat12/libfat12.h"

static char *convert_path(char *restrict path)
{
	char *final_path = NULL;
	char delimiter[2] = "/";
	char *part = NULL;
	char *converted_part = NULL;
	char *temp = NULL;
	struct lf12_directory_entry entry = { 0 };

	part = strtok(path, delimiter);
	while (NULL != part) {
		converted_part = lf12_convert_name(part);
		if (NULL == converted_part) {
			free(final_path);

			return NULL;
		}

		temp = converted_part;
		converted_part = lf12_get_file_name(temp, temp + 8);
		free(temp);
		if (NULL == converted_part) {
			free(final_path);

			return NULL;
		}

		if (!final_path) {
			final_path = converted_part;
		} else {
			esprintf(&final_path, "%s/%s", final_path,
				 converted_part);
			free(converted_part);
		}
		part = strtok(NULL, delimiter);
	}

	return final_path;
}

static char *destination_path(const char *source, const char *destination)
{
	size_t src_len = strlen(source);
	size_t dest_len = strlen(destination);
	size_t src_offset = dest_len;
	size_t path_len = src_len + dest_len + 1;
	unsigned char dest_has_delim = 1;
	char *restrict path = NULL;
	char *converted_path = NULL;

	if ('/' != destination[dest_len - 1]) {
		dest_has_delim = 0;
		path_len++;
		src_offset++;
	}

	path = malloc(path_len);
	if (NULL == path) {
		return NULL;
	}

	strcpy(path, destination);
	if (!dest_has_delim) {
		path[dest_len] = '/';
	}

	strcpy(path + src_offset, source);

	converted_path = convert_path(path);
	free(path);

	return converted_path;
}

int _f12_dump_f12_structure(FILE * fp, struct lf12_metadata *f12_meta,
			    struct lf12_directory_entry *entry, char *dest_path,
			    struct f12_get_arguments *args, char **output)
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

int _f12_walk_dir(FILE * fp, struct f12_put_arguments *args,
		  struct lf12_metadata *f12_meta, suseconds_t created,
		  char **output)
{
	enum lf12_error err;
	char *source_dir_path = args->source;
	size_t source_offset = strlen(source_dir_path) + 1;
	char *dest = args->destination;
	char *src_path;
	char *putpath;
	char *const paths[] = { source_dir_path, NULL };

	FTS *ftsp = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
	FILE *src;
	FTSENT *ent;

	if (ftsp == NULL) {
		esprintf(output, _("fts_open error: %s\n"), strerror(errno));

		return -1;
	}

	while (1) {
		ent = fts_read(ftsp);
		if (ent == NULL) {
			if (errno == 0) {
				// No more items, leave
				break;
			}
			esprintf(output, _("fts_read error: %s\n"),
				 strerror(errno));

			return -1;
		}

		if (!(ent->fts_info & FTS_F)) {
			continue;
		}
		src_path = ent->fts_path + source_offset;
		putpath = destination_path(src_path, dest);

		if (args->verbose) {
			esprintf(output, "%s'%s' -> '%s'\n", *output,
				 src_path, putpath);
		}

		if (NULL == (src = fopen(ent->fts_path, "r"))) {
			esprintf(output, _("%s\nCannot open source file %s\n"),
				 *output, ent->fts_path);

			return -1;
		}
		struct lf12_path *dest;

		err = lf12_parse_path(putpath, &dest);
		free(putpath);
		if (F12_SUCCESS != err) {
			esprintf(output, "%s\n%s\n", *output,
				 lf12_strerror(err));

			return -1;
		}

		err = lf12_create_file(fp, f12_meta, dest, src, created);
		lf12_free_path(dest);
		if (F12_SUCCESS != err) {
			esprintf(output, _("%s\nError : %s\n"), *output,
				 lf12_strerror(err));

			return -1;
		}
	}

	if (fts_close(ftsp) == -1) {
		esprintf(output, _("%s\nfts_close error: %s\n"), *output,
			 strerror(errno));

		return -1;
	}

	return 0;
}
