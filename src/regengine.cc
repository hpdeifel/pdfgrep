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

#include "regengine.h"

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "output.h"
#include "pdfgrep.h"

// regex(3)

PosixRegex::PosixRegex(const char *pattern, bool case_insensitive)
{
	int regex_flags = REG_EXTENDED | (case_insensitive ? REG_ICASE : 0);

	int err = regcomp(&this->regex, pattern, regex_flags);
	if(err) {
		char err_msg[256];
		regerror(err, &this->regex, err_msg, 256);
		fprintf(stderr, "pdfgrep: %s\n", err_msg);
		exit(EXIT_ERROR);
	}
}

int PosixRegex::exec(const char *str, size_t offset, struct match *m)
{
	regmatch_t match[] = {{0, 0}};
	const int nmatch = 1;

	int ret = regexec(&this->regex, str + offset, nmatch, match, 0);

	if(ret) {
		return ret;
	}

	m->start = offset + match[0].rm_so;
	m->end = offset + match[0].rm_eo;

	return 0;
}

PosixRegex::~PosixRegex()
{
	regfree(&this->regex);
}


// pcre(3)

#ifdef HAVE_LIBPCRE

PCRERegex::PCRERegex(const char *pattern, bool case_insensitive)
{
	const char *pcre_err;
	int pcre_err_ofs;
	const int pcre_options = case_insensitive ? PCRE_CASELESS : 0;

	this->regex = pcre_compile(pattern, pcre_options,
	                           &pcre_err, &pcre_err_ofs, NULL);

	if (this->regex == NULL) {
		fprintf(stderr, "pdfgrep: %s\n", pattern);
		fprintf(stderr, "pdfgrep: %*s\n", pcre_err_ofs + 1, "^");
		fprintf(stderr, "pdfgrep: Error compiling PCRE pattern: %s\n", pcre_err);
		exit(EXIT_ERROR);
	}
}

PCRERegex::~PCRERegex()
{
	pcre_free(this->regex);
}

int PCRERegex::exec(const char *str, size_t offset, struct match *m)
{
	const size_t len = strlen(str);
	int ov[3];

	const int ret = pcre_exec(this->regex, NULL, str, len, offset,
	                          PCRE_NOTEMPTY, ov, 3);

	// TODO: Print human readable error
	if(ret < 0)
		return 1;

	m->start = ov[0];
	m->end = ov[1];

	return 0;
}

#endif // HAVE_LIBPCRE

FixedString::FixedString(char *pattern, bool case_insensitive)
	: case_insensitive(case_insensitive)
{
	// split pattern at newlines
	const char *token = strtok(pattern, "\n");
	while (token != NULL) {
		patterns.push_back(token);
		token = strtok(NULL, "\n");
	}
}

int FixedString::exec(const char *str, size_t offset, struct match *m)
{
	for (const char* pattern : patterns) {
		const char *result;
		if (this->case_insensitive) {
			result = strcasestr(str+offset, pattern);
		} else {
			result = strstr(str+offset, pattern);
		}

		if (result == NULL) {
			continue;
		}

		m->start = offset + (result - (str + offset));
		m->end = m->start + strlen(pattern);
		return 0;
	}

	return 1;
}
