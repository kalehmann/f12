#ifndef F12_CREATE_H
#define F12_CREATE_H

#include "f12.h"
#include "libfat12/libfat12.h"

/**
 * Initializes a bios parameter block based on the values in the arguments.
 *
 * @param bpb a pointer to the bios parameter block to initialize
 * @param args a pointer to the argument structure to initialize the bios
 * parameter block with
 */
void
_f12_initialize_bpb(struct bios_parameter_block *bpb,
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
uint16_t _f12_sectors_per_fat(struct bios_parameter_block *bpb);

#endif
