/***************************************************************************
 *   Copyright (C) 2016 by Christian Dietrich                              *
 *   stettberger@dokucode.de                                               *
 *   Copyright (C) 2017-2023 by Hans-Peter Deifel <hpd@hpdeifel.de>        *
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

#ifndef CACHE_H
#define CACHE_H

#include <vector>
#include <string>

struct CachePage {
	std::string text;
	std::string label;
};

class Cache {
	std::vector<CachePage> pages;
	std::string cache_file;
	bool valid;
public:
	explicit Cache(std::string const& cache_file);

	bool get_page(unsigned pagenum, CachePage& text);
	void set_page(unsigned pagenum, const CachePage& page);

	void dump();
};

void limit_cachesize(const char *cache, int entries);

/** Write cache directory to dir.
 *
 * Returns -1 on failure and 0 on success
 */
int find_cache_directory(std::string &dir);

#endif

/* Local Variables: */
/* mode: c++ */
/* End: */
