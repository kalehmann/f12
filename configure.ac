# AC_INIT - perform the initialization and verification
# Required arguments: package name and version
# Optional arguments: bug-report address, tarname and url.
AC_INIT([f12], [1.0], [ka.lehmann@yahoo.com])
# Is needed to use the archiver.
AM_PROG_AR

# Determine the c compiler to use.
AC_PROG_CC
# Activate libtool
AC_PROG_LIBTOOL

PKG_CHECK_MODULES([CHECK], [check >= 0.9.6])

# Required for the generate makefiles.
# Only argument is a space seperated list of compiler options.
AM_INIT_AUTOMAKE([subdir-objects -Wall])

AC_CONFIG_MACRO_DIRS([m4])

# Creates the files from templates (.in-files)
AC_CONFIG_FILES([
  Makefile
  src/libfat12/Makefile
  src/Makefile
  tests/Makefile
])
# Creates and runs the config.status file
AC_OUTPUT
