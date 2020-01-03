#ifndef LF12_IO_P_H
#define LF12_IO_P_H

#include "inttypes.h"

#include "libfat12.h"

/**
 * Get the value for a position in the file allocation table.
 *
 * @param fat a pointer to the compressed file allocation table
 * @param n the number of the entry to readout
 * @return the value of the entry
 */
uint16_t _lf12_read_fat_entry(char *fat, int n);

/**
 * Get the position of a cluster on a fat12 partition.
 *
 * @param cluster the number of the cluster
 * @param f12_meta a pointer to the metadata of the partition
 * @return the position of the cluster on the partition
 */
int _lf12_cluster_offset(uint16_t cluster, struct lf12_metadata *f12_meta);

/**
 * Get the number of clusters in a cluster chain.
 *
 * @param start_cluster number of the first cluster on the cluster chain
 * @param f12_meta a pointer to the metadata of the partition
 * @return the number of clusters in the cluster chain
 */
uint16_t _lf12_get_cluster_chain_length(uint16_t start_cluster,
					struct lf12_metadata *f12_meta);

/**
 * Get the cluster size of the image
 *
 * @param f12_meta a pointer to the metadata of the partition
 * @return the size of a cluster on the partition in bytes
 */
size_t _lf12_get_cluster_size(struct lf12_metadata *f12_meta);

/**
 * Get the size of a cluster chain in bytes.
 *
 * @param start_cluster the number of the first cluster of the cluster chain
 * @param f12_meta a pointer to the metadata of the fat12 partition
 * @return the size of the cluster chain in bytes
 */
size_t _lf12_get_cluster_chain_size(uint16_t start_cluster,
				    struct lf12_metadata *f12_meta);

/**
 * Populate a lf12_directory_entry structure from the raw entry from the disk.
 *
 * @param data a pointer to the raw data
 * @param entry a pointer to the lf12_directory_entry structure to populate
 */
void _lf12_read_dir_entry(char *data, struct lf12_directory_entry *entry);

/**
 * Create a new cluster chain in the file allocation table of the metadata.
 *
 * @param f12_meta a pointer to the metadata of a fat12 image
 * @param cluster_count the number of clusters to allocate
 * @return the first cluster in the new cluster chain or zero if the file
 * allocation table is full
 */
uint16_t _lf12_create_cluster_chain(struct lf12_metadata *f12_meta,
				    int cluster_count);

#endif
