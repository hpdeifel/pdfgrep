bin_PROGRAMS = pdfgrep

pdfgrep_SOURCES = pdfgrep.cc output.cc output.h exclude.cc exclude.h
pdfgrep_LDADD = $(poppler_cpp_LIBS) $(unac_LIBS)
AM_CPPFLAGS = $(poppler_cpp_CFLAGS) $(unac_CFLAGS)

SUBDIRS = completion doc
