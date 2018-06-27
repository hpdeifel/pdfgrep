ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST=autogen.sh README.md CONTRIBUTING.md NEWS.md

SUBDIRS = src completion doc testsuite

# This hack is needed to prevent autotools from emitting spurious warnings about
# redefining .PHONY
PHONIES =

PHONIES += clang-tidy
clang-tidy:
	@clang-tidy --checks="modernize-*,bugprone-*,performance-*,readability-*" src/*.cc

# Coverage stuff

if ENABLE_COV

PHONIES += cov-reset
cov-reset:
	@rm -rf coverage_report
	@find src/ -name "*.gcda" -exec rm {} \;
	@find src/ -name "*.gcno" -exec rm {} \;
	@lcov --directory src/ --zerocounters

PHONIES += cov-report
cov-report:
	@gcovr -r src/

PHONIES += cov-report-html
cov-report-html: coverage
	@mkdir -p coverage_report
	@lcov --directory src/ --capture --no-external --output-file coverage_report/pdfgrep.info
	@genhtml -o coverage_report/ coverage_report/pdfgrep.info

clean-local:
	@make cov-reset

PHONIES += coverage
coverage:
	@make check
	@make cov-report
endif

.PHONY: $(PHONIES)
