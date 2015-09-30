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

#ifndef OUTPUT_H
#define OUTPUT_H

#include <sys/types.h>
#include <string>
#include "pdfgrep.h"

struct context {
	char *filename;
	int pagenum;

	const Outconf *out;
};

struct match {
	const char *string;
	size_t   strlen;
	int   start;
	int   end;
};

void print_context_line(const struct context *context, const struct match *match);

/* print the line prefix followed only by the match */
void print_only_match(const struct context *context, const struct match *match);

// C++ interface:

// Print to stderr, with "pdfgrep: " as prefix;
std::ostream& err();

std::ostream& line_prefix(const Outconf& outconf, const std::string& filename,
                          size_t page);
std::ostream& line_prefix(const Outconf& outconf, const std::string& filename);

#endif
