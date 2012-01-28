#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.68)

AC_INIT([pfish_bovespa], [0.0], [http://github.com/coolparadox/pfish_bovespa/issues])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([pilot_fish/bovespa.h])
AC_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE

AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([pfish_syslog],[pfish_syslog],[],[AC_MSG_ERROR([libpfish_syslog not usable (is pilotfish-syslog installed?)])])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([pilot_fish/syslog.h pilot_fish/syslog_macros.h])

# Syslog facility of this package.
AH_TEMPLATE([SYSLOG_FACILITY],[Syslog facility of this package.])
AC_DEFINE([SYSLOG_FACILITY],[LOG_LOCAL0])

# Option to enable debugging code.
AC_ARG_ENABLE(
	debug,
	[AS_HELP_STRING(
		[--enable-debug],
		[enable debugging code during compilation @<:@default=no@:>@])],
	[case "${enableval}" in
		yes) debugging=1 ;;
		no) debugging=0 ;;
		*) AC_MSG_ERROR(bad value "$enableval" for --enable-debug) ;;
	esac],
	[debugging=0])
AM_CONDITIONAL(DEBUGGING, test x$debugging = x1)
AH_TEMPLATE([DEBUGGING], [Define to enable debugging code.])
test x$debugging = x1 && AC_DEFINE([DEBUGGING], [])
AC_MSG_RESULT([enable debugging code... $debugging])

# Location of the stock database working files.
CFLAGS="$CFLAGS -DDBPATH=\\\"${localstatedir}/pilot_fish/bovespa\\\""

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

# Output configuration files.
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
