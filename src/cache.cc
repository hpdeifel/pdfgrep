/***************************************************************************
 *   Copyright (C) 2016 by Christian Dietrich                              *
 *   stettberger@dokucode.de                                               *
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

#include "cache.h"

#include <fstream>
#include <iostream>

using namespace std;

Cache::Cache(string cache_file)
	: cache_file(cache_file), valid(false) {
	// Open the cache file
	ifstream fd(cache_file);
	if (!fd) return;

	unsigned char indicator;
	fd >> indicator;
	if (indicator != 'G') return;
	for (std::string page; std::getline(fd, page, '\0'); ) {
        pages.push_back(page);
    }
	valid = true;
}

void Cache::set_page(unsigned pagenum, const string &text) {
	pages.resize(max(pagenum, (unsigned)pages.size()));
	pages[pagenum-1] = text;
}

bool Cache::get_page(unsigned pagenum, string &text) {
	if (!valid) return false;
	if (pagenum-1 < pages.size()) {
		text = pages[pagenum-1];
		return true;
	}
	return false;
}

void Cache::dump() {
	ofstream fd(cache_file);
	if (!fd) return;
	// The first byte of the cache file is an indicator byte, which
	// can be written atomically. Initial it is \0, after all pages
	// have been flushed, the indicator byte is written to 'G' (for
	// good).
	fd << '\0';
	for (const string &page : pages) {
		fd << page;
		fd << '\0';
	}
	fd.flush();
	fd.seekp(0, ios_base::beg);
	fd << 'G';
	fd.close();
}
