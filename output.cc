/***************************************************************************
 *   Copyright (C) 2015 by Hans-Peter Deifel                               *
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
 *   Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
 *   Boston, MA 02110-1301 USA.                                            *
 ***************************************************************************/

#include "output.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

bool is_valid_color(const char* colorcode) {
	return colorcode && strcmp(colorcode, "");
}

void start_color(bool use_colors, const char *colorcode)
{
	if (use_colors && is_valid_color(colorcode))
		printf("\33[%sm\33[K", colorcode);
}

void end_color(bool use_colors, const char *colorcode)
{
	if (use_colors && is_valid_color(colorcode))
		printf("\33[m\33[K");
}

#define with_color(use_colors, color, code)	\
	do {					\
		start_color(use_colors, color);	\
		{				\
			code;			\
		}				\
		end_color(use_colors, color);	\
	} while (0);

void print_line_prefix(const struct outconf *conf, const char *filename, const int pagenum)
{
	if (filename && conf->filename) {
		with_color(conf->color, conf->colors.filename,
			printf("%s", filename););
		with_color(conf->color, conf->colors.separator, printf(":"););
	}
	if (pagenum >= 0 && conf->pagenum) {
		with_color(conf->color, conf->colors.pagenum,
			printf("%d", pagenum););
		with_color(conf->color, conf->colors.separator, printf(":"););
	}
}

void putsn(char *string, int from, int to)
{
	for (; from < to; from++) {
		char c = string[from];
		if (isprint(c))
			putchar(c);
	}
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

	if (index >= buflen || string[index] == '\n')
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

void print_context_chars(const struct context *context, const struct match *match)
{
	int a = match->start;
	int b = match->end;
	int chars_left = context->before;

	int left = next_word_left(match->string, a-1);
	int right = next_word_right(match->string, b, match->strlen);

	while (true) {
		if ((left < 0 || left > chars_left)
			&& (right < 0 || right > chars_left))
			break;

		if (right < 0 || right > chars_left ||
				(left > 0 && left <= chars_left
				 && (match->start - a) + left
					< (b - match->end) + right)) {
			a -= left;
			chars_left -= left;
			left = next_word_left(match->string,a-1);
		} else {
			b += right;
			chars_left -= right;
			right = next_word_right(match->string, b, match->strlen);
		}
	}

	print_line_prefix(context->out, context->filename, context->pagenum);

	putsn(match->string, a, match->start);

	with_color(context->out->color, context->out->colors.highlight,
		putsn(match->string, match->start, match->end);
	);

	putsn(match->string, match->end, b);
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

	print_line_prefix(context->out, context->filename, context->pagenum);

	putsn(match->string, a, match->start);

	with_color(context->out->color, context->out->colors.highlight,
		putsn(match->string, match->start, match->end);
	);

	putsn(match->string, match->end, b);
}

void print_only_match(const struct context *context, const struct match *match)
{
	print_line_prefix(context->out, context->filename, context->pagenum);

	with_color(context->out->color, context->out->colors.highlight,
		putsn(match->string, match->start, match->end);
	);
}
