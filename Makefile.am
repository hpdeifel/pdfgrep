bin_PROGRAMS = pdfgrep

pdfgrep_SOURCES = pdfgrep.cc output.cc output.h exclude.cc exclude.h
pdfgrep_LDADD = $(poppler_cpp_LIBS)
AM_CPPFLAGS = $(poppler_cpp_CFLAGS)

dist_man1_MANS = pdfgrep.1
