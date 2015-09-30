/***************************************************************************
 *   Copyright (C) 2015 by Hans-Peter Deifel                               *
 *   hpd@hpdeifel.de                                                       *
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
 *   Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
 *   Boston, MA 02110-1301 USA.                                            *
 ***************************************************************************/

#include "output.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <iostream>

using namespace std;

static bool is_valid_color(const char* colorcode) {
	return colorcode && strcmp(colorcode, "");
}

static void start_color(bool use_colors, const char *colorcode)
{
	if (use_colors && is_valid_color(colorcode))
		cout << "\33[" << colorcode << "m\33[K";
}

static void end_color(bool use_colors, const char *colorcode)
{
	if (use_colors && is_valid_color(colorcode))
		cout << "\33[m\33[K";
}

#define with_color(use_colors, color, code)	\
	do {					\
		start_color(use_colors, color);	\
		{				\
			code;			\
		}				\
		end_color(use_colors, color);	\
	} while (0);

static void putsn(const char *string, int from, int to)
{
	for (; from < to; from++)
		cout << (string[from]);
}

void print_context_line(const struct context *context, const struct match *match)
{
	int a = match->start;
	int b = match->end;

	while (a >= 0 && match->string[a] != '\n')
		a--;
	a++;

	while (b < match->strlen && match->string[b] != '\n')
		b++;

	line_prefix(*context->out, context->filename, context->pagenum);

	putsn(match->string, a, match->start);

	with_color(context->out->color, context->out->colors.highlight,
		putsn(match->string, match->start, match->end);
	);

	putsn(match->string, match->end, b);

	cout << endl;
}

void print_only_match(const struct context *context, const struct match *match)
{
	line_prefix(*context->out, context->filename, context->pagenum);

	with_color(context->out->color, context->out->colors.highlight,
		putsn(match->string, match->start, match->end);
	);

	cout << endl;
}

ostream& err() {
	return cerr << "pdfgrep: ";
}

std::ostream& line_prefix(const Outconf& outconf, const std::string& filename,
                          size_t page) {
	line_prefix(outconf, filename);

	if (outconf.pagenum) {
		with_color(outconf.color, outconf.colors.pagenum,
			cout << page;);
		with_color(outconf.color, outconf.colors.separator,
			cout << outconf.prefix_sep;);
	}

	return cout;
}

std::ostream& line_prefix(const Outconf& outconf, const std::string& filename) {
	if (outconf.filename) {
		with_color(outconf.color, outconf.colors.filename,
			cout << filename;);
		// Here, --null takes precedence over --match-prefix-separator
		// in the sense, that if --null is given, the null byte is
		// always printed after the filename instead of the separator.
		if (outconf.null_byte_sep) {
			cout << '\0';
		} else {
			with_color(outconf.color, outconf.colors.separator,
				cout << outconf.prefix_sep;);
		}
	}

	return cout;
}
