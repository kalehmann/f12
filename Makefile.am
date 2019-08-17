ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src . tests

if ENABLECOVERAGE
coverage-report:
	lcov --directory . --capture --output-file tests.info
	genhtml -o coverage-html/ tests.info
endif

if ENABLEBEAUTIFY
beautify:
	indent $$(find . -name "*.[ch]")
endif

clean-local:
	rm -f tests.info
	rm -rf coverage-html

.PHONY: coverage-report
