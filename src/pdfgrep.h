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

#ifndef PDFGREP_H
#define PDFGREP_H

#include "config.h"
#include "exclude.h"

#include <vector>
#include <string>

/* Exit codes. */
enum {
	/* EXIT_SUCCESS = 0 */

	// no match was found
	EXIT_NOT_FOUND = 1,
	// an error occured
	EXIT_ERROR = 2,
};

enum class Recursion {
	NONE,
	FOLLOW_SYMLINKS,
	DONT_FOLLOW_SYMLINKS
};

enum class ContextMode {
	WHOLE_LINE,
	TERMINAL_WIDTH,
        // Meh, sum types would be so good
	FIXED
};

struct Colorconf {
	char *filename;
	char *pagenum;
	char *highlight;
	char *separator;
};

// Controls, what to print
struct Outconf {
	bool filename = false;
	bool pagenum = false;
	bool color = false;
	bool only_matching = false;
	bool null_byte_sep = false;
	std::string prefix_sep = ":";

	Colorconf colors;
};


struct Options {
	bool ignore_case = false;
	Recursion recursive = Recursion::NONE;
	ContextMode context_mode = ContextMode::TERMINAL_WIDTH;
	// Only used when context_mode is ContextMode::FIXED
	int context_chars = 0;
	int line_width = 80;
	bool count = false;
	bool pagecount = false;
	bool quiet = false;
	// vector of all passwords to try on any pdf
	std::vector<std::string> passwords;
	int max_count = 0;
	bool debug = 0;
	bool warn_empty = false;
#ifdef HAVE_UNAC
	bool use_unac = false;
#endif
	Outconf outconf;
	ExcludeList excludes;
	ExcludeList includes;
};

#endif /* PDFGREP_H */
