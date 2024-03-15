/***************************************************************************
 *   Copyright (C) 2015-2023 by Hans-Peter Deifel                          *
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

#include "output.h"
#include "pdfgrep.h"

using namespace std;

bool PatternList::exec(const string &str, size_t offset, struct match &m) const
{
	struct match m_copy = m;

	for (auto &r : patterns) {
		if (r->exec(str, offset, m_copy)) {
			m.start = m_copy.start;
			m.end = m_copy.end;
			return true;
		}
	}
	return false;
}


void PatternList::add_pattern(unique_ptr<Regengine> pattern) {
	patterns.push_back(move(pattern));
}

// regex(3)

PosixRegex::PosixRegex(const string &pattern, bool case_insensitive)
{
	int regex_flags = REG_EXTENDED | (case_insensitive ? REG_ICASE : 0);

	// The regcomp/regexec implementation of OpenBSD doesn't like empty
	// patterns. Thus we just replace empty patterns by "()", which does
	// have the same semantics.
	const char *c_str_pattern;
	if (pattern.empty()) {
		c_str_pattern = "()";
	} else {
		c_str_pattern = pattern.c_str();
	}

	int ret = regcomp(&this->regex, c_str_pattern, regex_flags);
	if (ret != 0) {
		char err_msg[256];
		regerror(ret, &this->regex, err_msg, 256);
		err() << err_msg << endl;
		exit(EXIT_ERROR);
	}
}

bool PosixRegex::exec(const string &str, size_t offset, struct match &m) const
{
	regmatch_t match[] = {{0, 0}};
	const int nmatch = 1;

	// If we aren't at the beginning of the page, ^ should not match.
	int flags = offset == 0 ? 0 : REG_NOTBOL;

	int ret = regexec(&this->regex, &str[offset], nmatch, match, flags);

	if (ret != 0) {
		return false;
	}

	m.start = offset + match[0].rm_so;
	m.end = offset + match[0].rm_eo;

	return true;
}

PosixRegex::~PosixRegex()
{
	regfree(&this->regex);
}


// pcre2(3)

#ifdef HAVE_LIBPCRE

PCRERegex::PCRERegex(const string &pattern, bool case_insensitive)
{
	int pcre_err;
	PCRE2_SIZE pcre_err_ofs;
	const uint32_t pcre_options = PCRE2_UTF | (case_insensitive ? PCRE2_CASELESS : 0);

	this->regex = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()), pattern.size(),
				    pcre_options, &pcre_err, &pcre_err_ofs, nullptr);

	if (this->regex == nullptr) {
		PCRE2_UCHAR message[128]; // Actual size unknowable, longer messages get truncated
		pcre2_get_error_message(pcre_err, message, sizeof message / sizeof *message);
		err() << pattern << endl;
		err() << setw(pcre_err_ofs+1) << "^" << endl;
		err() << "Error compiling PCRE pattern: " << message << endl;
		exit(EXIT_ERROR);
	}
}

PCRERegex::~PCRERegex()
{
	pcre2_code_free(this->regex);
}

bool PCRERegex::exec(const string &str, size_t offset, struct match &m) const
{
	pcre2_match_data *data;
	PCRE2_SIZE *ov;

	data = pcre2_match_data_create(1, nullptr);
	const int ret = pcre2_match(this->regex,
				    reinterpret_cast<PCRE2_SPTR>(str.data()), str.size(),
				    offset, 0, data, nullptr);

	// TODO: Print human readable error
	if (ret < 0 || pcre2_get_ovector_count(data) != 1) {
		pcre2_match_data_free(data);
		return false;
	}

	ov = pcre2_get_ovector_pointer(data);
	m.start = ov[0];
	m.end = ov[1];

	pcre2_match_data_free(data);
	return true;
}

#endif // HAVE_LIBPCRE

FixedString::FixedString(const string &pattern, bool case_insensitive)
	: case_insensitive(case_insensitive)
{
	istringstream str { pattern };
	string line;

	if (pattern.empty()) {
		// special case for the empty pattern. In this case we _do_ want
		// matches, but getline returns false leaving our patterns array
		// empty. Thus we add the whole pattern explicitly.
		patterns.push_back(pattern);
		return;
	}

	// split pattern at newlines
	while (getline(str, line)) {
		patterns.push_back(line);
	}
}

bool FixedString::exec(const string &str, size_t offset, struct match &m) const
{
	// We use C-style strings here, because of strcasestr
	const char *str_begin = &str[offset];

	// FIXME Searching for multiple patterns is very inefficient, because we
	// search the same thing over and over, until it becomes the next match.
	// We should introduce some kind of caching here

	const char *min_result = nullptr;
	const string *min_pattern;

	for (const string &pattern : patterns) {
		const char *result;
		if (this->case_insensitive) {
			result = strcasestr(str_begin, pattern.c_str());
		} else {
			result = strstr(str_begin, pattern.c_str());
		}

		if (result != nullptr) {
			if (min_result == nullptr || result < min_result) {
				min_result = result;
				min_pattern = &pattern;
			}
		}
	}

	if (min_result != nullptr) {
		m.start = offset + (min_result - str_begin);
		m.end = m.start + (*min_pattern).size();
		return true;
	}

	return false;
}
