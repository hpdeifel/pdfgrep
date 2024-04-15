This directory contains the testsuite for pdfgrep.

## Dependencies

The tests depend on the following packages:

  - dejagnu
  - pdflatex
  - parskip.sty

## Running

To run the tests, execute `make check` from the toplevel source
directory.

The testsuite might output some harmless messages like "WARNING:
Couldn't find the global config file.". What's really important are
any lines beginning with "FAIL:".

## Coverage

To get a test coverage report, install `gcovr` and run configure with
the following arguments:

    ./configure CXXFLAGS="-O0" --enable-coverage

and then run:

    make clean
    make coverage

This will print a test coverage report to stdout. To get an HTML
report with lcov, run `make cov-report-html`.

## Configuration

The test runs can be configured by editing `testsuite/site.exp`. The
following variables can be set:

- `disable_poppler_version_check`: Don't mark tests as unsupported if
  the poppler version is not recent enough. (This is useful if you
  have backported a patch from a later version to your current one).

## Writing new tests

See the existing tests in `testsuite/pdfgrep.tests/` as example on how
to write tests. The utility functions to use are defined in
`testsuite/lib/pdfgrep.exp` and extensively commented. In particular,
the following functions and variables should be of interest:

### Variables

- `test`: The name of the following test.
- `required_poppler_version`: The least poppler version that this test
  does require.
- `requires_pcre_support`: Whether this test requires libpcre support.
- `requires_unac_support`: Whether this test requires libunac support.

### Procedures

- `pdfgrep_expect`: Call pdfgrep and expect some output.
- `pdfgrep_expect_x`: Call pdfgrep and expect some output, but mark
  this test as known to fail.
- `pdfgrep_expect_error`: Call pdfgrep and expect it to print an error.
- `expect_exit_status`: Compare the exit status of the last spawned
  process.
- `mkpdf`: Generate a pdf using latex.
- `clear_pdfdir`: Delete the content of the directory of generated
  pdfs.
