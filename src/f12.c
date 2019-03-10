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


static int recursive_del_entry(FILE *fp,
                               struct f12_metadata *f12_meta,
                               struct f12_directory_entry *entry,
                               int soft_delete)
{
        struct f12_directory_entry *child;

        if (f12_is_directory(entry) && f12_get_child_count(entry) > 2) {
                for (int i = 0; i < entry->child_count; i++) {
                        child = &entry->children[i];
                        if (f12_entry_is_empty(child) ||
                            f12_is_dot_dir(child)) {
                                continue;
                        }

                        recursive_del_entry(fp, f12_meta, child, soft_delete);
                }
        }

        f12_del_entry(fp, f12_meta, entry, soft_delete);

        return 0;
}

static int dump_f12_structure(FILE *fp,
                              struct f12_metadata *f12_meta,
                              struct f12_directory_entry *entry,
                              char *dest_path,
                              int verbose)
{
        struct f12_directory_entry *child_entry;
        char *entry_path;

        if (!f12_is_directory(entry)) {
                FILE *dest_fp = fopen(dest_path, "w");
                f12_dump_file(fp, f12_meta, entry, dest_fp);
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

                if (-1 == asprintf(&entry_path, "%s/%s",
                                   dest_path, child_name)) {
                        return -1;
                }

                if (verbose) {
                        printf("%s\n", entry_path);
                }

                free(child_name);
                dump_f12_structure(fp, f12_meta, child_entry, entry_path,
                                   verbose);
                free(entry_path);
        }

        return 0;
}

static int info_dump_bpb(struct f12_metadata *f12_meta, char **output)
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

        return 0;
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

static void walk_dir(FILE *fp, const char *path, const char *dest,
                     struct f12_metadata *f12_meta)
{
        char *src_path;
        char putpath[1024];
        char *paths[] = {path, NULL};

        FTS *ftsp = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
        FTSENT *ent;

        if (ftsp == NULL) {
                perror("fts_open");
                exit(EXIT_FAILURE);
        }

        while (1) {
                ent = fts_read(
                        ftsp); // get next entry (could be file or directory).
                if (ent == NULL) {
                        if (errno == 0)
                                break; // No more items, bail out of while loop
                        else {
                                // fts_read() had an error.
                                perror("fts_read");
                                exit(EXIT_FAILURE);
                        }
                }

                if (ent->fts_info & FTS_F) {
                        int res;
                        FILE *src;
                        src_path = ent->fts_path + strlen(path);
                        memcpy(putpath, dest, strlen(dest));
                        memcpy(putpath + strlen(dest), src_path, strlen(src_path) + 1);

                        printf("Putting %s -> %s\n", src_path, putpath);
                        if (NULL == (src = fopen(ent->fts_path, "r"))) {
                                printf("Cannot open source file %s\n", ent->fts_path);
                                exit(0);
                        }
                        struct f12_path *dest;
                        res = f12_parse_path(putpath, &dest);
                        if (res == -1) {
                                exit(0);
                        }
                        if (res == F12_EMPTY_PATH) {
                                printf("Cannot replace root directory\n");
                                exit(0);
                        }

                        res = f12_create_file(fp, f12_meta, dest, src);
                }
        }

        // close fts and check for error closing.
        if (fts_close(ftsp) == -1)
                perror("fts_close");
}

int f12_create(struct f12_create_arguments *args, char **output);

int f12_defrag(struct f12_defrag_arguments *args, char **output);

int f12_del(struct f12_del_arguments *args, char **output)
{
        FILE *fp;
        int res;
        struct f12_metadata *f12_meta;

        if (NULL == (fp = fopen(args->device_path, "r+"))) {
                return -1;
        }

        res = f12_read_metadata(fp, &f12_meta);

        if (res != 0) {
                fclose(fp);
                return res;
        }

        struct f12_path *path;

        res = f12_parse_path(args->path, &path);
        if (res == -1) {
                fclose(fp);
                return -1;
        }
        if (res == F12_EMPTY_PATH) {
                asprintf(output, "Cannot delete the root dir\n");
                fclose(fp);
                return 0;
        }

        struct f12_directory_entry *entry = f12_entry_from_path(
                f12_meta->root_dir, path);

        if (entry == F12_FILE_NOT_FOUND) {
                asprintf(output, "The file %s was not found on the device\n",
                         args->path);
                fclose(fp);
                return 0;
        }

        if (args->recursive) {
                recursive_del_entry(fp, f12_meta, entry, args->soft_delete);
        } else {
                f12_del_entry(fp, f12_meta, entry, args->soft_delete);
        }

        f12_free_path(path);
        f12_free_metadata(f12_meta);
        fclose(fp);

        return 0;
}

int f12_get(struct f12_get_arguments *args, char **output)
{
        FILE *fp;
        int res;

        struct f12_metadata *f12_meta;

        if (NULL == (fp = fopen(args->device_path, "r+"))) {
                return -1;
        }

        res = f12_read_metadata(fp, &f12_meta);

        if (res != 0) {
                fclose(fp);
                return res;
        }

        struct f12_path *src_path;

        res = f12_parse_path(args->path, &src_path);
        if (res == -1) {
                fclose(fp);
                return -1;
        }

        struct f12_directory_entry *entry = f12_entry_from_path(
                f12_meta->root_dir,
                src_path);
        if (entry == F12_FILE_NOT_FOUND) {
                asprintf(output, "The file %s was not found on the device\n",
                         args->path);
                fclose(fp);
                return 0;
        }

        dump_f12_structure(fp, f12_meta, entry, args->dest, args->verbose);

        fclose(fp);
        return 0;
}

int f12_info(struct f12_info_arguments *args, char **output)
{
        FILE *fp;
        int res;
        char *formated_size, *formated_used_bytes;
        char *device_path = args->device_path;
        struct f12_metadata *f12_meta;

        if (NULL == (fp = fopen(device_path, "r"))) {
                return -1;
        }

        res = f12_read_metadata(fp, &f12_meta);
        fclose(fp);

        if (res != 0) {
                return res;
        }

        formated_size = format_bytes(f12_get_partition_size(f12_meta));
        formated_used_bytes = format_bytes(f12_get_used_bytes(f12_meta));

        asprintf(output,
                 "F12 info\n"
                 "  Partition size:\t\t%s\n"
                 "  Used bytes:\t\t\t%s\n"
                 "  Files:\t\t\t%d\n"
                 "  Directories:\t\t\t%d\n",
                 formated_size,
                 formated_used_bytes,
                 f12_get_file_count(f12_meta->root_dir),
                 f12_get_directory_count(f12_meta->root_dir));

        free(formated_size);
        free(formated_used_bytes);

        if (args->dump_bpb) {
                char *bpb, *tmp;
                res = info_dump_bpb(f12_meta, &bpb);
                asprintf(&tmp, "%s%s", *output, bpb);
                free(bpb);
                free(*output);
                *output = tmp;
        }

        f12_free_metadata(f12_meta);
        return res;
}

int f12_list(struct f12_list_arguments *args, char **output)
{
        FILE *fp;
        char *device_path = args->device_path;
        int res;
        struct f12_metadata *f12_meta;

        *output = malloc(1);

        if (NULL == *output) {
                return -1;
        }

        (*output)[0] = 0;

        if (NULL == (fp = fopen(device_path, "r"))) {
                return -1;
        }

        res = f12_read_metadata(fp, &f12_meta);
        fclose(fp);

        if (res != 0) {
                return res;
        }

        if (args->path != 0 && args->path[0] != '\0') {
                struct f12_path *path;
                int res = f12_parse_path(args->path, &path);
                if (res == -1) {
                        return -1;
                }
                if (res == F12_EMPTY_PATH) {
                        list_root_dir_entries(f12_meta, args, output);

                        return 0;
                }

                struct f12_directory_entry *entry = f12_entry_from_path(
                        f12_meta->root_dir, path);
                f12_free_path(path);

                if (F12_FILE_NOT_FOUND == entry) {
                        asprintf(output, "File not found\n");
                        return 0;
                }

                if (f12_is_directory(entry)) {
                        for (int i = 0; i < entry->child_count; i++) {
                                list_f12_entry(&entry->children[i], output,
                                               args, 0);
                        }

                        return 0;
                }

                list_f12_entry(entry, output, args, 0);

                return 0;
        }

        list_root_dir_entries(f12_meta, args, output);

        f12_free_metadata(f12_meta);

        return 0;
}

int f12_move(struct f12_move_arguments *args, char **output)
{
        FILE *fp;
        int res;
        char *device_path = args->device_path;
        struct f12_metadata *f12_meta;
        struct f12_path *src, *dest;
        struct f12_directory_entry *src_entry, *dest_entry;

        *output = malloc(1);

        if (NULL == *output) {
                return -1;
        }

        (*output)[0] = 0;

        if (NULL == (fp = fopen(device_path, "r+"))) {
                return -1;
        }

        res = f12_read_metadata(fp, &f12_meta);

        if (res != 0) {
                return res;
        }

        res = f12_parse_path(args->source, &src);
        if (res == -1) {
                return -1;
        }
        if (res == F12_EMPTY_PATH) {
                asprintf(output, "Cannot move the root directory\n");
                return 0;
        }

        res = f12_parse_path(args->destination, &dest);
        if (res == -1) {
                return -1;
        }

        switch (f12_path_get_parent(src, dest)) {
                case F12_PATHS_FIRST:
                        asprintf(output,
                                 "Cannot move the directory into a child\n");
                        return 0;
                case F12_PATHS_EQUAL:
                        return 0;
        }

        src_entry = f12_entry_from_path(f12_meta->root_dir, src);

        if (src_entry == F12_FILE_NOT_FOUND) {
                asprintf(output, "File or directory %s not found\n",
                         args->source);
                return 0;
        }

        dest_entry = f12_entry_from_path(f12_meta->root_dir, dest);

        if (dest_entry == F12_FILE_NOT_FOUND) {
                asprintf(output, "File or directory %s not found\n",
                         args->destination);
                return 0;
        }

        res = f12_move_entry(src_entry, dest_entry);

        f12_write_metadata(fp, f12_meta);
        f12_free_metadata(f12_meta);
        fclose(fp);

        return 0;
}

int f12_put(struct f12_put_arguments *args, char **output)
{
        FILE *fp, *src;
        int res;
        char *device_path = args->device_path;
        struct f12_metadata *f12_meta;
        struct f12_path *dest;

        *output = malloc(1);

        if (NULL == *output) {
                return -1;
        }

        (*output)[0] = 0;

        if (NULL == (fp = fopen(device_path, "r+"))) {
                return -1;
        }

        res = f12_read_metadata(fp, &f12_meta);

        if (res != 0) {
                return res;
        }

        struct stat sb;

        if (0 != stat(args->source, &sb)) {
                asprintf(output, "Cannot open source file\n");
                return 0;
        }

        if (S_ISDIR(sb.st_mode)) {
                walk_dir(fp, args->source, args->destination, f12_meta);
        } else if (S_ISREG(sb.st_mode)) {
                if (NULL == (src = fopen(args->source, "r"))) {
                        asprintf(output, "Cannot open source file\n");
                        return 0;
                }
                res = f12_parse_path(args->destination, &dest);
                if (res == -1) {
                        return -1;
                }
                if (res == F12_EMPTY_PATH) {
                        asprintf(output, "Cannot replace root directory\n");
                        return 0;
                }

                res = f12_create_file(fp, f12_meta, dest, src);
        } else {
                asprintf(output, "Source file has unsupported type\n");
                return 0;
        }

        f12_write_metadata(fp, f12_meta);
        f12_free_metadata(f12_meta);
        fclose(fp);

        return 0;
}

int f12_resize(struct f12_resize_arguments *args, char **output);

