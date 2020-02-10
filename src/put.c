#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common.h"
#include "f12.h"
#include "libfat12/libfat12.h"

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
