HTML = index.html pdfgrep.html changes.html
CSS = style.css
IMAGES = screenshot.png
ASCIIDOC = asciidoc
ASCIIDOCFLAGS = -s -b html5

.PHONY: all clean deploy

all: $(HTML)

%-content.html: %.txt
	$(ASCIIDOC) $(ASCIIDOCFLAGS) -o $@ $<

%.html: %-content.html header.html footer.html
	cat header.html $< footer.html > $@

clean:
	rm -f $(HTML) $(HTML:.html=-content.html)

deploy: all
	scp $(HTML) $(CSS) $(IMAGES) "web.sourceforge.net:/home/project-web/pdfgrep/htdocs/"
