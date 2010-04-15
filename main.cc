/***************************************************************************
 *   Copyright (C) 2010 by Hans-Peter Deifel                               *
 *   hpdeifel@gmx.de                                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
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

#define BUFFER_SIZE 1024

static const char *filename_color = "35";
static const char *pagenum_color = "32";
static const char *highlight_color = "01;31";

/* default options */

int ignore_case = 0;
int color = 1;
int context = 50;
int print_filename = -1;
int print_pagenum = 0;
int count = 0;

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
	{"help", 0, 0, HELP_OPTION}
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

void print_context(char *string, int pos, int matchend, int buflen)
{
	int a = pos, b = pos;
	int chars_left_a = context/2;
	int chars_left_b = context/2;

	a = pos;
	b = matchend;

	while (a >= 0 && (chars_left_a || !isspace(string[a]))) {
		a--;
		if (chars_left_a) chars_left_a--;
	}

	while (b < buflen && (chars_left_b || !isspace(string[b]))) {
		b++;
		if (chars_left_b) chars_left_b--;
	}

	fwrite(string+a, pos - a, 1, stdout);
	with_color(highlight_color,
		fwrite(string+pos, matchend-pos, 1, stdout););
	fwrite(string+matchend, b-matchend, 1, stdout);
}

void search_in_document(PDFDoc *doc, regex_t *needle)
{
	GooString *filename = doc->getFileName();
	struct stream *stream = make_stream();
	regmatch_t match[] = {{0, 0}};
	int count_matches = 0;
	TextOutputDev *text_out = new TextOutputDev(write_to_stream, stream,
			gFalse, gFalse);

	if (!text_out->isOk()) {
		fprintf(stderr, "Could not search %s\n", filename->getCString());
		goto clean;
	}


	for (int i = 1; i <= doc->getNumPages(); i++) {
		doc->displayPage(text_out, i,
				72.0, 72.0, 0,
				gTrue, gFalse, gFalse);

		stream->buf[stream->charpos] = '\0';
		int index = 0;
		
		while (!regexec(needle, stream->buf+index, 1, match, 0)) {
			if (count) {
				count_matches++;
			} else {
				if (print_filename)
					with_color(filename_color,
						printf("%s:", filename->getCString()););
				if (print_pagenum)
					with_color(pagenum_color,
						printf("%d:", i););
				if (print_filename || print_pagenum)
					printf(" ");

				print_context(stream->buf, match[0].rm_so, match[0].rm_eo, stream->charpos);

				printf("\n");
			}

			index += match[0].rm_eo;
		}


		reset_stream(stream);
	}

	if (count) {
		if (print_filename)
			with_color(filename_color,
				printf("%s: ", filename->getCString()););
		printf("%d\n", count_matches);
	}
	
clean:
	free(stream->buf);
	free(stream);
	delete text_out;
}

void print_help(char *self)
{
	printf("Usage: %s [files]\n\n"
"Options:\n"
"     --help\t\t\tPrint this help\n"
" -i, --ignore-case\t\tIgnore case distinctions\n"
" -H, --with-filename\t\tPrint the filename for each match\n"
" -h, --no-filename\t\tSuppress the prefixing filename on output\n"
" -n, --page-number\t\tPrint page number with output lines\n"
" -c, --count\t\t\tPrint only a count of matches per file\n"
" -C, --context NUM\t\tPrint NUM chars of context\n"
"     --color \t\t\tUse colors for highlighting;\n"
"\t\t\t\tWHEN can be `always', `never' or `auto'\n"
, self);
}

int main(int argc, char** argv)
{
	regex_t regex;

	while (1) {
		int c = getopt_long(argc, argv, "ic:C:nhH", long_options, NULL);

		if (c == -1)
			break;

		switch (c) {
			case HELP_OPTION:
				print_help(argv[0]);
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
			default:
				break;
		}
	}

	if (argc - optind < 2) {
		fprintf(stderr, "Usage: %s needle [files]\n", argv[0]);
		return 1;
	}

	globalParams = new GlobalParams();

	int error = regcomp(&regex, argv[optind++], REG_EXTENDED | ignore_case);
	if (error) {
		char err_msg[256];
		regerror(error, &regex, err_msg, 256);
		fprintf(stderr, "%s\n", err_msg);
		exit(1);
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
			continue;
		}

		search_in_document(doc, &regex);
	}
}

