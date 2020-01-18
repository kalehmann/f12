# AC_INIT - initialize autoconf
# Required arguments: package name and version
# Optional arguments: bug-report address, tarname and url.
AC_INIT([f12], [1.0], [mail@kalehmann.de])
# AC_INIT_AUTOMAKE - initialize automake
# Required for the generation of makefiles.
# Only argument is a space seperated list of compiler options.
AM_INIT_AUTOMAKE([subdir-objects -Wall foreign])

AM_PROG_AR
# Determine the C compiler to use.
AC_PROG_CC
# Activate libtool
AC_PROG_LIBTOOL

# Add the directory m4 to the search path for macro files
AC_CONFIG_MACRO_DIRS([m4])

# Add --enable-beautify
AC_FEAT_BEAUTIFY
# Add --enable-coverage
AC_FEAT_COVERAGE

# Make sure that libcheck is installed. The minimum version is 0.11.0 as it
# introduced ck_assert_mem_eq and ck_assert_mem_ne
PKG_CHECK_MODULES([CHECK], [check >= 0.11.0])

# Creates the files from templates (.in-files)
AC_CONFIG_FILES([
  po/Makefile.in
  Makefile
])

# Set the version of GNU gettext
AM_GNU_GETTEXT_VERSION([0.19.8])
# Check for the presense of GNU gettext
# The first argument, intlsymbol should always be [external]
AM_GNU_GETTEXT([external])

# Creates and runs the config.status file
AC_OUTPUT
