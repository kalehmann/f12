#! /usr/bin/env bats
# -*- mode: sh -*-
#
# Copyright 2019 by Karsten Lehmann <mail@kalehmann.de>
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
#           multiple instances of this script are run in parallel to prevent
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
        run "${@}"
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
}

@test "I can get info about a fat12 image" {
    _run ./src/f12 info "${TEST_IMAGE}"
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"Partition size"* ]]
    [[ "$output" == *"Used bytes"* ]]
    [[ "$output" == *"Files"* ]]
    [[ "$output" == *"Directories"* ]]
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
    [[ "${#lines[@]}" == "6" ]]
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
