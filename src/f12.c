/* Use the not posix conform function asprintf */
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fts.h>
#include <libgen.h>

#include "f12.h"
#include "libfat12/libfat12.h"

char *format_bytes(size_t bytes)
{
        char *out;

        if (bytes < 10000) {
                asprintf(&out, "%ld bytes", bytes);
        } else if (bytes < 10000000) {
                asprintf(&out, "%ld KiB", bytes / 1024);
        } else if (bytes < 10000000000) {
                asprintf(&out, "%ld MiB", bytes / (1024 * 1024));
        } else {
                asprintf(&out, "%ld GiB", bytes / (1024 * 1024 * 1024));
        }

        return out;
}


static enum f12_error recursive_del_entry(FILE *fp,
                               struct f12_metadata *f12_meta,
                               struct f12_directory_entry *entry,
                               int soft_delete)
{
        struct f12_directory_entry *child;
        enum f12_error err;

        if (f12_is_directory(entry) && f12_get_child_count(entry) > 2) {
                for (int i = 0; i < entry->child_count; i++) {
                        child = &entry->children[i];
                        if (f12_entry_is_empty(child) ||
                            f12_is_dot_dir(child)) {
                                continue;
                        }

                        err = recursive_del_entry(fp, f12_meta, child, soft_delete);
                        if (F12_SUCCESS != err) {
                                return err;
                        }
                }
        }

        return f12_del_entry(fp, f12_meta, entry, soft_delete);
}

static int dump_f12_structure(FILE *fp,
                              struct f12_metadata *f12_meta,
                              struct f12_directory_entry *entry,
                              char *dest_path,
                              int verbose,
                              char **output)
{
        enum f12_error err;
        struct f12_directory_entry *child_entry;
        char *entry_path, *temp;
        int res;

        if (!f12_is_directory(entry)) {
                FILE *dest_fp = fopen(dest_path, "w");
                err = f12_dump_file(fp, f12_meta, entry, dest_fp);
                if (F12_SUCCESS != err) {
                        temp = *output;
                        asprintf(output, "%s\n%s\n", *output, f12_strerror(err));
                        free(temp);
                        fclose(dest_fp);
                        return -1;
                }
                fclose(dest_fp);

                return 0;
        }

        if (0 != mkdir(dest_path, 0777)) {
                if (errno != EEXIST) {
                        return -1;
                }
        }

        for (int i = 0; i < entry->child_count; i++) {
                child_entry = &entry->children[i];

                if (f12_entry_is_empty(child_entry)
                    || f12_is_dot_dir(child_entry)
                        ) {
                        continue;
                }

                char *child_name = f12_get_file_name(child_entry);

                if (NULL == child_name) {
                        return -1;
                }

                asprintf(&entry_path, "%s/%s", dest_path, child_name);

                if (verbose) {
                        temp = *output;
                        asprintf(output, "%s\n%s\n", *output, entry_path);
                        free(temp);
                }

                free(child_name);
                res = dump_f12_structure(fp, f12_meta, child_entry, entry_path,
                                   verbose, output);
                free(entry_path);

                if (res) {
                        return res;
                }
        }

        return 0;
}

static void info_dump_bpb(struct f12_metadata *f12_meta, char **output)
{
        struct bios_parameter_block *bpb = f12_meta->bpb;

        char *text =
                "\n"
                "Bios Parameter Block\n"
                "  OEMLabel:\t\t\t%s\n"
                "  Sector size:\t\t\t%d bytes\n"
                "  Sectors per cluster:\t\t%d\n"
                "  Reserved sectors:\t\t%d\n"
                "  Number of fats:\t\t%d\n"
                "  Root directory entries:\t%d\n"
                "  Logical sectors:\t\t%d\n"
                "  Medium byte:\t\t\t0x%02x\n"
                "  Sectors per fat:\t\t%d\n"
                "  Sectors per track:\t\t%d\n"
                "  Number of heads:\t\t%d\n"
                "  Hidden sectors:\t\t%d\n"
                "  Logical sectors (large):\t%d\n"
                "  Drive number:\t\t\t0x%02x\n"
                "  Flags:\t\t\t0x%02x\n"
                "  Signature:\t\t\t0x%02x\n"
                "  VolumeID:\t\t\t0x%08x\n"
                "  Volume label:\t\t\t%s\n"
                "  File system:\t\t\t%s\n";

        asprintf(output, text, bpb->OEMLabel, bpb->SectorSize,
                 bpb->SectorsPerCluster,
                 bpb->ReservedForBoot, bpb->NumberOfFats, bpb->RootDirEntries,
                 bpb->LogicalSectors, bpb->MediumByte, bpb->SectorsPerFat,
                 bpb->SectorsPerTrack, bpb->NumberOfHeads, bpb->HiddenSectors,
                 bpb->LargeSectors, bpb->DriveNumber, bpb->Flags,
                 bpb->Signature,
                 bpb->VolumeID, bpb->VolumeLabel, bpb->FileSystem);
}

static int list_f12_entry(struct f12_directory_entry *entry, char **output,
                          struct f12_list_arguments *args, int depth)
{
        if (f12_entry_is_empty(entry)) {
                return 0;
        }

        char *name = f12_get_file_name(entry);

        if (NULL == name) {
                return -1;
        }

        char *temp = *output;
        asprintf(output,
                 "%s"
                 "%*s|-> %s\n",
                 temp,
                 depth, "",
                 name
        );
        free(temp);
        free(name);

        if (args->recursive && f12_is_directory(entry) &&
            !f12_is_dot_dir(entry)) {
                for (int i = 0; i < entry->child_count; i++) {
                        list_f12_entry(&entry->children[i], output, args,
                                       depth + 2);
                }
        }

        return 0;
}

static void list_root_dir_entries(struct f12_metadata *f12_meta,
                                  struct f12_list_arguments *args,
                                  char **output)
{
        for (int i = 0; i < f12_meta->root_dir->child_count; i++) {
                list_f12_entry(&f12_meta->root_dir->children[i], output, args,
                               0);
        }
}

static int walk_dir(FILE *fp, char *path, const char *dest,
                     struct f12_metadata *f12_meta, char **output)
{
        enum f12_error err;
        char *temp;
        char *src_path;
        char putpath[1024];
        char * const paths[] = {path, NULL};

        FTS *ftsp = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
        FTSENT *ent;

        if (ftsp == NULL) {
                asprintf(output, "fts_open error: %s\n",
                         strerror(errno));
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
                        memcpy(putpath + strlen(dest), src_path, strlen(src_path) + 1);

                        temp = *output;
                        asprintf(output, "%s\nPutting %s -> %s\n", *output, src_path, putpath);
                        free(temp);

                        if (NULL == (src = fopen(ent->fts_path, "r"))) {
                                temp = *output;
                                asprintf(output, "%s\nCannot open source file %s\n", *output, ent->fts_path);
                                free(temp);

                                return -1;
                        }
                        struct f12_path *dest;

                        err = f12_parse_path(putpath, &dest);
                        if (F12_SUCCESS != err) {
                                temp = *output;
                                asprintf(output, "%s\n%s\n", *output, f12_strerror(err));
                                free(temp);

                                return -1;
                        }

                        err = f12_create_file(fp, f12_meta, dest, src);
                        if (F12_SUCCESS != err) {
                                temp = *output;
                                asprintf(output, "%s\nError : %s\n", *output, f12_strerror(err));
                                free(temp);

                                return -1;
                        }
                }
        }

        if (fts_close(ftsp) == -1) {
                temp = *output;
                asprintf(output, "%s\nfts_close error: %s\n",
                         *output,
                         strerror(errno));
                free(temp);

                return -1;
        }

        return 0;
}

int f12_create(struct f12_create_arguments *args, char **output);

int f12_del(struct f12_del_arguments *args, char **output)
{
        FILE *fp;
        enum f12_error err;
        struct f12_metadata *f12_meta;

        if (NULL == (fp = fopen(args->device_path, "r+"))) {
                asprintf(output, "Error opening image: %s\n",
                         strerror(errno));
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
                fclose(fp);

                return EXIT_FAILURE;
        }

        struct f12_directory_entry *entry = f12_entry_from_path(
                f12_meta->root_dir, path);

        if (NULL == entry) {
                asprintf(output, "The file %s was not found on the device\n",
                         args->path);
                fclose(fp);
                return EXIT_FAILURE;
        }

        if (args->recursive) {
                err = recursive_del_entry(fp, f12_meta, entry, args->soft_delete);
        } else {
                err = f12_del_entry(fp, f12_meta, entry, args->soft_delete);
        }

        if (err != F12_SUCCESS) {
                asprintf(output, "Error: %s\n",
                         f12_strerror(err));
                f12_free_path(path);
                f12_free_metadata(f12_meta);
                fclose(fp);

                return EXIT_FAILURE;
        }

        f12_free_path(path);
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
                asprintf(output, "Error opening image: %s\n",
                         strerror(errno));
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

        struct f12_directory_entry *entry = f12_entry_from_path(
                f12_meta->root_dir,
                src_path);
        if (NULL == entry) {
                asprintf(output, "The file %s was not found on the device\n",
                         args->path);
                fclose(fp);

                return EXIT_FAILURE;
        }

        res = dump_f12_structure(fp, f12_meta, entry, args->dest, args->verbose, output);

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
                asprintf(output, "Error opening image: %s\n",
                         strerror(errno));
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
                asprintf(output, "Allocation error: %s\n",
                         strerror(errno));
                return EXIT_FAILURE;
        }

        (*output)[0] = 0;

        if (NULL == (fp = fopen(device_path, "r"))) {
                asprintf(output, "Error opening image: %s\n",
                         strerror(errno));
                return EXIT_FAILURE;
        }

        err = f12_read_metadata(fp, &f12_meta);
        fclose(fp);
        if (F12_SUCCESS != err) {
                asprintf(output, "Error loading image: %s\n",
                         f12_strerror(err));
                return EXIT_FAILURE;
        }

        if (args->path != 0 && args->path[0] != '\0') {
                struct f12_path *path;
                err = f12_parse_path(args->path, &path);
                if (F12_EMPTY_PATH == err) {
                        list_root_dir_entries(f12_meta, args, output);

                        return EXIT_SUCCESS;
                }
                if (F12_SUCCESS != err) {
                        asprintf(output, "%s\n", f12_strerror(err));

                        return EXIT_FAILURE;
                }

                struct f12_directory_entry *entry = f12_entry_from_path(
                        f12_meta->root_dir, path);
                f12_free_path(path);

                if (NULL == entry) {
                        asprintf(output, "File not found\n");

                        return EXIT_FAILURE;
                }

                if (f12_is_directory(entry)) {
                        for (int i = 0; i < entry->child_count; i++) {
                                list_f12_entry(&entry->children[i], output,
                                               args, 0);
                        }

                        return EXIT_SUCCESS;
                }

                list_f12_entry(entry, output, args, 0);

                return EXIT_SUCCESS;
        }

        list_root_dir_entries(f12_meta, args, output);

        f12_free_metadata(f12_meta);

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
                asprintf(output, "Allocation error: %s\n",
                         strerror(errno));

                return EXIT_FAILURE;
        }

        (*output)[0] = 0;

        if (NULL == (fp = fopen(device_path, "r+"))) {
                asprintf(output, "Error opening image: %s\n",
                         strerror(errno));
                return EXIT_FAILURE;
        }

        if (F12_SUCCESS != (err = f12_read_metadata(fp, &f12_meta))) {
                fclose(fp);
                asprintf(output, "Error loading image: %s\n",
                         f12_strerror(err));
                return EXIT_FAILURE;
        }

        err = f12_parse_path(args->source, &src);
        if (F12_EMPTY_PATH == err) {
                asprintf(output, "Cannot move the root directory\n");
                fclose(fp);

                return EXIT_FAILURE;
        }
        if (F12_SUCCESS != err) {
                asprintf(output, "%s\n", f12_strerror(err));
                fclose(fp);

                return EXIT_FAILURE;
        }

        err = f12_parse_path(args->destination, &dest);
        if (F12_SUCCESS != err && F12_EMPTY_PATH != err) {
                asprintf(output, "Cannot move the root directory\n");
                fclose(fp);

                return EXIT_FAILURE;
        }

        switch (f12_path_get_parent(src, dest)) {
                case F12_PATHS_FIRST:
                        asprintf(output,
                                 "Cannot move the directory into a child\n");

                        return EXIT_FAILURE;
                case F12_PATHS_EQUAL:
                        return EXIT_SUCCESS;
                default:
                        break;
        }

        src_entry = f12_entry_from_path(f12_meta->root_dir, src);

        if (NULL == src_entry) {
                asprintf(output, "File or directory %s not found\n",
                         args->source);
                return EXIT_SUCCESS;
        }

        dest_entry = f12_entry_from_path(f12_meta->root_dir, dest);

        if (NULL == dest_entry) {
                asprintf(output, "File or directory %s not found\n",
                         args->destination);
                return EXIT_SUCCESS;
        }

        if (F12_SUCCESS != (err = f12_move_entry(src_entry, dest_entry))) {
                asprintf(output, "Error: %s\n", f12_strerror(err));

                return EXIT_FAILURE;
        }

        err = f12_write_metadata(fp, f12_meta);
        f12_free_metadata(f12_meta);
        fclose(fp);
        if (F12_SUCCESS != err) {
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

        *output = malloc(1);

        if (NULL == *output) {
                asprintf(output, "Allocation error: %s\n",
                         strerror(errno));
                return EXIT_FAILURE;
        }

        (*output)[0] = 0;

        if (NULL == (fp = fopen(device_path, "r+"))) {
                asprintf(output, "Error opening image: %s\n",
                         strerror(errno));
                return EXIT_FAILURE;
        }

        if (F12_SUCCESS != (err = f12_read_metadata(fp, &f12_meta))) {
                fclose(fp);
                asprintf(output, "Error loading image: %s\n",
                         f12_strerror(err));
                return EXIT_FAILURE;
        }


        struct stat sb;

        if (0 != stat(args->source, &sb)) {
                asprintf(output, "Cannot open source file\n");
                f12_free_metadata(f12_meta);

                return EXIT_SUCCESS;
        }

        if (S_ISDIR(sb.st_mode)) {
                res = walk_dir(fp, args->source, args->destination, f12_meta, output);
                if (res) {
                        f12_free_metadata(f12_meta);
                        fclose(fp);

                        return EXIT_FAILURE;
                }
        } else if (S_ISREG(sb.st_mode)) {
                if (NULL == (src = fopen(args->source, "r"))) {
                        asprintf(output, "Cannot open source file\n");
                        f12_free_metadata(f12_meta);

                        return EXIT_FAILURE;
                }
                err = f12_parse_path(args->destination, &dest);
                if (F12_EMPTY_PATH == err) {
                        asprintf(output, "Cannot replace root directory\n");
                        f12_free_metadata(f12_meta);
                        fclose(fp);

                        return EXIT_FAILURE;
                }
                if (F12_SUCCESS != err) {
                        asprintf(output, "%s\n", f12_strerror(err));
                        f12_free_metadata(f12_meta);
                        fclose(fp);

                        return EXIT_FAILURE;
                }

                err = f12_create_file(fp, f12_meta, dest, src);
                if (F12_SUCCESS != err) {
                        asprintf(output, "%s\n", f12_strerror(err));
                        f12_free_metadata(f12_meta);
                        fclose(fp);

                        return EXIT_FAILURE;
                }
        } else {
                asprintf(output, "Source file has unsupported type\n");
                return EXIT_SUCCESS;
        }

        err = f12_write_metadata(fp, f12_meta);
        f12_free_metadata(f12_meta);
        fclose(fp);
        if (F12_SUCCESS != err) {
                asprintf(output, "Error: %s\n", f12_strerror(err));
                return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
}
