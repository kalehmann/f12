/* Use the not posix conform function asprintf */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "f12.h"
#include "libfat12/libfat12.h"

char *format_bytes(size_t bytes)
{
  char *out;
  
  if (bytes < 10000) {
    asprintf(&out, "%d bytes", bytes);
  } else if (bytes < 10000000) {
    asprintf(&out, "%d KiB", bytes / 1024);
  } else if (bytes < 10000000000) {
    asprintf(&out, "%d MiB", bytes / (1024 * 1024));
  } else {
    asprintf(&out, "%d GiB", bytes / (1024 * 1024 * 1024));
  }

  return out;
}

static int info_dump_bpb(struct f12_metadata *f12_meta, char **output)
{
  struct bios_parameter_block *bpb = f12_meta->bpb;

  char * text =
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
  
  asprintf(output, text, bpb->OEMLabel, bpb->SectorSize, bpb->SectorsPerCluster,
	   bpb->ReservedForBoot, bpb->NumberOfFats, bpb->RootDirEntries,
	   bpb->LogicalSectors, bpb->MediumByte, bpb->SectorsPerFat,
	   bpb->SectorsPerTrack, bpb->NumberOfHeads, bpb->HiddenSectors,
	   bpb->LargeSectors, bpb->DriveNumber, bpb->Flags, bpb->Signature,
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

  if (args->recursive && f12_is_directory(entry) && !f12_is_dot_dir(entry)) {
    for (int i=0; i<entry->child_count; i++) {
      list_f12_entry(&entry->children[i], output, args, depth + 2);
    }
  }

  return 0;
}

static void list_root_dir_entries(struct f12_metadata *f12_meta,
				  struct f12_list_arguments * args, char **output)
{
  for (int i = 0; i<f12_meta->root_dir->child_count; i++) {
    list_f12_entry(&f12_meta->root_dir->children[i], output, args, 0);
  }
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

  res  = f12_parse_path(args->path, &path);
  if (res == -1) {
    fclose(fp);
    return -1;
  }
  if (res == F12_EMPTY_PATH) {
    asprintf(output, "Cannot delete the root dir\n");
    fclose(fp);
    return 0;
  }

  struct f12_directory_entry *entry = f12_entry_from_path(f12_meta->root_dir, path);

  if (entry == F12_FILE_NOT_FOUND) {
    asprintf(output, "The file %s was not found on the device\n", args->path);
    fclose(fp);
    return 0;
  }
  
  f12_del_entry(fp, f12_meta, entry, args->hard_delete);
  
  f12_free_path(path);
  f12_free_metadata(f12_meta);
  fclose(fp);
  
  return 0;
}
int f12_get(struct f12_get_arguments *args, char **output);
int f12_info(struct f12_info_arguments *args, char ** output)
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
    
    struct f12_directory_entry *entry = f12_entry_from_path(f12_meta->root_dir, path);
    f12_free_path(path);
    
    if (F12_FILE_NOT_FOUND == entry) {
      asprintf(output, "File not found\n");
      return 0;
    }

    if (f12_is_directory(entry)) {
      for (int i=0; i<entry->child_count; i++) {
	list_f12_entry(&entry->children[i], output, args, 0);
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

  switch (f12_path_get_parent(src, dest))
    {
    case F12_PATHS_FIRST:
      asprintf(output, "Cannot move the directory into a child\n");
      return 0;
    case F12_PATHS_EQUAL:
      return 0;
    }

  src_entry = f12_entry_from_path(f12_meta->root_dir, src);

  if (src_entry == F12_FILE_NOT_FOUND) {
    asprintf(output, "File or directory %s not found\n", args->source);
    return 0;
  }

  dest_entry = f12_entry_from_path(f12_meta->root_dir, dest);

  if (dest_entry == F12_FILE_NOT_FOUND) {
    asprintf(output, "File or directory %s not found\n", args->destination);
    return 0;
  }

  res = f12_move_entry(src_entry, dest_entry);

  f12_write_metadata(fp, f12_meta);
  f12_free_metadata(f12_meta);
  fclose(fp);
  
  return 0;
}
int f12_put(struct f12_put_arguments *args, char **output);
int f12_resize(struct f12_resize_arguments *args, char **output);

