/***************************************************************************
 *   Copyright (C) 2017 by Christian Dietrich                              *
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
#include <errno.h>
#include <string.h>

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

// I feel so bad...
static const char *cache_directory;
static int agesort(const struct dirent ** a, const struct dirent **b) {
	std::string A = string(cache_directory) + "/" + (*a)->d_name;
	std::string B = string(cache_directory) + "/" + (*b)->d_name;

	struct stat bufa, bufb;
	if (stat(A.c_str(), &bufa) != 0) return 0;
	if (stat(B.c_str(), &bufb) != 0) return 0;

	return bufb.st_mtime - bufa.st_mtime;
}

static int agefilter(const struct dirent * a) {
	if (a->d_name[0] == '.') return false;
	std::string A = string(cache_directory) + "/" + a->d_name;
	struct stat bufa;
	if (stat(A.c_str(), &bufa) != 0) return false;

	// Filter all files that are younger than one day
	return (time(NULL) - bufa.st_mtime) > 24 * 60 * 60;
}

void limit_cachesize(const char *cache, int entries) {
	struct dirent **namelist;
	cache_directory = cache;
	int n = scandir(cache, &namelist, agefilter, agesort);
	if (n < 0) {
		return;
	} else {
		while (entries--, n--) {
			// Skip the first N cache entries
			if (entries >= 0) continue;

			string path(cache + string("/") + namelist[n]->d_name);
			unlink(path.c_str());
			free(namelist[n]);
		}
		free(namelist);
	}
}

int find_cache_directory(std::string &dir)
{
	const char *cache_base = getenv("XDG_CACHE_HOME");

	dir = "";
	if (cache_base != NULL && cache_base[0] != '\0') {
		dir += cache_base;
	} else {
		char *home = getenv("HOME");
		if (home == NULL) {
			struct passwd *passwd;
			passwd = getpwuid(getuid());
			if (passwd != NULL) {
				home = passwd->pw_dir;
			} else {
				return -1;
			}
		}
		dir += home;
		dir += "/.cache";
	}
	// according to xdg spec, all directories should be created as 0700.
	if (mkdir(dir.c_str(), 0700) && errno != EEXIST) {
		char *msg = strerror(errno);
		err() << "mkdir(" << dir << "): " << msg << endl;
		return -1;
	}

	dir += "/pdfgrep/";

	if (mkdir(dir.c_str(), 0700) && errno != EEXIST) {
		char *msg = strerror(errno);
		err() << "mkdir(" << dir << "): " << msg << endl;
		return -1;
	}
	return 0;
}