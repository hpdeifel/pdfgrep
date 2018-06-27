/***************************************************************************
 *   Copyright (C) 2011-2018 by Hans-Peter Deifel                          *
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

#include <cstdio>
#include <cstring>
#include <cctype>
#include <iostream>

using namespace std;

static bool is_valid_color(const char* colorcode) {
	return colorcode != nullptr && strcmp(colorcode, "") != 0;
}

static void start_color(bool use_colors, const char *colorcode)
{
	if (use_colors && is_valid_color(colorcode)) {
		cout << "\33[" << colorcode << "m\33[K";
	}
}

static void end_color(bool use_colors, const char *colorcode)
{
	if (use_colors && is_valid_color(colorcode)) {
		cout << "\33[m\33[K";
	}
}

#define with_color(use_colors, color, code)	\
	do {					\
		start_color(use_colors, color);	\
		{				\
			code;			\
		}				\
		end_color(use_colors, color);	\
	} while (0);

static void putsn(const string &str, int from, int to)
{
	for (; from < to; from++) {
		cout << (str[from]);
	}
}

void print_only_match(const struct context &context, const struct match &match)
{
	line_prefix(context.out, context.filename, false, context.pagenum);

	with_color(context.out.color, context.out.colors.highlight,
		putsn(match.string, match.start, match.end);
	);

	cout << endl;
}

ostream& err() {
	return cerr << "pdfgrep: ";
}

std::ostream& line_prefix(const Outconf& outconf, const std::string& filename,
                          bool in_context, size_t page) {
	line_prefix(outconf, filename, in_context);

	if (outconf.pagenum) {
		with_color(outconf.color, outconf.colors.pagenum,
			cout << page;);
		with_color(outconf.color, outconf.colors.separator,
			cout << (in_context ? "-" : outconf.prefix_sep););
	}

	return cout;
}

void print_only_filename(const Outconf& outconf, const std::string& filename) {
	with_color(outconf.color, outconf.colors.filename,
		   cout << filename;);

	if (outconf.null_byte_sep) {
		cout << '\0';
	} else {
		cout << endl;
	}
}

std::ostream& line_prefix(const Outconf& outconf, const std::string& filename,
                          bool in_context) {
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
				cout << (in_context ? "-" : outconf.prefix_sep ););
		}
	}

	return cout;
}


// Invariant: matches can't be empty
void print_matches(const context& context, const std::vector<match>& matches) {
	const match& first_match = matches.front();
	const match& last_match = matches.back();

	auto str = first_match.string;

	auto a = str.rfind('\n', first_match.start);
	auto b = str.find('\n', last_match.end);

	// If a == -1, a gets 0 (beginning of string) and if it's a valid index
	// to a newline, it now points to the character after that.
	a++;

	if (b == string::npos) {
		b = str.size();
	}

	line_prefix(context.out, context.filename, false, context.pagenum);

	int previous_end = a;
	for (auto match : matches) {
		putsn(str, previous_end, match.start);

		with_color(context.out.color, context.out.colors.highlight,
			putsn(str, match.start, match.end);
		);

		previous_end = match.end;
	}

	putsn(str, previous_end, b);

	cout << endl;
}

void print_context_before(const context& context, const match& match, int lines) {
	if (!context.out.context_mode) {
		return;
	}

	if (lines < 0) {
		lines = context.out.context_before;
	}

	auto str = match.string;
	auto line_begin = str.rfind('\n', match.start);

	// we are at the first line
	if (line_begin == string::npos) {
		return;
	}

	vector<string> lines_to_output;

	auto pos = line_begin;
	while (lines --> 0) {
		if (pos == 0) {
			lines_to_output.emplace_back("");
			break;
		}
		auto newpos = str.rfind('\n', pos-1);

		auto start_pos = newpos == string::npos ? 0 : newpos + 1;
		lines_to_output.push_back(str.substr(start_pos, pos-start_pos));

		if (newpos == string::npos) {
			break;
		}

		pos = newpos;
	}

	for (auto l = lines_to_output.rbegin(); l != lines_to_output.rend(); ++l) {
		line_prefix(context.out, context.filename, true, context.pagenum) << *l << endl;
	}
}

void print_context_after(const context& context, const match& match, int lines) {
	if (!context.out.context_mode) {
		return;
	}

	if (lines < 0) {
		lines = context.out.context_after;
	}

	auto str = match.string;
	auto line_end = str.find('\n', match.end);

	// we are at the first line
	if (line_end == string::npos) {
		return;
	}

	auto pos = line_end;
	while (lines --> 0) {
		if (pos == str.size()-1) {
			break;
		}
		auto newpos = str.find('\n', pos+1);

		auto end_pos = newpos == string::npos ? str.size() : newpos;
		line_prefix(context.out, context.filename, true, context.pagenum)
			<< str.substr(pos+1, end_pos-pos-1) << endl;

		if (newpos == string::npos) {
			break;
		}

		pos = newpos;
	}

}

void print_context_between(const context& context, const match& match1, const match& match2) {
	if (!context.out.context_mode) {
		return;
	}

	auto str = match1.string;

	auto pos_right = str.find('\n', match1.end);
	auto pos_left = str.rfind('\n', match2.start);

	// count the lines that we have to the right of match1
	int lines_right = 0;
	while (pos_right != string::npos && lines_right < context.out.context_after
		&& pos_right < pos_left) {
		lines_right++;
		pos_right = str.find('\n', pos_right+1);
	}

	// count the lines that we have to the left of match2
	int lines_left = 0;
	while (pos_left != string::npos && lines_left < context.out.context_before
	       && pos_left > pos_right) {
		lines_left++;
		if (pos_left == 0) {
			pos_left = string::npos;
			break;
		}
		pos_left = str.rfind('\n', pos_left-1);
	}

	print_context_after(context, match1, lines_right);
	if (pos_left > pos_right) {
		print_context_separator(context.out);
	}
	print_context_before(context, match2, lines_left);
}

void print_context_separator(const Outconf &out) {
	// TODO Add color here

	if (out.context_mode) {
		cout << "--" << endl;
	}
}
