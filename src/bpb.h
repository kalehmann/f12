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
void info_dump_bpb(struct lf12_metadata *f12_meta, char **output);

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
 * @param bpb a pointer to a bios parameter block structure with at least the
 *            following attributes set: LargeSectors, SectorSize,
 *            SectorsPerCluster, ReservedForBoot, RootDirEntries, NumberOfFats
 *
 * @return
 */
uint16_t sectors_per_fat(struct bios_parameter_block *bpb);
#endif
