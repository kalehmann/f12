#ifndef FAT12_H
#define FAT12_H

struct f12_create_arguments
{
  char *device_path;
  int sectors;
  int sector_size;
  int sectors_per_cluster;
  char *volume_label;
};

struct f12_defrag_arguments
{
  char *device_path;
  int simulate;
};

struct f12_del_arguments
{
  char *device_path;
  char *path;
  int recursive;
  int hard_delete;
};

struct f12_get_arguments
{
  char *device_path;
  char *path;
};

struct f12_info_arguments
{
  char *device_path;
  int dump_bpb;
};

struct f12_list_arguments
{
  char *device_path;
  char *path;
  int with_size;
  int recursive;
};

struct f12_move_arguments
{
  char *device_path;
  char *source;
  char *destination;
};

struct f12_put_arguments
{
  char *device_path;
  char *source;
  char *destination;
};

struct f12_resize_arguments
{
  char *device_path;
  int *size;
};


extern int f12_create(struct f12_create_arguments *, char **output);
extern int f12_defrag(struct f12_defrag_arguments *, char **output);
extern int f12_del(struct f12_del_arguments *, char **output);
extern int f12_get(struct f12_get_arguments *, char **output);
extern int f12_info(struct f12_info_arguments *, char **output);
extern int f12_list(struct f12_list_arguments *, char **output);
extern int f12_move(struct f12_move_arguments *, char **output);
extern int f12_put(struct f12_put_arguments *, char **output);
extern int f12_resize(struct f12_resize_arguments *, char **output);

#endif
