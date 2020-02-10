#define _GNU_SOURCE

#include <errno.h>
#include <fts.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "f12.h"

char *_f12_format_bytes(size_t bytes)
{
	char *out;

	if (bytes < 10000) {
		asprintf(&out, "%ld bytes", bytes);
	} else if (bytes < 10000000) {
		asprintf(&out, "%ld KiB  ", bytes / 1024);
	} else if (bytes < 10000000000) {
		asprintf(&out, "%ld MiB  ", bytes / (1024 * 1024));
	} else {
		asprintf(&out, "%ld GiB  ", bytes / (1024 * 1024 * 1024));
	}

	return out;
}

static int evasprintf(char **strp, const char *fmt, va_list ap)
{
	char *tmp = *strp;
	int ret = vasprintf(strp, fmt, ap);
	if (tmp) {
		free(tmp);
	}

	return ret;
}

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

int esprintf(char **strp, const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = evasprintf(strp, fmt, ap);
	va_end(ap);

	return ret;
}

int open_image(FILE * fp, struct lf12_metadata **f12_meta, char **output)
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

int print_error(FILE * fp, struct lf12_metadata *f12_meta, char **strp,
		char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (NULL != fp) {
		fclose(fp);
	}
	if (NULL != f12_meta) {
		lf12_free_metadata(f12_meta);
	}

	evasprintf(strp, fmt, ap);
	va_end(ap);

	return EXIT_FAILURE;
}

suseconds_t time_usec(void)
{
	struct timeval tv = { 0 };
	time_t timer = 0;

	gettimeofday(&tv, NULL);
	timer = time(NULL);

	return timer * 1000000 + tv.tv_usec;
}
