AC_INIT([pdfgrep], [1.3.0], [pdfgrep-users@lists.sourceforge.net])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADER([config.h])

AC_PROG_CXX
AC_PROG_CC

AC_CHECK_HEADERS([stdlib.h string.h unistd.h sys/ioctl.h getopt.h])

AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([regcomp])
AC_CHECK_FUNCS([getopt_long])


dnl Poppler checking
PKG_CHECK_MODULES(poppler_cpp, poppler-cpp)
AC_SUBST(poppler_cpp_CFLAGS)
AC_SUBST(poppler_cpp_LIBS)

dnl libunac stuff
AC_ARG_WITH([unac],
	AS_HELP_STRING([--with-unac], [enable experimental support for libunac])
)

AS_IF([test "x$with_unac" = "xyes"], [
	PKG_CHECK_MODULES([unac], [unac])
	AC_SUBST(unac_CFLAGS)
	AC_SUBST(unac_LIBS)
	AC_DEFINE([HAVE_UNAC], [1], [Define to 1 if you have libunac _and_ want to use it])
])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
