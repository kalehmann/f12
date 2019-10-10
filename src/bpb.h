#ifndef F12_BPB_H
#define F12_BPB_H

#include "f12.h"
#include "libfat12/libfat12.h"

/**
 * Formats the contents of a DOS 3.4 extended bios parameter block as a human
 * readable string.
 *
 * @param f12_meta a pointer to the metadata of an fat 12 image
 * @param output a pointer to the string pointer with the output
 */
void info_dump_bpb(struct f12_metadata *f12_meta, char **output);

/**
 * Initializes a bios parameter block based on the values in the arguments.
 *
 * @param bpb a pointer to the bios parameter block to initialize
 * @param args a pointer to the argument structure to initialize the bios
 * parameter block with
 */
void initialize_bpb(struct bios_parameter_block *bpb,
		    struct f12_create_arguments *args);

/**
 * Calculate the number of sectors per file allocation table.
 * 
 * @param sectors the total number of sectors
 * @param sector_size the size of each sectors
 * @param sectors_per_cluster the number of sectors per cluster
 * @return
 */
uint16_t sectorsPerFat(uint16_t sectors, uint16_t sector_size,
		       uint16_t sectors_per_cluster);

#endif
