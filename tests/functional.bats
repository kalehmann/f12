#! /usr/bin/env bats
# -*- mode: sh -*-
#
# Copyright 2019 - 2020 by Karsten Lehmann <mail@kalehmann.de>
#
# This file defines functional tests for f12 using bats
# <https://github.com/bats-core/bats-core>.
#
#    This file is part of f12.
#
#    f12 is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    f12 is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with f12.  If not, see <https://www.gnu.org/licenses/>.
#
# The tests can be configured with the following variables:
#  TMP_DIR: A directory for temporary files during the test. This should be set when
#           multiple instances of this script are ruen in parallel to prevent
#           complications
#  VALGRIND: Set this variable to run every call to f12 with valgrind.
#  VALGRIND_TAP: If this variable is set, a tap <https://testanything.org/> compliant
#                output for the tests including valgrinds output as comment on an error
#                gets written to the file specified in this variable.

FIXTURE_IMAGE=tests/fixtures/test.img.bak
TMP_DIR=${TMP_DIR:-tests/tmp}
TEST_IMAGE=${TMP_DIR}/test.img
VALGRIND="${VALGRIND}"
VALGRIND_TAP="${VALGRIND_TAP}"

load tap_helper

if [[ -n "${VALGRIND_TAP}" ]]; then
    tap_set_file "${VALGRIND_TAP}"
fi

####
# This generates a regex for extrating the specified information from the output
# of f12's info command with the --dump-bpb option.
#
# Arguments:
#  the information to extract
#  all POSIX character classes for the match seperated by spaces (no brackets or colons)
# Globals:
#  None
# Returns:
#  the regex to extract the given information through command substitution
###
info_regex() {
    read -a parts <<< "${1}"
    read -a classes <<< "${2}"
    
    REGEX="${parts[0]}"
    for part in ${parts[@]:1}
    do
	REGEX="${REGEX}[[:space:]]${part}"
    done
    REGEX="${REGEX}:[[:space:]]+(["
    for class in ${classes}
    do
	REGEX="${REGEX}[:${class}:]"
    done
    REGEX="${REGEX}]+)"

    echo "${REGEX}"
}

####
# This generates a regex for checking for a specific file and its size in the
# output of the f12 list command.
#
# Arguments:
#  the file name
#  the expected size with unit (bytes, KiB, MiB, GiB)
# Globals:
#  None
# Returns:
#  a regex that matches the list output for the file with the given size
###
list_size_regex() {
    FILENAME="${1}"
    read -a splited_size <<< "${2}"
    SIZE="${splitted_size[0]}"
    UNIT="${splitted_size[1]}"

    echo "${FILENAME}[[:space:]]*${SIZE}[[:space:]]${UNIT}"
}

DIR_COUNT_REGEX="$(info_regex Directories digit)"
DRIVE_NUMBER_REGEX="$(info_regex "Drive number" alnum)"
FAT_COUNT_REGEX="$(info_regex "Number of fats" digit)"
FILE_COUNT_REGEX="$(info_regex Files digit)"
HEAD_COUNT_REGEX="$(info_regex "Number of heads" digit)"
MEDIUM_BYTE_REGEX="$(info_regex "Medium byte" alnum)"
ROOT_DIR_ENTRIES_REGEX="$(info_regex "Root directory entries" digit)"
SECTOR_SIZE_REGEX="$(info_regex "Sector size" digit)"
SECTORS_CLUSTER_REGEX="$(info_regex "Sectors per cluster" digit)"
SECTORS_TRACK_REGEX="$(info_regex "Sectors per track" digit)"
VOLUME_LABEL_REGEX="$(info_regex "Volume label" alnum)"

# Matches dates of the format %Y-%m-%d
DATE_REGEX="[[:digit:]]{4}-[[:digit:]]{2}-[[:digit:]]{2}"
# Matches times of the format %H:%M:%S
TIME_REGEX="[[:digit:]]{2}:[[:digit:]]{2}:[[:digit:]]{2}"
# Matches dates of the format %Y-%m-%d %H:%M:%S
DATETIME_REGEX="${DATE_REGEX}[[:space:]]${TIME_REGEX}"
LIST_REGEX="[[:space:]]*[|]->[[:space:]][^[:space:]]*"
LIST_CREATE_REGEX="${LIST_REGEX}[[:space:]]*${DATETIME_REGEX}"
LIST_CREATE_MOD_REGEX="${LIST_CREATE_REGEX}[[:space:]]{2}${DATETIME_REGEX}"
LIST_CREATE_MOD_ACC_REGEX="${LIST_CREATE_MOD_REGEX}[[:space:]]{2}${DATE_REGEX}"

echo ${SECTORS_CLUSTER_REGEX} > sc.out
echo ${SECTOR_SIZE_REGEX} > ss.out

setup() {
    if [[ "${BATS_TEST_NUMBER}" -eq 1 ]]; then
        tap_setup
    fi

    if [[ ! -f "${FIXTURE_IMAGE}" ]]; then
        echo "Fixture ${FIXTURE_IMAGE} not found"
        exit 1
    fi

    tap_begin_test

    mkdir "${TMP_DIR}"
    cp "${FIXTURE_IMAGE}" "${TEST_IMAGE}"
}

teardown() {
    rm -rf "${TMP_DIR}"
}

if [[ -n "${VALGRIND}" ]]; then
    ####
    # This as a wrapper about the bats run helper, which invokes the given command
    # with valgrind.
    #
    # Arguments:
    #  the command to execute
    # Globals:
    #  output
    #  status
    # Returns:
    #  0 on success
    #  1 on failure
    ###
    _run() {
        run libtool --mode=execute \
            valgrind \
                --leak-check=full \
	        --track-origins=yes \
	        --error-exitcode=123 \
	        --log-file="${TMP_DIR}"/valgrind.log \
	        "${@}"
	if [[ "$status" -eq 123 ]]; then
	    tap_fail
	    tap_write_file "${TMP_DIR}"/valgrind.log

            return 1
        fi

        return 0
    }
else
    _run() {
        run libtool --mode=execute "${@}"
    }
fi

@test "I can get the help" {
    _run ./src/f12 --help
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"The following options are available"* ]]
    [[ "$output" == *"put DEVICE"* ]]
    [[ "$output" == *"move DEVICE"* ]]
    [[ "$output" == *"list DEVICE"* ]]
    [[ "$output" == *"info DEVICE"* ]]
    [[ "$output" == *"get DEVICE"* ]]
    [[ "$output" == *"del DEVICE"* ]]
    [[ "$output" == *"create DEVICE"* ]]
}

@test "I can not create a fat12 image at an inaccessible path" {
    _run ./src/f12 create non/existing/directory/test.img
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Error opening image: No such file or directory"* ]]
}

@test "I can create a fat12 image with a volume label" {
    _run ./src/f12 create "${TEST_IMAGE}" --volume-label="NEWF12IMAGE"
    [[ "$status" -eq 0 ]]
    _run ./src/f12 info "${TEST_IMAGE}" --dump-bpb
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${VOLUME_LABEL_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" == "NEWF12IMAGE" ]]
}

@test "I can set the drive number when I create a fat12 image" {
    _run ./src/f12 create "${TEST_IMAGE}" --drive-number=0x81
    [[ "$status" -eq 0 ]]
    _run ./src/f12 info --dump-bpb "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${DRIVE_NUMBER_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" == "0x81" ]]
}

@test "I can specify the number of root directory entries when I create a fat12 image" {
    _run ./src/f12 create "${TEST_IMAGE}" --root-dir-entries=112
    [[ "$status" -eq 0 ]]
    _run ./src/f12 info "${TEST_IMAGE}" --dump-bpb
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${ROOT_DIR_ENTRIES_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" -eq 112 ]]
    for i in {1..112}
    do
	run ./src/f12 put "${TEST_IMAGE}" tests/fixtures/TEST/DATA.BIN FILE"${i}".BIN
	[[ "$status" -eq 0 ]]
    done
    _run ./src/f12 put "${TEST_IMAGE}" tests/fixtures/TEST/DATA.BIN DATA.BIN
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Directory full"* ]]
}

@test "I can specify the sector size when I create a fat12 image" {
    _run ./src/f12 create "${TEST_IMAGE}" --sector-size=4096
    [[ "$status" -eq 0 ]]
    _run ./src/f12 info --dump-bpb "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${SECTOR_SIZE_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" -eq 4096 ]]
}

@test "I can specify the number of sectors per cluster when I create a fat12 image" {
    _run ./src/f12 create "${TEST_IMAGE}" --sectors-per-cluster=4
    [[ "$status" -eq 0 ]]
    _run ./src/f12 info --dump-bpb "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${SECTORS_CLUSTER_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" -eq 4 ]]
}

@test "I can specify the number of file allocation tables when I create a fat12 image" {
    _run ./src/f12 create "${TEST_IMAGE}" --number-of-fats=1
    [[ "$status" -eq 0 ]]
    _run ./src/f12 info --dump-bpb "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${FAT_COUNT_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" -eq 1 ]]
}

@test "I change the geometry and medium byte when I create a fat12 image with a specified size" {
    _run ./src/f12 create "${TEST_IMAGE}" --size=2880
    [[ "$status" -eq 0 ]]
    _run ./src/f12 info --dump-bpb "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${SECTOR_SIZE_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" -eq 512 ]]
    [[ "$output" =~ ${SECTORS_TRACK_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" -eq 36 ]]
    [[ "$output" =~ ${HEAD_COUNT_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" -eq 2 ]]
    [[ "$output" =~ ${MEDIUM_BYTE_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" == "0xf0" ]]

    _run ./src/f12 create "${TEST_IMAGE}" --size=160
    [[ "$status" -eq 0 ]]
    _run ./src/f12 info --dump-bpb "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${SECTOR_SIZE_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" -eq 512 ]]
    [[ "$output" =~ ${SECTORS_TRACK_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" -eq 8 ]]
    [[ "$output" =~ ${HEAD_COUNT_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" -eq 1 ]]
    [[ "$output" =~ ${MEDIUM_BYTE_REGEX} ]]
    [[ "${BASH_REMATCH[1]}" == "0xfe" ]]
}

@test "I can use a local folder as root directory when I create a fat12 image" {
    _run ./src/f12 create "${TEST_IMAGE}" --root-dir=tests/fixtures/TEST
    [[ "$status" -eq 0 ]]
    _run ./src/f12 list --recursive "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"SUBDIR1"* ]]
    [[ "$output" == *"FILE1.TXT"* ]]
    [[ "$output" == *"SUBDIR2"* ]]
    [[ "$output" == *"FILE2.TXT"* ]]
    [[ "$output" == *"DATA.BIN"* ]]
    [[ "$output" == *"TEST.DAT"* ]]
    _run ./src/f12 get "${TEST_IMAGE}" SUBDIR1/FILE1.TXT "${TMP_DIR}"/file1.txt
    run cat "${TMP_DIR}"/file1.txt
    [[ "$output" == "test" ]]
    _run ./src/f12 get "${TEST_IMAGE}" TEST.DAT "${TMP_DIR}"/test.dat
    run cat "${TMP_DIR}"/test.dat
    [[ "$output" == "1234" ]]
}

@test "I get an error when I speficy a nonexistent directory as root directory during the creation of a fat12 image" {
    _run ./src/f12 create "${TEST_IMAGE}" --root-dir=root-dir
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Can not open source file"* ]]
}

@test "I get an error when I specify a regular file as root directory during the creation of a fat12 image" {
    _run ./src/f12 create "${TEST_IMAGE}" --root-dir=Readme.md
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Expected the root dir to be a directory"* ]]
}

@test "I can specify a file to boot when I create a fat12 image with a root directory" {
    _run ./src/f12 create "${TEST_IMAGE}" --root-dir=tests/fixtures/TEST --boot-file=BOOT.BIN
    [[ "$status" -eq 0 ]]
}

@test "I get an error when I specify a boot file without the root-dir option during the creation of a fat12 image" {
    _run ./src/f12 create "${TEST_IMAGE}" --boot-file=BOOT.BIN
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Can not use the boot-file option without also specifying a root directory"* ]]
}

@test "I get an error when I specify a boot file that does not exist in the root directory during the creation of a fat12 image" {
    _run ./src/f12 create "${TEST_IMAGE}" --root-dir=tests/fixtures/TEST --boot-file=NOF.ILE
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Could not find NOF.ILE in the root directory"* ]]
}

@test "I get an error when I specify a boot file in a subdirectory" {
    _run ./src/f12 create "${TEST_IMAGE}" --root-dir=tests/fixtures/TEST --boot-file=SUBDIR1/FILE1.TXT
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"The boot file can not be in a subdirectory"* ]]
}

@test "I can delete a file from a fat12 image" {
    _run ./src/f12 del "${TEST_IMAGE}" FOLDER1/DATA.DAT
    [[ "$status" -eq 0 ]]
    run ./src/f12 list "${TEST_IMAGE}" FOLDER1/DATA.DAT
    [[ "$output" == "File not found" ]]
}

@test "I can delete a directory from a fat12 image" {
    _run ./src/f12 del --recursive "${TEST_IMAGE}" FOLDER1
    [[ "$status" -eq 0 ]]
    _run ./src/f12 list "${TEST_IMAGE}"
    [[ "$output" != *"FOLDER1"* ]]
}

@test "I can list all the files and subdirectories when deleting a directory" {
    _run ./src/f12 del --recursive --verbose "${TEST_IMAGE}" FOLDER1
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"/FOLDER1/SUBDIR/SECRET.TXT"* ]]
    [[ "$output" == *"/FOLDER1/DATA.DAT"* ]]
}

@test "I get an error when I try to delete a nonexistant file" {
    _run ./src/f12 del "${TEST_IMAGE}" NON/EXISTANT/FILE
    [[ "$status" -eq 1 ]]
    [[ "$output" ==  *"The file NON/EXISTANT/FILE was not found"* ]]
}

@test "I get an error when I try to delete a file from a nonexistant image" {
    _run ./src/f12 del no_image FILE1
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Error opening image: No such file or directory"* ]]
}

@test "I can not delete a directory without the recursive flag" {
    _run ./src/f12 del "${TEST_IMAGE}" FOLDER1
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Error: Target is a directory. Maybe use the recursive flag"* ]]
    _run ./src/f12 list "${TEST_IMAGE}"
    [[ "$output" == *"FOLDER1"* ]]
}

@test "I can decrease the number of files by deleting one" {
    _run ./src/f12 info "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${FILE_COUNT_REGEX} ]]
    OLD_FILE_COUNT="${BASH_REMATCH[1]}"
    _run ./src/f12 del "${TEST_IMAGE}" FOLDER1/DATA.DAT
    [[ "$status" -eq 0 ]]
    _run ./src/f12 info "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${FILE_COUNT_REGEX} ]]
    NEW_FILE_COUNT="${BASH_REMATCH[1]}"
    [[ $(( ${OLD_FILE_COUNT} - ${NEW_FILE_COUNT} )) -eq 1 ]]
}

@test "I can decrease the number of directories by deleting one" {
    _run ./src/f12 info "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${DIR_COUNT_REGEX} ]]
    OLD_DIR_COUNT="${BASH_REMATCH[1]}"
    _run ./src/f12 del "${TEST_IMAGE}" --recursive FOLDER2
    [[ "$status" -eq 0 ]]
    _run ./src/f12 info "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${DIR_COUNT_REGEX} ]]
    NEW_DIR_COUNT="${BASH_REMATCH[1]}"
    [[ $(( ${OLD_DIR_COUNT} - ${NEW_DIR_COUNT} )) -eq 1 ]]
}

@test "I can get a file from a fat12 image" {
    _run ./src/f12 get "${TEST_IMAGE}" FOLDER1/SUBDIR/SECRET.TXT "${TMP_DIR}"/secret.txt
    [[ "$status" -eq 0 ]]
    run cat "${TMP_DIR}"/secret.txt
    [[ "$output" == "12345678" ]]
}

@test "I can get a large file from a fat12 image" {
    checksum="$(md5sum LICENSE.txt | awk '{ print $1 }')"
    _run ./src/f12 put "${TEST_IMAGE}" LICENSE.txt TEXT.TXT
    _run ./src/f12 get "${TEST_IMAGE}" TEXT.TXT "${TMP_DIR}"/license.txt
    [[ "$status" -eq 0 ]]
    [[ "$checksum" == "$(md5sum ${TMP_DIR}/license.txt | awk '{ print $1 }')" ]]
}

@test "I can get a directory from a fat12 image" {
    _run ./src/f12 get "${TEST_IMAGE}" FOLDER1/SUBDIR "${TMP_DIR}"/subdir --recursive
    [[ "$status" -eq 0 ]]
    [[ -d "${TMP_DIR}"/subdir ]]
    [[ -f "${TMP_DIR}"/subdir/SECRET.TXT ]]
    run cat "${TMP_DIR}"/subdir/SECRET.TXT
    [[ "$output" == "12345678" ]]
}

@test "I get an error when I try to get a nonexistant file" {
    _run ./src/f12 get "${TEST_IMAGE}" NON/EXISTANT/FILE "${TMP_DIR}"/non.file
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"The file NON/EXISTANT/FILE was not found on the device"* ]]
}

@test "I get an error when I try to get a file from a nonexistant image" {
    _run ./src/f12 get no_image FILE1.TXT "${TMP_DIR}"/file1.txt
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Error opening image: No such file or directory"* ]]
}

@test "I can not get a directory without the recursive flag" {
    _run ./src/f12 get "${TEST_IMAGE}" FOLDER1 "${TMP_DIR}"/folder1
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Target is a directory. Maybe use the recursive flag"* ]]
}

@test "I can get info about a fat12 image" {
    _run ./src/f12 info "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"Partition size"* ]]
    [[ "$output" == *"Used bytes"* ]]
    [[ "$output" == *"Files"* ]]
    [[ "$output" == *"Directories"* ]]
}

@test "I get an error when I request info about a nonexistant image" {
    _run ./src/f12 info no_image
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Error opening image: No such file or directory"* ]]
}

@test "I can dump the bios parameter block of a fat12 image" {
    _run ./src/f12 info --dump-bpb "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"Bios Parameter Block"* ]]
    [[ "$output" == *"OEMLabel"* ]]
    [[ "$output" == *"Sector size"* ]]
    [[ "$output" == *"Sectors per cluster"* ]]
    [[ "$output" == *"VolumeID"* ]]
    [[ "$output" == *"Volume label"* ]]
    [[ "$output" == *"File system"* ]]
}

@test "I can list the content of a fat12 image" {
    _run ./src/f12 list "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "${lines[0]}" == "|-> FOLDER1" ]]
    [[ "${lines[1]}" == "|-> FOLDER2" ]]
    [[ "${lines[2]}" == "|-> FILE.BIN" ]]
}

@test "I can list the content of a fat12 image recursively" {
    _run ./src/f12 list --recursive "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"FOLDER1"* ]]
    [[ "$output" == *"SUBDIR"* ]]
    [[ "$output" == *"SECRET.TXT"* ]]
    [[ "$output" == *"DATA.DAT"* ]]
    [[ "$output" == *"FOLDER2"* ]]
    [[ "$output" == *"FILE.BIN"* ]]
}

@test "I can list the content of a specific directory on a fat12 image" {
    _run ./src/f12 list "${TEST_IMAGE}" FOLDER2
    [[ "$status" -eq 0 ]]
    [[ "${lines[0]}" == "|-> ." ]]
    [[ "${lines[1]}" == "|-> .." ]]
    [[ "${lines[2]}" == "|-> TEXT.TXT" ]]
}

@test "I can list the content of a specific directory on a fat12 image recursively" {
    _run ./src/f12 list --recursive "${TEST_IMAGE}" FOLDER1
    [[ "$status" -eq 0 ]]
    [[ "${lines[0]}" == "|-> ." ]]
    [[ "${lines[1]}" == "|-> .." ]]
    [[ "${lines[2]}" == "|-> SUBDIR" ]]
    [[ "${lines[3]}" == "  |-> ." ]]
    [[ "${lines[4]}" == "  |-> .." ]]
    [[ "${lines[5]}" == "  |-> SECRET.TXT" ]]
    [[ "${lines[6]}" == "|-> DATA.DAT" ]]
}

@test "I can list the creation dates of files and directories" {
    _run ./src/f12 list --recursive --creation "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    for line in "${lines[@]}"
    do
	[[ "${line}" =~ ${LIST_CREATE_REGEX} ]]
    done
}

@test "I can list the creation, modification and access dates of all entries in a subdirectory" {
    _run ./src/f12 list --recursive --creation --modification --access "${TEST_IMAGE}" FOLDER1
    for line in "${lines[@]}"
    do
	[[ "${line}" =~ ${LIST_CREATE_MOD_ACC_REGEX} ]]
    done
}

@test "I can list the size of files" {
    _run ./src/f12 list --recursive --with-size "${TEST_IMAGE}"
    [[ "$output" =~ $(list_size_regex SECRET.TXT "9 bytes") ]]
    [[ "$output" =~ $(list_size_regex FILE.BIN "15 bytes") ]]
}

@test "I can list the size of a single file" {
    _run ./src/f12 list --with-size "${TEST_IMAGE}" FOLDER2/TEXT.TXT
    [[ "$output" == "|-> TEXT.TXT 7 bytes" ]]
}

@test "I can list a single file" {
    _run ./src/f12 list "${TEST_IMAGE}" FILE.BIN
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"FILE.BIN"* ]]
}

@test "I get an error when I list the contents of a nonexistant image" {
    _run ./src/f12 list no_image
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Error opening image: No such file or directory"* ]]
}

@test "I get an error when I list the contents of a nonexistant directory" {
    _run ./src/f12 list "${TEST_IMAGE}" NON/EXISTANT/DIRECTORY
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"File not found"* ]]
}

@test "I can move a file on a fat12 image" {
    _run ./src/f12 move "${TEST_IMAGE}" FILE.BIN FOLDER2
    [[ "$status" -eq 0 ]]
    _run ./src/f12 list "${TEST_IMAGE}" FILE.BIN
    [[ "$output" == "File not found" ]]
    _run ./src/f12 get "${TEST_IMAGE}" FOLDER2/FILE.BIN "${TMP_DIR}"/file.bin
    run cat "${TMP_DIR}"/file.bin
    [[ "$output" == "Binary COntent" ]]
}

@test "I can move a directory on a fat12 image" {
    _run ./src/f12 move --recursive "${TEST_IMAGE}" FOLDER1 FOLDER2
    [[ "$status" -eq 0 ]]
    _run ./src/f12 list "${TEST_IMAGE}" FOLDER1
    [[ "$output" == "File not found" ]]
    _run ./src/f12 get "${TEST_IMAGE}" FOLDER2/FOLDER1/SUBDIR/SECRET.TXT "${TMP_DIR}"/secret.txt
    [[ "$status" -eq 0 ]]
    run cat "${TMP_DIR}"/secret.txt
    [[ "$output" == "12345678" ]]
}

@test "I can move a subdirectory into the root directory" {
    _run ./src/f12 move --recursive "${TEST_IMAGE}" FOLDER1/SUBDIR /
    [[ "$status" -eq 0 ]]
    _run ./src/f12 list "${TEST_IMAGE}" SUBDIR
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"SECRET.TXT"* ]]
    _run ./src/f12 list "${TEST_IMAGE}" FOLDER1/SUBDIR
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"File not found"* ]]
}

@test "I can list all affected files during a movement" {
    _run ./src/f12 move --recursive --verbose "${TEST_IMAGE}" FOLDER1 FOLDER2
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"/FOLDER1/SUBDIR -> /FOLDER2/FOLDER1/SUBDIR"* ]]
    [[ "$output" == *"/FOLDER1/SUBDIR/SECRET.TXT -> /FOLDER2/FOLDER1/SUBDIR/SECRET.TXT"* ]]
    [[ "$output" == *"/FOLDER1/DATA.DAT -> /FOLDER2/FOLDER1/DATA.DAT"* ]]
    _run ./src/f12 move --recursive --verbose "${TEST_IMAGE}" FOLDER2/FOLDER1 /
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"/FOLDER2/FOLDER1/SUBDIR -> /FOLDER1/SUBDIR"* ]]
    [[ "$output" == *"/FOLDER2/FOLDER1/SUBDIR/SECRET.TXT -> /FOLDER1/SUBDIR/SECRET.TXT"* ]]
    [[ "$output" == *"/FOLDER2/FOLDER1/DATA.DAT -> /FOLDER1/DATA.DAT"* ]]

    _run ./src/f12 move --recursive --verbose "${TEST_IMAGE}" FOLDER1/DATA.DAT /
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"/FOLDER1/DATA.DAT -> /DATA.DAT"* ]]
}

@test "I can not move a file on a non existant image" {
    _run ./src/f12 move no_image FILE1 FILE2
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Error opening image: No such file or directory"* ]]
}

@test "I can not move a nonexistant file" {
    _run ./src/f12 move "${TEST_IMAGE}" FILE1 FOLDER2
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"File or directory FILE1 not found"* ]]
}

@test "I can not move a file into a nonexistant directory" {
    _run ./src/f12 move "${TEST_IMAGE}" FILE.BIN NOFOLDER
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"File or directory NOFOLDER not found"* ]]
}

@test "I can not move a directory without the recursive flag" {
    _run ./src/f12 move "${TEST_IMAGE}" FOLDER1 FOLDER2
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Target is a directory. Maybe use the recursive flag"* ]]
}

@test "I can not move a directory into a file" {
    _run ./src/f12 move "${TEST_IMAGE}" --recursive FOLDER1 FILE.BIN
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Not a directory"* ]]
}

@test "I can not move the root directory into a subdirectory" {
    _run ./src/f12 move --recursive "${TEST_IMAGE}" / /FOLDER1
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Can not move the root directory"* ]]
}

@test "I can not move a directory into a subdirectory of it" {
    _run ./src/f12 move --recursive "${TEST_IMAGE}" FOLDER1 FOLDER1/SUBDIR
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Can not move the directory into a child"* ]]
}

@test "I can have source and destination be the same directory when using move" {
    _run ./src/f12 move --recursive "${TEST_IMAGE}" FOLDER1 FOLDER1
    [[ "$status" -eq 0 ]]
}

@test "I can put a single file in the root directory of a fat12 image" {
    _run ./src/f12 put "${TEST_IMAGE}" tests/fixtures/test.txt TEST.TXT
    [[ "$status" -eq 0 ]]
    _run ./src/f12 list "${TEST_IMAGE}"
    [[ "$output" == *"TEST.TXT"* ]]
    _run ./src/f12 get "${TEST_IMAGE}" TEST.TXT "${TMP_DIR}"/test.txt
    [[ "$status" -eq 0 ]]
    run cat "${TMP_DIR}"/test.txt
    [[ "$output" == "This is test content" ]]
}

@test "I can put a single file in a non existing subdirectory of a fat12 image" {
    _run ./src/f12 put "${TEST_IMAGE}" tests/fixtures/test.txt NEW_DIR/TEST.TXT
    [[ "$status" -eq 0 ]]
    _run ./src/f12 list "${TEST_IMAGE}" NEW_DIR
    [[ "$output" == *"TEST.TXT"* ]]
    _run ./src/f12 get "${TEST_IMAGE}" NEW_DIR/TEST.TXT "${TMP_DIR}"/test.txt
    [[ "$status" -eq 0 ]]
    run cat "${TMP_DIR}"/test.txt
    [[ "$output" == "This is test content" ]]
}

@test "I can put a single file in an existent subdirectory of a fat12 image" {
    _run ./src/f12 put "${TEST_IMAGE}" tests/fixtures/test.txt FOLDER1/TEST.TXT
    [[ "$status" -eq 0 ]]
    _run ./src/f12 list "${TEST_IMAGE}" FOLDER1
    [[ "$output" == *"TEST.TXT"* ]]
    _run ./src/f12 get "${TEST_IMAGE}" FOLDER1/TEST.TXT "${TMP_DIR}"/test.txt
    [[ "$status" -eq 0 ]]
    run cat "${TMP_DIR}"/test.txt
    [[ "$output" == "This is test content" ]]
}

@test "I can overwrite an existing file on a fat12 image by putting another file in its place" {
    _run ./src/f12 put "${TEST_IMAGE}" tests/fixtures/test.txt NEW/TEST.TXT
    [[ "$status" -eq 0 ]]
    _run ./src/f12 put "${TEST_IMAGE}" tests/fixtures/data.txt NEW/TEST.TXT
    [[ "$status" -eq 0 ]]
    _run ./src/f12 list "${TEST_IMAGE}" NEW
    [[ "${#lines[@]}" == "3" ]]
    _run ./src/f12 get "${TEST_IMAGE}" NEW/TEST.TXT "${TMP_DIR}"/test.txt
    [[ "$status" -eq 0 ]]
    run cat "${TMP_DIR}"/test.txt
    [[ "$output" == "This is data" ]]
}

@test "I can put a directory on a fat12 image" {
    _run ./src/f12 put --recursive "${TEST_IMAGE}" tests/fixtures/TEST NEW/TESTDIR
    [[ "$status" -eq 0 ]]
    _run ./src/f12 list "${TEST_IMAGE}" NEW/TESTDIR
    [[ "${#lines[@]}" == "7" ]]
    _run ./src/f12 list "${TEST_IMAGE}" NEW/TESTDIR/SUBDIR1
    [[ "${#lines[@]}" == "3" ]]
    [[ "${lines[2]}" == "|-> FILE1.TXT" ]]
    _run ./src/f12 list "${TEST_IMAGE}" NEW/TESTDIR/SUBDIR2
    [[ "${#lines[@]}" == "3" ]]
    [[ "${lines[2]}" == "|-> FILE2.TXT" ]]
    _run ./src/f12 get "${TEST_IMAGE}" NEW/TESTDIR/TEST.DAT "${TMP_DIR}"/test.txt
    [[ "$status" -eq 0 ]]
    run cat "${TMP_DIR}"/test.txt
    [[ "$output" == "1234" ]]
    _run ./src/f12 get "${TEST_IMAGE}" NEW/TESTDIR/DATA.BIN "${TMP_DIR}"/test.txt
    [[ "$status" -eq 0 ]]
    _run cat "${TMP_DIR}"/test.txt
    [[ "$output" == "5678" ]]
    _run ./src/f12 get "${TEST_IMAGE}" NEW/TESTDIR/SUBDIR1/FILE1.TXT "${TMP_DIR}"/test.txt
    [[ "$status" -eq 0 ]]
    _run cat "${TMP_DIR}"/test.txt
    [[ "$output" == "test" ]]
    _run ./src/f12 get "${TEST_IMAGE}" NEW/TESTDIR/SUBDIR2/FILE2.TXT "${TMP_DIR}"/test.txt
    [[ "$status" -eq 0 ]]
    _run cat "${TMP_DIR}"/test.txt
    [[ "$output" == "foo" ]]
}

@test "I can not use a file as directory when I put a file on a fat12 image" {
    _run ./src/f12 put "${TEST_IMAGE}" tests/fixtures/TEST/TEST.DAT FOLDER1/DATA.DAT/NEWFILE
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Not a directory"* ]]
}

@test "I can not put a file to a nonexistent image" {
    _run ./src/f12 put no_image tests/fixtures/TEST/TEST.DAT TESTF.ILE
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Error opening image: No such file or directory"* ]]
}

@test "I can not put a nonexistent file to an image" {
    _run ./src/f12 put "${TEST_IMAGE}" non-existant.file TESTF.ILE
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Can not open source file"* ]]
}

@test "I can not put a directory on an image without the recursive flag" {
    _run ./src/f12 put "${TEST_IMAGE}" tests/fixtures/TEST TEST
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Target is a directory. Maybe use the recursive flag"* ]]
}

@test "I can not overwrite the root directory by putting a directory in its place" {
    _run ./src/f12 put "${TEST_IMAGE}" tests/fixtures/TEST /
    [[ "$status" -eq 1 ]]
    [[ "$output" == *"Can not replace root directory"* ]]
}

@test "I can increase the number of files by putting one to the image" {
    _run ./src/f12 info "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${FILE_COUNT_REGEX} ]]
    OLD_FILE_COUNT="${BASH_REMATCH[1]}"
    _run ./src/f12 put "${TEST_IMAGE}" tests/fixtures/TEST/DATA.BIN NEWF.ILE
    [[ "$status" -eq 0 ]]
    _run ./src/f12 info "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${FILE_COUNT_REGEX} ]]
    NEW_FILE_COUNT="${BASH_REMATCH[1]}"
    [[ $(( ${NEW_FILE_COUNT} - ${OLD_FILE_COUNT} )) -eq 1 ]]
}


@test "I can increase the number of directories by putting one to the image" {
    _run ./src/f12 info "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${DIR_COUNT_REGEX} ]]
    OLD_DIR_COUNT="${BASH_REMATCH[1]}"
    _run ./src/f12 put "${TEST_IMAGE}" --recursive tests/fixtures/TEST/SUBDIR1 NEWDIR1
    [[ "$status" -eq 0 ]]
    _run ./src/f12 info "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" =~ ${DIR_COUNT_REGEX} ]]
    NEW_DIR_COUNT="${BASH_REMATCH[1]}"
    [[ $(( ${NEW_DIR_COUNT} - ${OLD_DIR_COUNT} )) -eq 1 ]]
}

@test "I see the paths of all new files when I put a directory on a fat12 image with the verbose flag" {
    _run ./src/f12 put "${TEST_IMAGE}" --recursive --verbose tests/fixtures/TEST NEWDIR
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"NEWDIR/SUBDIR1/FILE1.TXT"* ]]
    [[ "$output" == *"NEWDIR/DATA.BIN"* ]]
    [[ "$output" == *"NEWDIR/SUBDIR2/FILE2.TXT"* ]]
    [[ "$output" == *"NEWDIR/TEST.DAT"* ]]
}
