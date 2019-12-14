;;; This file contains a dummy Extended Bios Parameter Block for inclusion
;;; in bootcode.
;;; In addition to the bios parameter block, this template also includes the OEM
;;; label before the bpb, which is technically not a part of the bpb.
;;; Theoretically one could also just write
;;; bpb:		times 59   db 0
;;; However with this template one already has the labels for addressing all
;;; the parameters.

OEMLabel:		times 8	   db 0x20
SectorSize:		times 2    db 0
SectorsPerCluster:	times 1    db 0
ReservedForBoot:	times 2    db 0
NumberOfFats:		times 1    db 0
NumberOfRootDirEntries:	times 2    db 0
LogicalSectors:		times 2    db 0
MediumByte:		times 1    db 0
SectorsPerFat:		times 2    db 0
SectorsPerTrack:	times 2    db 0
NumberOfHeads:		times 2    db 0
HiddenSectors:		times 4    db 0
LargeSectors:		times 4    db 0
DriveNumber:		times 2    db 0
Signature:		times 1    db 0
VolumeID:		times 4    db 0
VolumeLabel:		times 11   db 0x20
FileSystem:		times 8	   db 0x20
