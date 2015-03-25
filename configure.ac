AC_INIT([pdfgrep], [1.3.2], [pdfgrep-users@lists.sourceforge.net])
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
AS_IF([test "x$GITHEAD" != "x"], [
AC_DEFINE_UNQUOTED([PDFGREP_GIT_HEAD], ["$GITHEAD"], [Git commit used to build this programm])
])


dnl Poppler checking
PKG_CHECK_MODULES(poppler_cpp, poppler-cpp)
AC_SUBST(poppler_cpp_CFLAGS)
AC_SUBST(poppler_cpp_LIBS)

dnl PCRE (optional)
AC_CHECK_LIB([pcre], [pcre_compile], [], [AC_MSG_NOTICE([libpcre not found, building without PCRE support])])
AC_SUBST(pcre_CFLAGS)
AC_SUBST(pcre_LIBS)

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


AC_CHECK_PROG(HAVE_A2X, [a2x], [yes], [no])

AC_CONFIG_FILES([Makefile completion/Makefile doc/Makefile])
AC_OUTPUT


AS_IF([test "x$HAVE_A2X" = "xno"], [
	echo
	echo "* I didn't find Asciidoc, so you won't be able to (re)build the manpage."
	echo "* That's not a problem, if you're building from a release-tarball, because"
	echo "* they include a prebuild manpage. But if you build from git, you should"
	echo "* install Asciidoc."
])
