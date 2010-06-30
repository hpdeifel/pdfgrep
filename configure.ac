AC_INIT([pdfgrep], [1.1], [hpdeifel@gmx.de])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADER([config.h])

AC_PROG_CXX
AC_PROG_CC

AC_CHECK_HEADERS([stdlib.h string.h unistd.h sys/ioctl.h getopt.h])

AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([regcomp])
AC_CHECK_FUNCS([getopt_long])


PKG_CHECK_MODULES(poppler, poppler)
AC_SUBST(poppler_CFLAGS)
AC_SUBST(poppler_LIBS)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
