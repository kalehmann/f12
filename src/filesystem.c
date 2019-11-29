/* Use the not posix conform function asprintf */
#define _GNU_SOURCE

#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "filesystem.h"

int _f12_dump_f12_structure(FILE * fp, struct lf12_metadata *f12_meta,
			    struct lf12_directory_entry *entry, char *dest_path,
			    struct f12_get_arguments *args, char **output)
{
	int verbose = args->verbose, recursive = args->recursive;
	enum lf12_error err;
	struct lf12_directory_entry *child_entry;
	char *entry_path, *temp;
	int res;

	if (!lf12_is_directory(entry)) {
		FILE *dest_fp = fopen(dest_path, "w");
		err = lf12_dump_file(fp, f12_meta, entry, dest_fp);
		if (F12_SUCCESS != err) {
			temp = *output;
			asprintf(output, "%s\n", lf12_strerror(err));
			free(temp);
			fclose(dest_fp);
			return -1;
		}
		fclose(dest_fp);

		return 0;
	}

	if (!recursive) {
		temp = *output;
		asprintf(output, "%s\n", lf12_strerror(F12_IS_DIR));
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

		if (lf12_entry_is_empty(child_entry)
		    || lf12_is_dot_dir(child_entry)) {
			continue;
		}

		char *child_name = lf12_get_file_name(child_entry);

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
			struct lf12_path *dest;

			err = lf12_parse_path(putpath, &dest);
			if (F12_SUCCESS != err) {
				temp = *output;
				if (NULL != *output) {
					asprintf(output, "%s\n%s\n", *output,
						 lf12_strerror(err));
				} else {
					asprintf(output, "%s\n",
						 lf12_strerror(err));
				}
				free(temp);

				return -1;
			}

			err = lf12_create_file(fp, f12_meta, dest, src,
					       created);
			lf12_free_path(dest);
			if (F12_SUCCESS != err) {
				temp = *output;
				asprintf(output, "%s\nError : %s\n", *output,
					 lf12_strerror(err));
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
