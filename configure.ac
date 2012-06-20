AC_INIT([pdfgrep], [1.3.0], [pdfgrep-users@lists.sourceforge.net])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADER([config.h])

AC_PROG_CXX
AC_PROG_CC

AC_CHECK_HEADERS([stdlib.h string.h unistd.h sys/ioctl.h getopt.h termios.h])

AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([regcomp])
AC_CHECK_FUNCS([getopt_long])

dnl git revision, code stolen from paludis
if test -d "${GIT_DIR:-${ac_top_srcdir:-./}/.git}" ; then
	GITHEAD=`git describe 2>/dev/null`
	if test -z ${GITHEAD} ; then
		GITHEAD=`git rev-parse HEAD`
	fi
	if test -n "`git diff-index -m --name-only HEAD`" ; then
		GITHEAD=${GITHEAD}-dirty
	fi
else
	GITHEAD=
fi
AC_MSG_CHECKING([for git head])
AC_MSG_RESULT([$GITHEAD])
AC_DEFINE_UNQUOTED([PDFGREP_GIT_HEAD], ["$GITHEAD"], [Git commit used to build this programm])


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

AC_CONFIG_FILES([Makefile completion/Makefile])
AC_OUTPUT
