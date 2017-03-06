NEXT RELEASE
------------

  - Bugfix: Fix --cache when used with recursive search

Version 2.0  [2017-01-25]
-------------------------

  - **Incompatible change**: `--context/-C` is now line based as opposed to
    character based and works just like grep
  - Two new options from grep: `-A/--after-context` and `-B/--before-context`
  - Lines with multiple matches are now printed only once
  - Optional caching of pdf-text for faster operation (by Christian Dietrich)
    This adds a **new dependency**: libgcrypt
  - Bash-completion improvements (by Rainer Müller)
  - Bugfix: Fixed string search (`-F`) now works as advertised with multiple
    patterns
  - Bugfix: Empty pages can now be matched with `^$`
  - Bugfix: The pattern `^` now matches *only* at the beginning of pages
  - Bugfix: Text outside of PDF's CropBox but inside the MediaBox is not
    ignored anymore.
  - Various fixes for BSD support

Version 1.4.1  [2015-09-26]
---------------------------

  - Test suite added
  - Bugfix: The tarball contains HACKING and README again
  - Bugfix: The zsh completion completes options as first
    argument correctly
  - Bugfix: Exit status is set as advertised
  - Bugfix: Spurious null bytes removed from output
  - Bugfix: Skipping of some matches in certain conditions fixed.
  - Bugfix: Empty matches don't trigger a loop

Version 1.4.0  [2015-08-14]
---------------------------

  - PCRE support (by Julius Plenz)
  - Fixed string search
  - Ability to pass multiple passwords
  - Option to change the colon as prefix separator
  - Optional warning about PDFs that contain no searchable text
  - New option from grep: `--only-matching`
  - New option from grep: `--null`
  - Bugfix: Correctly print unicode characters
  - Installation: New configure flag `--without-libpcre`
  - Installation: New configure flag `--disable-doc` to disable
        manpage generation with asciidoc
  - Installation: pdfgrep now requires c++11

Version 1.3.2    [2015-02-20]
-----------------------------

  - A bash completion module
  - Don't limit output to 80 characters on non-terminals
  - Print a lot less error messages by default (only with >= poppler-0.30.0)
  - New option `--debug` to print verbose debug output
  - Installation: New configure flag `--with-zsh-completion`

Version 1.3.1    [2014-08-10]
-----------------------------

  - **Incompatible change**: `-r` doesn't follow symlinks
  - A zsh completion module
  - Support for password-protected PDFs
  - Allow to omit '.' with `-r` to search current directory
  - Add `-p` or `--page-count` to count matches per page (by Jascha Knack)
  - Add `-m` or `--max-count` to limit matches per file (by Thibault Marin)

Version 1.3.0    [2012-02-14]
-----------------------------

  - Experimental support for libunac (removing accents and ligatures
	before search)
  - Recursive search [`--recursive`] (by Mahmut Gundes)
  - Don't use colors on dumb terminals
  - A few minor bug fixes
  - Use poppler-cpp instead of the poppler core library (by Pino Toscano)

Version 1.2
-----------

  - 2 small bugfixes (`-h` option and closing open files)
  - This is probably the last release that supports poppler < 0.14

Version 1.1
-----------

  - Respect the `GREP_COLORS` environment variable
  - Fix buffer overflow bug
  - Don't exit on the first error
  - Use terminal width to calculate the context length

Version 1.0
-----------

  - First release
