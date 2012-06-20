dist_man1_MANS = pdfgrep.1

EXTRA_DIST = pdfgrep.txt
MAINTAINERCLEANFILES = pdfgrep.1

HTMLFILES = pdfgrep.html

html: $(HTMLFILES)

.txt.1:
	a2x -f manpage -a pdfgrepversion=$(VERSION) $<

.txt.html:
	asciidoc $<
