/***************************************************************************
 *   Copyright (C) 2016 by Hans-Peter Deifel                               *
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <getopt.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>
#include <fnmatch.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <vector>
#include <iostream>
#include <locale>
#include <basedir.h>
#include <rhash.h>

#include <cpp/poppler-document.h>
#include <cpp/poppler-page.h>
#include <cpp/poppler-version.h>


#ifdef HAVE_UNAC
#include <unac.h>
#endif

#include <memory>

#include "pdfgrep.h"
#include "output.h"
#include "exclude.h"
#include "regengine.h"
#include "search.h"
#include "cache.h"

using namespace std;

/* set this to 1 if any match was found. Used for the exit status */
int found_something = 0;


// Options

enum {
	HELP_OPTION,
	COLOR_OPTION,
	EXCLUDE_OPTION,
	INCLUDE_OPTION,
	PASSWORD,
	DEBUG_OPTION,
	PREFIX_SEP_OPTION,
	WARN_EMPTY_OPTION,
	UNAC_OPTION,
};

struct option long_options[] =
{
	{"ignore-case", 0, 0, 'i'},
	{"perl-regexp", 0, 0, 'P'},
	{"page-number", 0, 0, 'n'},
	{"with-filename", 0, 0, 'H'},
	{"no-filename", 0, 0, 'h'},
	{"count", 0, 0, 'c'},
	{"color", 1, 0, COLOR_OPTION},
	{"recursive", 0, 0, 'r'},
	{"dereference-recursive", 0, 0, 'R'},
	{"exclude", 1, 0, EXCLUDE_OPTION},
	{"include", 1, 0, INCLUDE_OPTION},
	{"help", 0, 0, HELP_OPTION},
	{"version", 0, 0, 'V'},
	{"page-count", 0, 0, 'p'},
	{"quiet", 0, 0, 'q'},
	{"password", 1, 0, PASSWORD},
	{"max-count", 1, 0, 'm'},
	{"debug", 0, 0, DEBUG_OPTION},
	{"only-matching", 0, 0, 'o'},
	{"null", 0, 0, 'Z'},
	{"match-prefix-separator", 1, 0, PREFIX_SEP_OPTION},
	{"warn-empty", 0, 0, WARN_EMPTY_OPTION},
	{"unac", 0, 0, UNAC_OPTION},
	{"fixed-strings", 0, 0, 'F'},
	{"cache", 0, 0, 'f'},
	{"after-context", 1, 0, 'A'},
	{"before-context", 1, 0, 'B'},
	{"context", 1, 0, 'C'},
	{0, 0, 0, 0}
};

#ifdef HAVE_UNAC
/* convenience layer over libunac. */
static char *simple_unac(const Options &opts, char *string)
{
	if (!opts.use_unac)
		return string;

	char *res = NULL;
	size_t reslen = 0;

	if (unac_string("UTF-8", string, strlen(string), &res, &reslen)) {
		perror("pdfgrep: Failed to remove accents: ");
		return strdup(string);
	}

	return res;
}
#endif

/* parses a color pair like "foo=bar" to "foo" and "bar" */
static void parse_env_color_pair(char* pair, char** name, char** value)
{
	*name = pair;
	int i = 0;
	while (pair[i] != '\0' && pair[i] != '=') {
		i++;
	}
	if (pair[i] == '=') {
		/* value starts one char after '=' */
		*value = pair+i+1;
		/* and replace '=' by '\0' */
		pair[i] = '\0';
	} else { /* if pair[i] == '\0' */
		/* let value point to the '\0' */
		*value = pair+i;
	}
}


/* set colors of output according to content of environment-varaible env_var.
   the content of env_var has to be like the GREP_COLORS variable for grep
   see man 1 grep for further details of GREP_COLORS */
static void read_colors_from_env(Colorconf &colors, const char* env_var)
{
	/* create a copy of var to edit it with strtok */
	if (!getenv(env_var)) {
		return;
	}
	char* colors_list = strdup(getenv(env_var));
	if (!colors_list) {
		/* if var is not set */
		return;
	}
	/* save original length _before_ editing it with strtok */
	int colors_list_len = strlen(colors_list);
	int i = 0;
	char* cur_color_pair;
	char *cur_name, *cur_value;
	/* read all color=value pairs */
	while (i < colors_list_len && (cur_color_pair = strtok(colors_list+i, ":"))) {
		/* set index i to next color pair */
		i += strlen(cur_color_pair); /* skip token content */
		i += 1; /* skip delemiter */
		/* get name and value of color pair */
		parse_env_color_pair(cur_color_pair, &cur_name, &cur_value);

#define PARSE_COLOR(GREP_NAME, GLOBAL_VAR) \
	if (!strcmp(cur_name, GREP_NAME)) { \
		free(GLOBAL_VAR); /* free old value */ \
		GLOBAL_VAR = strdup(cur_value); /* set to new color */ \
	}

		/* now check for known settings and set global colors */
		PARSE_COLOR("mt", colors.highlight)
		else PARSE_COLOR("ms", colors.highlight)
		else PARSE_COLOR("mc", colors.highlight)
		else PARSE_COLOR("fn", colors.filename)
		else PARSE_COLOR("ln", colors.pagenum)
		else PARSE_COLOR("se", colors.separator)

#undef PARSE_COLOR

	}
	/* free our copy of the environment var */
	free(colors_list);
}

static void set_default_colors(Colorconf &colors)
{
	// The grep(1) manpage documents the default value of GREP_COLORS to be
	// ms=01;31:mc=01;31:sl=:cx=:fn=35:ln=32:bn=32:se=36
	// which corresponds to these values below:

	colors.filename = strdup("35");
	colors.pagenum = strdup("32");
	colors.highlight = strdup("01;31");
	colors.separator = strdup("36");
}

static void init_colors(Colorconf &colors)
{
	set_default_colors(colors);
}

static void print_usage(char *self)
{
	cout << "Usage: " << self << " [OPTION]... PATTERN FILE..." << endl;
	cout << endl << "See '" << self << " --help' for more information" << endl;
}

static void print_help(char *self)
{
	cout << "Usage: " << self << " [OPTION]... PATTERN FILE..." << endl << endl
	     << "Search for PATTERN in each FILE." << endl
	     << "PATTERN is, by default, an extended regular expression." << endl
	     << endl
	     << "Options:" << endl
	     << " -i, --ignore-case\t\tIgnore case distinctions" << endl
	     << " -P, --pcre\t\t\tUse Perl compatible regular expressions (PCRE)" << endl
	     << " -H, --with-filename\t\tPrint the file name for each match" << endl
	     << " -h, --no-filename\t\tSuppress the prefixing of file name on output" << endl
	     << " -n, --page-number\t\tPrint page number with output lines" << endl
	     << " -c, --count\t\t\tPrint only a count of matches per file" << endl
	     << "     --color WHEN\t\tUse colors for highlighting;" << endl
	     << "\t\t\t\tWHEN can be `always', `never' or `auto'" << endl
	     << " -p, --page-count\t\tPrint only a count of matches per page" << endl
	     << " -m, --max-count NUM\t\tStop reading after NUM matching lines (per file)" << endl
	     << " -q, --quiet\t\t\tSuppress normal output" << endl
	     << " -r, --recursive\t\tSearch directories recursively" << endl
	     << " -R, --dereference-recursive\tLikewise, but follow all symlinks" << endl
		 << " -f, --cache\t\tUse cache for faster operation" << endl
	     << "     --help\t\t\tPrint this help" << endl
	     << " -V, --version\t\t\tShow version information" << endl;
}

static void print_version()
{
	cout << "This is " << PACKAGE << " version " << VERSION << "." << endl << endl;
	cout << "Using poppler version " << poppler::version_string().c_str() << endl;
#ifdef HAVE_UNAC
	cout << "Using unac version " << unac_version() << endl;
#endif
#ifdef HAVE_LIBPCRE
	cout << "Using libpcre version " << pcre_version() << endl;
#endif
#ifdef PDFGREP_GIT_HEAD
	cout << "Built from git-commit " << PDFGREP_GIT_HEAD << endl;
#endif
}

static bool is_dir(const string &filename)
{
	struct stat st;

	return stat(filename.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static int do_search_in_document(const Options &opts, const string &path, const string &filename,
                                 Regengine &re, bool check_excludes = true)
{
	if (check_excludes &&
	    (!is_excluded(opts.includes, filename) || is_excluded(opts.excludes, filename)))
		return 0;

	unique_ptr<Cache> cache;

	if (opts.use_cache) {
		unsigned char sha1sum[20];
		std::string cache_file(opts.cache_directory);
		if (rhash_file(RHASH_SHA1, filename.c_str(), sha1sum) != 0) {
			err() << "Could not hash " << path.c_str() << endl;
			return 1;
		}
		char translate[] = "0123456789abcdef";
		for (unsigned i = 0; i < 20; ++i) {
			cache_file += translate[sha1sum[i] & 0xf];
			cache_file += translate[(sha1sum[i] >> 4 ) & 0xf];
		}

		cache = unique_ptr<Cache>(new Cache(cache_file));
	}

	unique_ptr<poppler::document> doc;

	if (opts.passwords.empty()) {
		err() << "Internal error, password vector empty!" << endl;
		abort();
	}

	for (string password : opts.passwords) {
		// FIXME This logic doesn't seem to make sens. What if only the
		// first password is correct?
		doc = unique_ptr<poppler::document>(
			poppler::document::load_from_file(path, string(password),
							  string(password))
			);
	}

	if (!doc.get() || doc->is_locked()) {
		err() << "Could not open " << path.c_str() << endl;
		return 1;
	}

	int matches = search_document(opts, move(doc), move(cache), path, re);
	if (matches > 0) {
		found_something = 1;
		if (opts.quiet) {
			exit(EXIT_SUCCESS); // FIXME: Handle this with return value
		}
	}

	return 0;
}

static int do_search_in_directory(const Options &opts, const string &filename, Regengine &re)
{
	DIR *ptrDir = NULL;

	ptrDir = opendir(filename.c_str());
	if (!ptrDir) {
		err() << filename.c_str() << ": " << strerror(errno) << endl;
		return 1;
	}

	while(1) {
		string path(filename);
		errno = 0;
		struct dirent *ptrDirent = readdir(ptrDir);    //not sorted, in order as `ls -f`
		if (!ptrDirent)
			break;

		if (!strcmp(ptrDirent->d_name, ".") || !strcmp(ptrDirent->d_name, ".."))
			continue;

		path += "/";
		path += ptrDirent->d_name;

		struct stat st;
		int statret;

		if (opts.recursive == Recursion::FOLLOW_SYMLINKS)
			statret = stat(path.c_str(), &st);
		else
			statret = lstat(path.c_str(), &st);

		if (statret) {
			err() << filename.c_str() << ": " << strerror(errno) << endl;
			continue;
		}

		if (S_ISLNK(st.st_mode))
			continue;

		if (S_ISDIR(st.st_mode)) {
			do_search_in_directory(opts, path, re);
		} else {
			do_search_in_document(opts, path, ptrDirent->d_name, re);
		}
	}

	closedir(ptrDir);

	return 0;
}

static bool parse_int(const char *str, int *i)
{
	char *endptr;
	errno = 0;
	unsigned long int d = strtoul(str, &endptr, 10);
	if (errno != 0 || (long)d > INT_MAX || (long)d < INT_MIN
	    || *endptr != '\0') {
		return false;
	}

	*i = d;
	return true;
}

#if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 29
static void handle_poppler_errors(const string &msg, void *_opts)
{
	Options *opts = static_cast<Options*>(_opts);
	if (opts->debug) {
		err() << msg << endl;
	}
}
#endif

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

static void limit_cachesize(const char *cache, int entries) {
	struct dirent **namelist;
	cache_directory = cache;
	unsigned n = scandir(cache, &namelist, agefilter, agesort);
	if (n < 0) {
		return;
	} else {
		while (entries--, n--) {
			// Skip the first N cache entries
			if (entries > 0) continue;

			string path(cache + string("/") + namelist[n]->d_name);
			unlink(path.c_str());
			free(namelist[n]);
		}
		free(namelist);
	}
}

int main(int argc, char** argv)
{
	Options options;
	init_colors(options.outconf.colors);

	// Set locale to user-preference. If this locale is an UTF-8 locale, the
	// regex-functions regcomp/regexec become unicode aware, which means
	// e.g. that '.' will match a unicode character, not a single byte.
	locale::global(locale(""));

	enum re_engine_type {
		RE_POSIX = 0,
		RE_PCRE = 1,
		RE_FIXED = 2
	};

	int re_engine = RE_POSIX;

	// either -H or -h was set
	bool explicit_filename_option = false;

	enum {
		COLOR_ALWAYS,
		COLOR_AUTO,
		COLOR_NEVER
	} use_colors = COLOR_AUTO;

	while (1) {
		int c = getopt_long(argc, argv, "icA:B:C:nrRhHVPpqm:FfoZ",
				long_options, NULL);

		if (c == -1)
			break;

		switch (c) {
			case HELP_OPTION:
				print_help(argv[0]);
				exit(EXIT_SUCCESS);
			case 'V':
				print_version();
				exit(EXIT_SUCCESS);
			case 'n':
				options.outconf.pagenum = true;
				break;
			case 'r':
				options.recursive = Recursion::DONT_FOLLOW_SYMLINKS;
				break;
			case 'R':
				options.recursive = Recursion::FOLLOW_SYMLINKS;
				break;
			case 'h':
				options.outconf.filename = false;
				explicit_filename_option = true;
				break;
			case 'H':
				options.outconf.filename = true;
				explicit_filename_option = true;
				break;
			case 'i':
				options.ignore_case = true;
				break;
			case 'c':
				options.count = true;
				break;
			case COLOR_OPTION:
				if (!strcmp("always", optarg)) {
					use_colors = COLOR_ALWAYS;
				} else if (!strcmp("never", optarg)) {
					use_colors = COLOR_NEVER;
				} else if (!strcmp("auto", optarg)) {
					use_colors = COLOR_AUTO;
				} else {
					err() << "Invalid argument '" << optarg << "' for --color. "
					      << "Candidates are: always, never or auto" << endl;
					exit(EXIT_ERROR);
				}
				break;
			case EXCLUDE_OPTION:
				exclude_add(options.excludes, optarg);
				break;
			case INCLUDE_OPTION:
				exclude_add(options.includes, optarg);
				break;
			case 'P':
#ifndef HAVE_LIBPCRE
				err() << "PCRE support disabled at compile time!" << endl;
				exit(EXIT_ERROR);
#else
				re_engine |= RE_PCRE;
#endif
				break;
			case 'p':
				options.pagecount = true;
				options.outconf.pagenum = true;
				break;

			case 'q':
				options.quiet = true;
				break;

			case PASSWORD:
				options.passwords.push_back(string(optarg));
				break;

			case 'm':
				if (!parse_int(optarg, &options.max_count)) {
					err() << "Could not parse number: " << optarg << "." << endl;
					exit(EXIT_ERROR);
				} else if (options.max_count <= 0) {
					err() << "--max-count must be positive." << endl;
					exit(EXIT_ERROR);
				}
				break;
			case DEBUG_OPTION:
				options.debug = true;
				break;

			case UNAC_OPTION:
#ifndef HAVE_UNAC
				err() << "UNAC support disabled at compile time!" << endl;
				exit(EXIT_ERROR);
#else
				options.use_unac = true;
#endif
				break;
			case 'F':
				re_engine |= RE_FIXED;
				break;

			case 'f':
				options.use_cache = true;
				break;

			case 'o':
				options.outconf.only_matching = true;
				break;

			case 'Z': // --null
				options.outconf.null_byte_sep = true;
				break;

			case PREFIX_SEP_OPTION:
				options.outconf.prefix_sep = string(optarg);
				break;

			case WARN_EMPTY_OPTION:
				options.warn_empty = true;
				break;

			case 'A':
				if (!parse_int(optarg, &options.outconf.context_after)) {
					err() << "Could not parse number: " << optarg << "." << endl;
					exit(EXIT_ERROR);
				} else if (options.outconf.context_after < 0) {
					err() << "--after-context must be positive." << endl;
					exit(EXIT_ERROR);
				}
				options.outconf.context_mode = true;
				break;

			case 'B':
				if (!parse_int(optarg, &options.outconf.context_before)) {
					err() << "Could not parse number: " << optarg << "." << endl;
					exit(EXIT_ERROR);
				} else if (options.outconf.context_before < 0) {
					err() << "--before-context must be positive." << endl;
					exit(EXIT_ERROR);
				}
				options.outconf.context_mode = true;
				break;

			case 'C':
				if (!parse_int(optarg, &options.outconf.context_after)) {
					err() << "Could not parse number: " << optarg << "." << endl;
					exit(EXIT_ERROR);
				} else if (options.outconf.context_after < 0) {
					err() << "--after-context must be positive." << endl;
					exit(EXIT_ERROR);
				}
				options.outconf.context_before = options.outconf.context_after;
				options.outconf.context_mode = true;
				break;

			/* In these two cases, getopt already prints an
			 * error message
			 */
			case '?': // unknown option
			case ':': // missing argument
				exit(EXIT_ERROR);
				break;
			default:
				break;
		}
	}

	if (argc == optind || (argc - optind < 2 && options.recursive == Recursion::NONE)) {
		print_usage(argv[0]);
		exit(EXIT_ERROR);
	}

	char *pattern = argv[optind++];
#ifdef HAVE_UNAC
	pattern = simple_unac(options, pattern);
#endif

	unique_ptr<Regengine> re;
	if (re_engine == (RE_FIXED | RE_PCRE)) {
		err() << "--pcre and --fixed cannot be used together" << endl;
		exit(EXIT_ERROR);
	}
#ifdef HAVE_LIBPCRE
	if (re_engine == RE_PCRE) {
		re.reset(new PCRERegex(pattern, options.ignore_case));
	} else
#endif // HAVE_LIBPCRE
	if (re_engine == RE_FIXED) {
		re.reset(new FixedString(pattern, options.ignore_case));
	} else {
		re.reset(new PosixRegex(pattern, options.ignore_case));
	}

#if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 29
	// set poppler error output function
	poppler::set_debug_error_function(handle_poppler_errors, &options);
#endif

	bool color_tty = isatty(STDOUT_FILENO) && getenv("TERM") &&
		strcmp(getenv("TERM"), "dumb");

	options.outconf.color =
		use_colors == COLOR_ALWAYS
		|| (use_colors == COLOR_AUTO && color_tty);

	if (options.outconf.color)
		read_colors_from_env(options.outconf.colors, "GREP_COLORS");

	if (explicit_filename_option == false) {
		if ((argc - optind) == 1 && !is_dir(argv[optind])) {
			options.outconf.filename = false;
		} else
			options.outconf.filename = true;
	}

	if (options.outconf.only_matching && (options.outconf.context_before > 0
					      || options.outconf.context_after > 0)) {
		err() << "warning: --only-matching and context options can't be used together."
		      << " Ignoring context option." << endl;

		options.outconf.context_mode = false;
	}

	if (excludes_empty(options.includes))
		exclude_add(options.includes, "*.pdf");

	// If no password has been specified on the command line, insert the
	// empty string aka "no password" into the passwords array.
	if (options.passwords.empty()) {
		options.passwords.push_back("");
	}

	if (options.use_cache) {
		xdgHandle xdg_handle;
		if (!xdgInitHandle(&xdg_handle)) {
			err() << "warning: Could not initialize XDG handle;"
				  << " no cache is used!" << endl;
			options.use_cache = false;
		}
		const char *cache_base_directory = xdgCacheHome(&xdg_handle);
		options.cache_directory = string(cache_base_directory)
			+ "/pdfgrep/";

		// Make base directory
		mkdir(cache_base_directory, 0755);
		mkdir(options.cache_directory.c_str(), 0755);

		limit_cachesize(options.cache_directory.c_str(), 200);
	}

	int error = 0;

	for (int i = optind; i < argc; i++) {
		const string filename(argv[i]);

		if (!is_dir(filename)) {
			if (do_search_in_document(options, filename, filename, *re, false)) {
				error = 1;
			}
		} else if (options.recursive != Recursion::NONE) {
			if (do_search_in_directory(options, filename, *re)) {
				error = 1;
			}
		} else {
			err() << filename << " is a directory" << endl;
			error = 1;
		}
	}

	if (argc == optind && options.recursive != Recursion::NONE) {
		do_search_in_directory(options, ".", *re);
	}

	if (error) {
		exit(EXIT_ERROR);
	} else if (found_something) {
		exit(EXIT_SUCCESS);
	} else {
		exit(EXIT_NOT_FOUND);
	}
}

/* vim: set noet: */
