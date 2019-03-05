if BUILD_DOCUMENTATION
dist_man1_MANS = pdfgrep.1
DISTCLEANFILES = pdfgrep.1
endif

EXTRA_DIST = pdfgrep.asciidoc

HTMLFILES = pdfgrep.html

html: $(HTMLFILES)

.asciidoc.1:
	a2x -f manpage -a pdfgrepversion=$(VERSION) $<

.asciidoc.html:
	asciidoc $<

