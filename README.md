## Overview

*Pdfgrep* is a tool to search text in PDF files. It works similarly to *grep*.

## Features

 - search for regular expressions.
 - support for some important grep options, including:
    - filename output.
    - page number output.
    - optional case insensitivity.
    - count occurrences.
    - recursive search
    - support for extended or (optionally) Perl compatible regular expressions
 - and the most important feature: color output!


For a complete documentation, please consult the [manpage](pdfgrep.html).

## Dependencies

 - poppler-cpp (poppler >= 0.14) [http://poppler.freedesktop.org/]
 - optionally libpcre [http://www.pcre.org/]

## Building

... is easy. Just use the standard procedure:

    ./configure
    make
    sudo make install

The `./configure` script can take lots of options to customize the
build process, the most important of which are:

 - `--with-unac`: Build with experimental libunac support and add
   the `--unac` flag to pdfgrep that strips all accents from
   characters, making it possible to find the character 'ä' by
   searching 'a'.
 - `--with-{zsh,bash}-completion`: Configure installation directory
   for shell completion files.
 - `--without-libpcre`: Disable support for perl compatible regular
   expressions.
 - `--disable-doc`: Disable manpage generation.

See `configure --help` for more info or read the (very extensive)
`INSTALL` file in the source.

If you're using the git version, you will also have to run
`./autogen.sh` in advance.

## Download

Tarballs for releases are available at https://pdfgrep.org/download.html

The development version is available as a git repository at
https://gitlab.com/pdfgrep/pdfgrep

## Contact

General questions, suggestions, bug reports, patches or anything else
can be sent to the mailinglist at
mailto:pdfgrep-users@pdfgrep.org[pdfgrep-users@pdfgrep.org].

You can also use the
[issue tracker](https://gitlab.com/pdfgrep/pdfgrep/issues) for bug
reports or create a
[merge request](https://gitlab.com/pdfgrep/pdfgrep/merge_requests) on
GitLab, if you prefer that over mailinglists.
