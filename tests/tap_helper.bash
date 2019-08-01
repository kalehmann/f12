#! /usr/bin/env bash
#
# Copyright 2019 by Karsten Lehmann <mail@kalehmann.de>
#
# This file contains a set of helper functions for bats
# <https://github.com/bats-core/bats-core> to create TAP <https://testanything.org/>
# compliant output.
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


TAP_FILE=""

####
# Sets the file for the tap output.
#
# Globals:
#  TAP_FILE
# Arguments:
#  the path to the file
# Returns:
#   None
###
tap_set_file() {
    TAP_FILE="${1}"
}

####
# Write to the tap output file if it was set with tap_set_file.
#
# Globals:
#  TAP_FILE
# Arguments:
#  the string that should be written to the tap output file
# Returns:
#  None
###
tap_write() {
    if [[ -z "${TAP_FILE}" ]]; then
	return
    fi
    
    echo "${1}" >> "${TAP_FILE}"
}

####
# Writes a file as comment (each line prefixed with '#') to the tap output file if it
# was set with tap_set_file.
#
# Globals:
#  TAP_FILE
# Arguments:
#  the path to the file
# Returns:
#  None
###
tap_write_file() {
    if [[ -z "${TAP_FILE}" ]]; then
	return
    fi
    
    sed -e 's/^/# /g' "${1}" >> "${TAP_FILE}"
}

####
# Setup the header in the tap output file if it was set with tap_set_file.
#
# Globals:
#  TAP_FILE
# Arguments:
#  None
# Returns:
#  None
###
tap_setup() {
    if [[ -z "${TAP_FILE}" ]]; then
        return
    fi

    if [[ -f "${TAP_FILE}" ]]; then
        rm -f "${TAP_FILE}" 
    fi

    local TEST_COUNT
    TEST_COUNT="$(bats --count "${BATS_TEST_FILENAME}")"
    
    tap_write "1..${TEST_COUNT}"
}

####
# This should be called at the beginning of each test. It writes the information for
# the test to the tap output file if it was set with tap_set_file. If the test runs
# fine, calling this function is enough.
#
# Globals:
#  None
# Arguments:
#  None
# Returns:
#  None
###
tap_begin_test() {
    tap_write "ok ${BATS_TEST_NUMBER} ${BATS_TEST_DESCRIPTION}"
}

####
# Mark the last test as failed in the tap output file if it was set with tap_set_file.
# tap_being_test shall always be called before this function.
#
# Globals:
#  None
# Arguments:
#  None
# Returns:
#  None
###
tap_fail() {
    if [[ -z "${VALGRIND_TAP}" ]]; then
	return
    fi

    # Delete the last line
    sed -i '$d' "${VALGRIND_TAP}"
    
    tap_write "not ok ${BATS_TEST_NUMBER} ${BATS_TEST_DESCRIPTION}"
}
