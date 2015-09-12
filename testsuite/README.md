This directory contains the testsuite for pdfgrep.

## Dependencies

The tests depend on the following packages:

  - dejagnu
  - pdflatex

## Running

To run the tests, execute `make check` from the toplevel source
directory.

## Coverage

To get test coverage, run configure with the following arguments:

    ./configure CXXFLAGS="-O0" --enable-coverage

and then run:

    make clean
    make check

This will write a coverage report as HTML to the new directory
`coverage_report`.

## Configuration

The test runs can be configured by editing `testsuite/site.exp`. The
following variables can be set:

- `disable_poppler_version_check`: Don't mark tests as unsupported if
  the poppler version is not recent enought. (This is useful, if you
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

### Procedures

- `pdfgrep_expect`: Call pdfgrep an expect some output.
- `pdfgrep_expect_x`: Call pdfgrep an expect some output, but mark
  this test as known to fail.
- `pdfgrep_expect_error`: Call pdfgrep and expect it print an error.
- `pdfgrep_exit_status`: Compare the exit status of the last spawned
  process.
- `mkpdf`: Generate a pdf using latex.
- `clear_pdfdir`: Delete the content of the directory of generated
  pdfs.
