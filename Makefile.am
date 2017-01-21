ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST=autogen.sh README.md CONTRIBUTING.md

SUBDIRS = src completion doc testsuite


# Coverage stuff

if ENABLE_COV

.PHONY: cov-reset
cov-reset:
	@rm -rf coverage_report
	@find src/ -name "*.gcda" -exec rm {} \;
	@find src/ -name "*.gcno" -exec rm {} \;
	@lcov --directory src/ --zerocounters

.PHONY: cov-report
cov-report:
	@gcovr -r src/

.PHONY: cov-report-html
cov-report-html: coverage
	@mkdir -p coverage_report
	@lcov --directory src/ --capture --no-external --output-file coverage_report/pdfgrep.info
	@genhtml -o coverage_report/ coverage_report/pdfgrep.info

clean-local:
	@make cov-reset

.PHONY: coverage
coverage:
	@make check
	@make cov-report
endif
