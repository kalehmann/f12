#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "f12.h"
#include "libfat12/libfat12.h"

/**
 * Formats the contents of a DOS 3.4 extended bios parameter block as a human
 * readable string.
 *
 * @param f12_meta a pointer to the metadata of an fat 12 image
 * @param output a pointer to the string pointer with the output
 */
static void _f12_info_dump_bpb(struct lf12_metadata *f12_meta, char **output)
{
	struct bios_parameter_block *bpb = f12_meta->bpb;

	char *text = _("%s\n"
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
		       "  Volume label:\t\t\t%s\n  File system:\t\t\t%s\n");

	esprintf(output, text, *output, bpb->OEMLabel, bpb->SectorSize,
		 bpb->SectorsPerCluster,
		 bpb->ReservedForBoot, bpb->NumberOfFats,
		 bpb->RootDirEntries, bpb->LogicalSectors, bpb->MediumByte,
		 bpb->SectorsPerFat, bpb->SectorsPerTrack,
		 bpb->NumberOfHeads, bpb->HiddenSectors, bpb->LargeSectors,
		 bpb->DriveNumber, bpb->Flags, bpb->Signature, bpb->VolumeID,
		 bpb->VolumeLabel, bpb->FileSystem);
}

int f12_info(struct f12_info_arguments *args, char **output)
{
	enum lf12_error err;
	struct lf12_metadata *f12_meta = NULL;
	char *formatted_size, *formatted_used_bytes;
	FILE *fp = NULL;
	int res;

	fp = fopen(args->device_path, "r+");
	if (EXIT_SUCCESS != (res = open_image(fp, &f12_meta, output))) {
		return res;
	}
	fclose(fp);
	fp = NULL;

	formatted_size = _f12_format_bytes(lf12_get_partition_size(f12_meta));
	formatted_used_bytes = _f12_format_bytes(lf12_get_used_bytes(f12_meta));

	esprintf(output,
		 _("F12 info\n"
		   "  Partition size:\t\t%s\n"
		   "  Used bytes:\t\t\t%s\n"
		   "  Files:\t\t\t%d\n"
		   "  Directories:\t\t\t%d\n"),
		 formatted_size,
		 formatted_used_bytes,
		 lf12_get_file_count(f12_meta->root_dir),
		 lf12_get_directory_count(f12_meta->root_dir));

	free(formatted_size);
	free(formatted_used_bytes);

	if (args->dump_bpb) {
		_f12_info_dump_bpb(f12_meta, output);
	}

	lf12_free_metadata(f12_meta);

	return EXIT_SUCCESS;
}
