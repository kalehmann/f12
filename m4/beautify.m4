# Macro for the beautification of the source code
#
# It adds the configuration option --enable-beautify
# If this option is used, the macro checks for the program ident
# and sets tha variale ENABLEBEAUTIFY
AC_DEFUN([AC_FEAT_BEAUTIFY], [
	dnl Check for --enable-beautify

	AC_MSG_CHECKING([Whether to enable beautify])
	AC_ARG_ENABLE([beautify],
		[AS_HELP_STRING([--enable-beautify],
			[enable beautification (default is no)])],
		[enable_beautify=yes],
		[enable_beautify=no])
	AM_CONDITIONAL([ENABLEBEAUTIFY], [test "x${enable_beautify}" = "xyes"])
	AC_MSG_RESULT($enable_beautify)

	AS_IF([test "x${enable_beautify}" = "xyes"], [
		AC_CHECK_TOOL([INDENT], [indent], [:])
		AS_IF([test "x${INDENT}" = "x:"],
			[AC_MSG_ERROR([indent is needed for beautification])])
	])
])
