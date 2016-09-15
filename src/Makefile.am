bin_PROGRAMS = pdfgrep

pdfgrep_SOURCES = pdfgrep.h pdfgrep.cc output.cc output.h exclude.cc exclude.h regengine.h regengine.cc search.cc cache.h cache.cc

pdfgrep_LDADD = $(poppler_cpp_LIBS) $(unac_LIBS) $(libpcre_LIBS) $(cov_LDFLAGS) $(xdg_basedir_LIBS) $(LIBGCRYPT_LIBS)
AM_CPPFLAGS = $(poppler_cpp_CFLAGS) $(unac_CFLAGS) $(libpcre_CFLAGS) $(cov_CFLAGS) $(xdg_basedir_CFLAGS) $(LIBGCRYPT_CFLAGS)
