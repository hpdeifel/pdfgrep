/***************************************************************************
 *   Copyright (C) 2010 by Hans-Peter Deifel                               *
 *   hpdeifel@gmx.de                                                       *
 *                                                                         *
 *   This program is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include <getopt.h>
#include <unistd.h>

#include <PDFDoc.h>
#include <goo/GooString.h>
#include <TextOutputDev.h>
#include <GlobalParams.h>

#include "config.h"

#define BUFFER_SIZE 1024

static const char *filename_color = "35";
static const char *pagenum_color = "32";
static const char *highlight_color = "01;31";

/* set this to 1 if any match was found. Used for the exit status */
int found_something = 0;

/* default options */

int ignore_case = 0;
int color = 1;
int context = -1;
int print_filename = -1;
int print_pagenum = 0;
int count = 0;
int quiet = 0;

#define HELP_OPTION 1
#define COLOR_OPTION 2

struct option long_options[] =
{
	{"ignore-case", 0, 0, 'i'},
	{"page-number", 0, 0, 'n'},
	{"with-filename", 0, 0, 'H'},
	{"no-filename", 0, 0, 'h'},
	{"count", 0, 0, 'c'},
	{"color", 1, 0, COLOR_OPTION},
	{"context", 1, 0, 'C'},
	{"help", 0, 0, HELP_OPTION},
	{"version", 0, 0, 'V'},
	{"quiet", 0, 0, 'q'},
	{0, 0, 0, 0}
};

struct stream {
	int bufsize;
	char *buf;
	int charpos;
};

struct stream* reset_stream(struct stream *s)
{
	s->bufsize = BUFFER_SIZE;
	free(s->buf);
	s->buf = (char*) malloc(BUFFER_SIZE);
	s->charpos = 0;

	return s;
}

struct stream* make_stream()
{
	struct stream *s = (struct stream*) malloc(sizeof(struct stream));

	s->buf = NULL;

	return reset_stream(s);
}

void maybe_update_buffer(struct stream *s, int len)
{
	if (s->charpos + len > s->bufsize) {
		s->bufsize = s->bufsize * 2;
		s->buf = (char*) realloc(s->buf, s->bufsize);
	}
}

void write_to_stream(void *s, char *text, int length)
{
	struct stream *stream = (struct stream*) s;

	maybe_update_buffer(stream, length);
	strncpy(stream->buf+stream->charpos,
			text, length);
	stream->charpos += length;
}

void start_color(const char *colorcode)
{
	if (color)
		printf("\33[%sm\33[K", colorcode);
}

void end_color()
{
	if (color)
		printf("\33[m\33[K");
}

#define with_color(color, code) \
	do { \
		start_color(color); \
		{ \
			code; \
		} \
		end_color(); \
	} while (0);

void putsn(char *string, int from, int to)
{
	for (; from < to; from++)
		putchar(string[from]);
}

int next_word_left(char *string, int index)
{
	int i = 0;
	int in_whitespace;

	if (index < 0 || string[index] == '\n')
		return -1;

	in_whitespace = isspace(string[index]);
	while (index >= 0 && string[index] != '\n') {
		if (in_whitespace) {
			if (!isspace(string[index]))
				in_whitespace = 0;
		} else {
			if (isspace(string[index]))
				break;
		}
		i++;
		index--;
	}

	return i;
}

int next_word_right(char *string, int index, int buflen)
{
	int i = 0;
	int in_whitespace;

	if (index > buflen || string[index] == '\n')
		return -1;

	in_whitespace = isspace(string[index]);
	while (index < buflen && string[index] != '\n') {
		if (in_whitespace) {
			if (!isspace(string[index]))
				in_whitespace = 0;
		} else {
			if (isspace(string[index]))
				break;
		}
		i++;
		index++;
	}

	return i;
}

void print_context(char *string, int pos, int matchend, int buflen)
{
	int a = pos;
	int b = matchend;
	int chars_left = context;

	int left = next_word_left(string, a-1);
	int right = next_word_right(string, b, buflen);

	while (true) {
		if ((left < 0 || left > chars_left)
			&& (right < 0 || right > chars_left))
			break;

		if (right < 0 || (left > 0 && (pos - a) + left < (b - matchend) + right)) {
			a -= left;
			chars_left -= left;
			left = next_word_left(string,a-1);
		} else {
			b += right;
			chars_left -= right;
			right = next_word_right(string, b, buflen);
		}
	}

	putsn(string, a, pos);

	with_color(highlight_color,
		putsn(string, pos, matchend);
	);

	putsn(string, matchend, b);
}

void print_context_line(char *string, int pos, int matchend, int buflen)
{
	int a = pos;
	int b = matchend;

	while (a >= 0 && string[a] != '\n')
		a--;
	a++;
	
	while (b < buflen && string[b] != '\n')
		b++;


	putsn(string, a, pos);

	with_color(highlight_color,
		putsn(string, pos, matchend);
	);

	putsn(string, matchend, b);
}

int search_in_document(PDFDoc *doc, regex_t *needle)
{
	GooString *filename = doc->getFileName();
	struct stream *stream = make_stream();
	regmatch_t match[] = {{0, 0}};
	int count_matches = 0;
	TextOutputDev *text_out = new TextOutputDev(write_to_stream, stream,
			gFalse, gFalse);

	if (!text_out->isOk()) {
		if (!quiet) {
			fprintf(stderr, "pdfgrep: Could not search %s\n", filename->getCString());
		}
		goto clean;
	}


	for (int i = 1; i <= doc->getNumPages(); i++) {
		doc->displayPage(text_out, i,
				72.0, 72.0, 0,
				gTrue, gFalse, gFalse);

		stream->buf[stream->charpos] = '\0';
		int index = 0;
		
		while (!regexec(needle, stream->buf+index, 1, match, 0)) {
			count_matches++;
			if (quiet) {
				goto clean;
			} else if (!count) {
				if (print_filename)
					with_color(filename_color,
						printf("%s:", filename->getCString()););
				if (print_pagenum)
					with_color(pagenum_color,
						printf("%d:", i););
				if (print_filename || print_pagenum)
					printf(" ");

				if (context < 0)
					print_context_line(stream->buf, index + match[0].rm_so,
							index + match[0].rm_eo, stream->charpos);
				else
					print_context(stream->buf, index + match[0].rm_so,
							index + match[0].rm_eo, stream->charpos);

				printf("\n");
			}

			found_something = 1;

			index += match[0].rm_eo;
		}


		reset_stream(stream);
	}

	if (count && !quiet) {
		if (print_filename)
			with_color(filename_color,
				printf("%s: ", filename->getCString()););
		printf("%d\n", count_matches);
	}
	
clean:
	free(stream->buf);
	free(stream);
	delete text_out;

	return count_matches;
}

void print_usage(char *self)
{
	printf("Usage: %s [OPTION]... PATTERN FILE...\n", self);
}

void print_help(char *self)
{
	print_usage(self);
	printf("\nSearch for PATTERN in each FILE.\n"
"PATTERN is an extended regular expression.\n\n"

"Options:\n"
" -i, --ignore-case\t\tIgnore case distinctions\n"
" -H, --with-filename\t\tPrint the file name for each match\n"
" -h, --no-filename\t\tSuppress the prefixing of file name on output\n"
" -n, --page-number\t\tPrint page number with output lines\n"
" -c, --count\t\t\tPrint only a count of matches per file\n"
" -C, --context NUM\t\tPrint at most NUM chars of context\n"
"     --color WHEN\t\tUse colors for highlighting;\n"
" -q, --quiet\t\t\tSuppress normal output\n"
"\t\t\t\tWHEN can be `always', `never' or `auto'\n"
"     --help\t\t\tPrint this help\n"
" -V, --version\t\t\tShow version information\n"
, self);
}

void print_version()
{
	printf("This is %s version %s\n", PACKAGE, VERSION);
}

int main(int argc, char** argv)
{
	regex_t regex;

	while (1) {
		int c = getopt_long(argc, argv, "icC:nhHVq",
				long_options, NULL);

		if (c == -1)
			break;

		switch (c) {
			case HELP_OPTION:
				print_help(argv[0]);
				exit(0);
			case 'V':
				print_version();
				exit(0);
			case 'n':
				print_pagenum = 1;
				break;
			case 'h':
				print_filename = 0;
				break;
			case 'H':
				print_filename = 1;
				break;
			case 'i':
				ignore_case = REG_ICASE;
				break;
			case 'c':
				count = 1;
				break;
			case COLOR_OPTION:
				if (!optarg)
					break;
				if (!strcmp("always", optarg)) {
					color = 2;
				} else if (!strcmp("never", optarg)) {
					color = 0;
				} else {
					color = 1;
				}
				break;
			case 'C':
				if (optarg)
					context = atoi(optarg);
				break;
			case 'q':
				quiet = 1;
				break;
			case '?':
				/* TODO: do something here */
				break;
			default:
				break;
		}
	}

	if (argc - optind < 2) {
		print_usage(argv[0]);
		exit(2);
	}

	globalParams = new GlobalParams();
	globalParams->setTextPageBreaks(gFalse);
	globalParams->setErrQuiet(gTrue);

	int error = regcomp(&regex, argv[optind++], REG_EXTENDED | ignore_case);
	if (error) {
		char err_msg[256];
		regerror(error, &regex, err_msg, 256);
		fprintf(stderr, "pdfgrep: %s\n", err_msg);
		exit(2);
	}

	if (color == 1 && !isatty(STDOUT_FILENO))
		color = 0;

	if (argc - optind == 1 && print_filename < 0)
		print_filename = 0;
	else
		print_filename = 1;

	for (int i = optind; i < argc; i++) {
		GooString *s = new GooString(argv[i]);
		PDFDoc *doc = new PDFDoc(s);
		
		if (!doc->isOk()) {
			fprintf(stderr, "Could not open %s\n", argv[i]);
			exit(2);
			continue;
		}

		if (search_in_document(doc, &regex) && quiet) {
			exit(0);
		}
	}


	if (found_something) {
		exit(0);
	} else {
		exit(1);
	}
}

