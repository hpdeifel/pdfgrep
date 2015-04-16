ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = pdfgrep

pdfgrep_SOURCES = pdfgrep.cc output.cc output.h exclude.cc exclude.h regengine.h regengine.cc
pdfgrep_LDADD = $(poppler_cpp_LIBS) $(unac_LIBS) $(libpcre_LIBS)
AM_CPPFLAGS = $(poppler_cpp_CFLAGS) $(unac_CFLAGS) $(libpcre_CFLAGS)

EXTRA_DIST=autogen.sh

SUBDIRS = completion doc
