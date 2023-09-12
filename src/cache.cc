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

#include "cache.h"
#include "output.h"

#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

using namespace std;

const char *CACHE_VERSION = "1";

static std::ostream& operator<<(std::ostream& out, const CachePage& page) {
	out << page.label << '\0';
	out << page.text << '\0';
	return out;
}

static std::istream& operator>>(std::istream& in, CachePage& page) {
	std::getline(in, page.label, '\0');
	std::getline(in, page.text, '\0');
	return in;
}

Cache::Cache(string const &cache_file)
	: cache_file(cache_file), valid(false) {
	// Open the cache file
	ifstream fd(cache_file);
	if (!fd) {
		return;
	}

	unsigned char indicator;
	fd >> indicator;
	if (indicator != 'C') {
		return;
	}
	std::string version;
	std::getline(fd, version, '\0');
	if (version != CACHE_VERSION) {
		return;
	}

	for (CachePage page; fd >> page; ) {
		pages.push_back(page);
	}
	valid = true;
}

void Cache::set_page(unsigned pagenum, const CachePage& page) {
	pages.resize(max(pagenum, (unsigned)pages.size()));
	pages[pagenum-1] = page;
}

bool Cache::get_page(unsigned pagenum, CachePage& page) {
	if (!valid) {
		return false;
	}
	if (pagenum-1 < pages.size()) {
		page = pages[pagenum-1];
		return true;
	}
	return false;
}

void Cache::dump() {
	ofstream fd(cache_file);
	if (!fd) {
		return;
	}
	// The first byte of the cache file is an indicator byte, which can be
	// written atomically. Initial it is \0, after all pages have been
	// flushed, the indicator byte is written to 'C' (complete cache).
	fd << '\0';
	fd << CACHE_VERSION << '\0';

	for (const CachePage& page : pages) {
		fd << page;
	}

	fd.flush();
	fd.seekp(0, ios_base::beg);
	fd << 'C';
	fd.close();
}

// I feel so bad...
static const char *cache_directory;
static int agesort(const struct dirent ** a, const struct dirent **b) {
	std::string A = string(cache_directory) + "/" + (*a)->d_name;
	std::string B = string(cache_directory) + "/" + (*b)->d_name;

	struct stat bufa, bufb;
	if (stat(A.c_str(), &bufa) != 0) {
		return 0;
	}
	if (stat(B.c_str(), &bufb) != 0) {
		return 0;
	}

	return bufb.st_mtime - bufa.st_mtime;
}

static int agefilter(const struct dirent * a) {
	if (a->d_name[0] == '.') {
		return 0;
	}
	std::string A = string(cache_directory) + "/" + a->d_name;
	struct stat bufa;
	if (stat(A.c_str(), &bufa) != 0) {
		return 0;
	}

	// Filter all files that are younger than one day
	if ((time(nullptr) - bufa.st_mtime) > 24 * 60 * 60) {
		return 1;
	}

	return 0;
}

void limit_cachesize(const char *cache, int entries) {
	struct dirent **namelist;
	cache_directory = cache;
	int n = scandir(cache, &namelist, agefilter, agesort);

	if (n < 0) {
		return;
	}

	while (entries--, n-- > 0) {
		// Skip the first N cache entries
		if (entries >= 0) {
			continue;
		}

		string path(cache + string("/") + namelist[n]->d_name);
		unlink(path.c_str());
		free(namelist[n]);
	}
	free(namelist);
}

int find_cache_directory(std::string &dir)
{
	const char *cache_base = getenv("XDG_CACHE_HOME");

	dir = "";
	if (cache_base != nullptr && cache_base[0] != '\0') {
		dir += cache_base;
	} else {
		char *home = getenv("HOME");
		if (home == nullptr) {
			struct passwd *passwd;
			passwd = getpwuid(getuid());
			if (passwd != nullptr) {
				home = passwd->pw_dir;
			} else {
				return -1;
			}
		}
		dir += home;
		dir += "/.cache";
	}
	// according to xdg spec, all directories should be created as 0700.
	if (mkdir(dir.c_str(), 0700) != 0 && errno != EEXIST) {
		char *msg = strerror(errno);
		err() << "mkdir(" << dir << "): " << msg << endl;
		return -1;
	}

	dir += "/pdfgrep/";

	if (mkdir(dir.c_str(), 0700) != 0 && errno != EEXIST) {
		char *msg = strerror(errno);
		err() << "mkdir(" << dir << "): " << msg << endl;
		return -1;
	}
	return 0;
}
