# Macro enabling code coverage with gcov
#
# It add the configuration option --enable-coverage
# If this option is used, the macro checks for the programs gcov, genhtml and
# lcov
# The following variables are set:
#  - ENABLECOVERAGE
#  - COVERAGE_CFLAGS
#  - COVERAGE_LDFLAGS
AC_DEFUN([AC_FEAT_COVERAGE], [
	dnl Check for --enable-coverage

	AC_MSG_CHECKING([whether to enable coverage])
	AC_ARG_ENABLE([coverage],
		[AS_HELP_STRING([--enable-coverage],
			[compile with coverage (default is no)])],
		[enable_coverage=yes],
		[enable_coverage=no])

	AM_CONDITIONAL([ENABLECOVERAGE], [test "x${enable_coverage}" = "xyes"])
	AC_MSG_RESULT($enable_coverage)

	AS_IF([test "x${enable_coverage}" = "xyes"], [
		AC_CHECK_TOOL([GCOV], [gcov], [:])
		AS_IF([test "x${GCOV}" = "x:"],
			[AC_MSG_ERROR([gcov is needed for coverage])])

		AC_CHECK_TOOL([LCOV], [lcov], [:])
		AS_IF([test "x${LCOV}" = ":"],
			[AC_MSG_ERROR([lcov is needed for coverage])])

		AC_CHECK_TOOL([GENHTML], [genhtml], [:])
		AS_IF([test "x${GENHTML}" = "x:"],
			[AC_MSG_ERROR([genhtml is needed for coverage])])

		COVERAGE_CFLAGS="-g -O0 -fprofile-arcs -ftest-coverage"
		COVERAGE_LDFLAGS="-lgcov --coverage -static"

		AC_SUBST([COVERAGE_CFLAGS])
		AC_SUBST([COVERAGE_LDFLAGS])
	])
])
