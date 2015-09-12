AC_INIT([pdfgrep], [1.4.0], [pdfgrep-users@pdfgrep.org])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADER([config.h])

AC_PROG_CXX
AC_PROG_CC

dnl check for c++11 std
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])

AC_CHECK_HEADERS([stdlib.h string.h unistd.h sys/ioctl.h getopt.h termios.h])

AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([regcomp])
AC_CHECK_FUNCS([getopt_long])
AC_CHECK_FUNCS([strcasestr])

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
AS_IF([test "x$GITHEAD" != "x"], [
AC_DEFINE_UNQUOTED([PDFGREP_GIT_HEAD], ["$GITHEAD"], [Git commit used to build this programm])
])


dnl Poppler checking
PKG_CHECK_MODULES(poppler_cpp, poppler-cpp)
AC_SUBST(poppler_cpp_CFLAGS)
AC_SUBST(poppler_cpp_LIBS)

dnl PCRE (optional)
AC_ARG_WITH([libpcre],
	AS_HELP_STRING([--without-libpcre], [disable support for perl compatible regular expresssions])
)

AS_IF([test "x$with_libpcre" != "xno"], [
	PKG_CHECK_MODULES([libpcre], [libpcre])
	AC_SUBST(libpcre_CFLAGS)
	AC_SUBST(libpcre_LIBS)
	AC_DEFINE([HAVE_LIBPCRE], [1], [Define to 1 if you have libpcre _and_ want to use it])
])

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

AC_MSG_CHECKING([zsh completion])
AS_VAR_SET([ZSH_COMPL_DIR], ["${datadir}/zsh/site-functions"])
AC_ARG_WITH([zsh-completion],
	[AS_HELP_STRING([--with-zsh-completion=DIR],
		[install zsh-completion file in directory DIR])],
	[AS_CASE([${withval}],
		["/"*], [AS_VAR_COPY([ZSH_COMPL_DIR], [withval])],
		[no], [AS_VAR_SET([ZSH_COMPL_DIR], [])])])
AC_SUBST(ZSH_COMPL_DIR)
AM_CONDITIONAL([INSTALL_ZSH_COMPLETION], [test "x$ZSH_COMPL_DIR" != "x"])
AM_COND_IF([INSTALL_ZSH_COMPLETION],
  [AC_MSG_RESULT($ZSH_COMPL_DIR)],
  [AC_MSG_RESULT(no)])

AC_MSG_CHECKING([bash completion])
AS_VAR_SET([BASH_COMPL_DIR], ["${datadir}/bash-completion/completions"])
AC_ARG_WITH([bash-completion],
	[AS_HELP_STRING([--with-bash-completion=DIR],
		[install bash-completion file in directory DIR])],
	[AS_CASE([${withval}],
		["/"*], [AS_VAR_COPY([BASH_COMPL_DIR], [withval])],
		[no], [AS_VAR_SET([BASH_COMPL_DIR], [])])])
AC_SUBST(BASH_COMPL_DIR)
AM_CONDITIONAL([INSTALL_BASH_COMPLETION], [test "x$BASH_COMPL_DIR" != "x"])
AM_COND_IF([INSTALL_BASH_COMPLETION],
  [AC_MSG_RESULT($BASH_COMPL_DIR)],
  [AC_MSG_RESULT(no)])

AC_ARG_ENABLE([doc],
	AS_HELP_STRING([--disable-doc], [disable manpage generation])
)

AS_IF([test "x$enable_doc" != "xno"], [
	AC_CHECK_PROG(HAVE_A2X, [a2x], [yes], [no])

	AS_IF([test "x$HAVE_A2X" = "xno"], [
		echo
		echo "* I didn't find Asciidoc, so you won't be able to (re)build the manpage."
		echo "* That's not a problem, if you're building from a release-tarball, because"
		echo "* they include a prebuild manpage. But if you build from git, you should"
		echo "* install Asciidoc or disable manpage generation with:"
		echo "* ./configure --disable-doc"
	])
])

AM_CONDITIONAL([BUILD_DOCUMENTATION], [test "x$enable_doc" != "xno"])

dnl Set global config file for DEJAGNU

if test x"$DEJAGNU" = x
then
	DEJAGNU="\$(top_srcdir)/testsuite/global-conf.exp"
fi

AC_SUBST(DEJAGNU)

# Coverage ssupport
AC_ARG_ENABLE([coverage],
	AS_HELP_STRING([--enable-coverage], [compile for coverage reports with gcov. For testing only. (default=no)])
)
AS_IF([test "x$enable_coverage" = "xyes"], [
	COV_FLAGS="--coverage"
	cov_CFLAGS=${COV_FLAGS}
	cov_LDFLAGS=${COV_FLAGS}
	AC_SUBST(cov_CFLAGS)
	AC_SUBST(cov_LDFLAGS)

	# check for coverage programs
	AC_CHECK_PROG(HAVE_GCOV, [gcov], [yes], [no])
	AS_IF([test "x$HAVE_GCOV" = "xno"], [
		AC_MSG_ERROR([gcov not found, but configured with --enable-coverage])
	])
	AC_CHECK_PROG(HAVE_LCOV, [lcov], [yes], [no])
	AS_IF([test "x$HAVE_LCOV" = "xno"], [
		AC_MSG_ERROR([lcov not found, but configured with --enable-coverage])
	])
	AC_CHECK_PROG(HAVE_GENHTML, [genhtml], [yes], [no])
	AS_IF([test "x$HAVE_GENHTML" = "xno"], [
		AC_MSG_ERROR([genhtml not found, but configured with --enable-coverage])
	])
])

AM_CONDITIONAL([ENABLE_COV], test "x$enable_coverage" = "xyes")

AC_CONFIG_FILES([Makefile
	src/Makefile
	completion/Makefile
	doc/Makefile
	testsuite/Makefile
	testsuite/config/Makefile
	testsuite/lib/Makefile
	testsuite/pdfgrep.tests/Makefile])
AC_OUTPUT

