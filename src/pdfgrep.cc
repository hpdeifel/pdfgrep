/***************************************************************************
 *   Copyright (C) 2010-2023 by Hans-Peter Deifel                          *
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <sys/types.h>
#include <getopt.h>
#include <cstdarg>
#include <unistd.h>
#include <cmath>
#include <fnmatch.h>
#include <cerrno>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <climits>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <locale>
#include <gcrypt.h>

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
#include "intervals.h"

using namespace std;

/* set this to 1 if any match was found. Used for the exit status */
bool found_something = false;


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
	CACHE_OPTION,
	PAGE_RANGE_OPTION,
	PAGENUM_OPTION,
};

struct option long_options[] =
{
	// name, has_arg, *flag, val
	{"ignore-case", no_argument, nullptr, 'i'},
	{"perl-regexp", no_argument, nullptr, 'P'},
	{"page-number", optional_argument, nullptr, PAGENUM_OPTION},
	{"with-filename", no_argument, nullptr, 'H'},
	{"no-filename", no_argument, nullptr, 'h'},
	{"count", no_argument, nullptr, 'c'},
	{"color", required_argument, nullptr, COLOR_OPTION},
	{"recursive", no_argument, nullptr, 'r'},
	{"dereference-recursive", no_argument, nullptr, 'R'},
	{"exclude", required_argument, nullptr, EXCLUDE_OPTION},
	{"include", required_argument, nullptr, INCLUDE_OPTION},
	{"help", no_argument, nullptr, HELP_OPTION},
	{"version", no_argument, nullptr, 'V'},
	{"page-count", no_argument, nullptr, 'p'},
	{"quiet", no_argument, nullptr, 'q'},
	{"password", required_argument, nullptr, PASSWORD},
	{"max-count", required_argument, nullptr, 'm'},
	{"debug", no_argument, nullptr, DEBUG_OPTION},
	{"only-matching", no_argument, nullptr, 'o'},
	{"null", no_argument, nullptr, 'Z'},
	{"match-prefix-separator", required_argument, nullptr, PREFIX_SEP_OPTION},
	{"warn-empty", no_argument, nullptr, WARN_EMPTY_OPTION},
	{"unac", no_argument, nullptr, UNAC_OPTION},
	{"fixed-strings", no_argument, nullptr, 'F'},
	{"cache", no_argument, nullptr, CACHE_OPTION},
	{"after-context", required_argument, nullptr, 'A'},
	{"before-context", required_argument, nullptr, 'B'},
	{"context", required_argument, nullptr, 'C'},
	{"page-range", required_argument, nullptr, PAGE_RANGE_OPTION},
	{"regexp", required_argument, nullptr, 'e'},
	{"file", required_argument, nullptr, 'f'},
	{"files-with-matches", no_argument, nullptr, 'l'},
	{"files-without-match", no_argument, nullptr, 'L'},
	{nullptr, 0, nullptr, 0}
};

#ifdef HAVE_UNAC
/* convenience layer over libunac. */
string simple_unac(const Options &opts, const string str)
{
	if (!opts.use_unac) {
		return str;
	}

	char *res = NULL;
	size_t reslen = 0;

	if (unac_string("UTF-8", str.c_str(), str.size(), &res, &reslen) != 0) {
		perror("pdfgrep: Failed to remove accents: ");
		return str;
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
	if (getenv(env_var) == nullptr) {
		return;
	}
	char* colors_list = strdup(getenv(env_var));
	if (colors_list == nullptr) {
		/* if var is not set */
		return;
	}
	/* save original length _before_ editing it with strtok */
	int colors_list_len = strlen(colors_list);
	int i = 0;
	char* cur_color_pair;
	char *cur_name, *cur_value;
	/* read all color=value pairs */
	while (i < colors_list_len && (cur_color_pair = strtok(colors_list+i, ":")) != nullptr) {
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
	     << "Commonly used options:" << endl
	     << " -i, --ignore-case              Ignore case distinctions" << endl
	     << " -P, --perl-regexp              Use Perl compatible regular expressions (PCRE)" << endl
	     << " -H, --with-filename            Print the file name for each match" << endl
	     << " -h, --no-filename              Suppress the prefixing of file name on output" << endl
	     << " -n, --page-number              Print page number with output lines" << endl
	     << " -c, --count                    Print only a count of matches per file" << endl
	     << "     --color WHEN               Use colors for highlighting;" << endl
	     << "                                WHEN can be `always', `never' or `auto'" << endl
	     << " -p, --page-count               Print only a count of matches per page" << endl
	     << " -m, --max-count NUM            Stop reading after NUM matching lines (per file)" << endl
	     << " -q, --quiet                    Suppress normal output" << endl
	     << " -r, --recursive                Search directories recursively" << endl
	     << " -R, --dereference-recursive    Likewise, but follow all symlinks" << endl
	     << "     --cache                    Use cache for faster operation" << endl
	     << "     --help                     Print this help" << endl
	     << " -V, --version                  Show version information" << endl << endl
	     << "The above list is only a selection of commonly used options. Please refer" << endl
	     << "to the man page for a complete list." << endl;
}

static void print_version()
{
	cout << "This is " << PACKAGE << " version " << VERSION << "." << endl << endl;
	cout << "Using poppler version " << poppler::version_string().c_str() << endl;
#ifdef HAVE_UNAC
	cout << "Using libunac version " << unac_version() << endl;
#endif
#ifdef HAVE_LIBPCRE
	auto pcre_version = make_unique<char[]>(pcre2_config(PCRE2_CONFIG_VERSION, nullptr));
	pcre2_config(PCRE2_CONFIG_VERSION, pcre_version.get());
	cout << "Using libpcre2 version " << pcre_version.get() << endl;
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

static int sha1_file(const std::string &filename, unsigned char *sha1out)
{
	std::ifstream file(filename);
	std::stringstream content;
	if (!(content << file.rdbuf())) {
		return -1;
	}
	std::string str(content.str());

	gcry_md_hash_buffer( GCRY_MD_SHA1, sha1out, str.c_str(), str.size());
	return 0;
}

/** Perform search in `path`
 *
 * - filename is the basename of the file without the directory part
 *
 * TODO Return bool
 */
static int do_search_in_document(const Options &opts, const string &path, const string &filename,
                                 Regengine &re, bool check_excludes = true)
{
	if (check_excludes &&
	    (!is_excluded(opts.includes, filename) || is_excluded(opts.excludes, filename))) {
		return 0;
	}

	unique_ptr<Cache> cache;

	if (opts.use_cache) {
		unsigned char sha1sum[20];
		std::string cache_file(opts.cache_directory);
		if (sha1_file(path, sha1sum) != 0) {
			err() << "Could not compute checksum for " << path << endl;
			return 1;
		}
		char translate[] = "0123456789abcdef";
		for (unsigned char c : sha1sum) {
			cache_file += translate[c & 0xf];
			cache_file += translate[(c >> 4 ) & 0xf];
		}

		cache = make_unique<Cache>(cache_file);
	}

	unique_ptr<poppler::document> doc;

	if (opts.passwords.empty()) {
		err() << "Internal error, password vector empty!" << endl;
		abort();
	}

	for (string const &password : opts.passwords) {
		// FIXME This logic doesn't seem to make sens. What if only the
		// first password is correct?
		doc = unique_ptr<poppler::document>(
			poppler::document::load_from_file(path, string(password),
							  string(password))
			);
	}

	if (doc == nullptr || doc->is_locked()) {
		err() << "Could not open " << path.c_str() << endl;
		return 1;
	}

	int matches = search_document(opts, std::move(doc), std::move(cache), path, re);
	if (matches > 0) {
		found_something = true;
		if (opts.quiet) {
			exit(EXIT_SUCCESS); // FIXME: Handle this with return value
		}
	}

	return 0;
}

static int do_search_in_directory(const Options &opts, const string &filename, Regengine &re)
{
	DIR *ptrDir = nullptr;

	ptrDir = opendir(filename.c_str());
	if (ptrDir == nullptr) {
		err() << filename.c_str() << ": " << strerror(errno) << endl;
		return 1;
	}

	while(true) {
		string path(filename);
		errno = 0;
		struct dirent *ptrDirent = readdir(ptrDir);    //not sorted, in order as `ls -f`
		if (ptrDirent == nullptr) {
			break;
		}

		if (strcmp(ptrDirent->d_name, ".") == 0 || strcmp(ptrDirent->d_name, "..") == 0) {
			continue;
		}

		path += "/";
		path += ptrDirent->d_name;

		struct stat st;
		int statret;

		if (opts.recursive == Recursion::FOLLOW_SYMLINKS) {
			statret = stat(path.c_str(), &st);
		} else {
			statret = lstat(path.c_str(), &st);
		}

		if (statret != 0) {
			err() << filename.c_str() << ": " << strerror(errno) << endl;
			continue;
		}

		if (S_ISLNK(st.st_mode)) {
			continue;
		}

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

bool read_pattern_file(string const &filename, vector<string> &patterns)
{
	ifstream file(filename);

	if (!file.is_open()) {
		err() << filename << ": " << strerror(errno) << endl;
		return false;
	}

	string line;
	while (getline(file, line)) {
		patterns.push_back(line);
	}

	if (file.bad()) {
		err() << filename << ": " << strerror(errno) << endl;
		return false;
	}

	return true;
}

#if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 29
static void handle_poppler_errors(const string &msg, void *_opts)
{
	auto *opts = static_cast<Options*>(_opts);
	if (opts->debug) {
		err() << msg << endl;
	}
}
#endif

int main(int argc, char** argv)
{
	Options options;
	init_colors(options.outconf.colors);

	try {
		// Set locale to user-preference. If this locale is an UTF-8 locale, the
		// regex-functions regcomp/regexec become unicode aware, which means
		// e.g. that '.' will match a unicode character, not a single byte.
		locale::global(locale(""));
	} catch (const runtime_error &e) {
		// Fall back to the "C" locale.
		locale::global(locale::classic());
		err() << "Failed to set user locale: \"" << e.what()
		      << "\". Falling back to default" << endl;
	}

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

	// patterns specified with --regex or --file
	vector<string> patterns;
	bool patterns_specified = false;

	while (true) {
		int c = getopt_long(argc, argv, "icA:B:C:nrRhHVPpqm:FoZe:f:lL",
				long_options, nullptr);

		if (c == -1) {
			break;
		}

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
			case PAGENUM_OPTION:
				options.outconf.pagenum = true;
				if (optarg == nullptr || !strcmp(optarg, "index")) {
					options.outconf.pagenum_type = PagenumType::INDEX;
				} else if (!strcmp(optarg, "label")) {
					options.outconf.pagenum_type = PagenumType::LABEL;
				} else {
					err() << "Invalid argument '" << optarg
					      << "' for --page-number"
					      << "Candidates are: index, label"
					      << endl;
					exit(EXIT_ERROR);
				}
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
				if (strcmp("always", optarg) == 0) {
					use_colors = COLOR_ALWAYS;
				} else if (strcmp("never", optarg) == 0) {
					use_colors = COLOR_NEVER;
				} else if (strcmp("auto", optarg) == 0) {
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
				options.passwords.emplace_back(string(optarg));
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

			case CACHE_OPTION:
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

			case PAGE_RANGE_OPTION:
				options.page_range = IntervalContainer::fromString(optarg);
				break;

			case 'e':
				patterns_specified = true;
				patterns.emplace_back(string(optarg));
				break;

			case 'f':
				patterns_specified = true;
				if (!read_pattern_file(string(optarg), patterns)) {
					exit(EXIT_ERROR);
				}
				break;

			case 'l':
				options.only_filenames = OnlyFilenames::WITH_MATCHES;
				break;

			case 'L':
				options.only_filenames = OnlyFilenames::WITHOUT_MATCH;
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

	int remaining_args = argc - optind;
	int required_args = 0;
	if (!patterns_specified) {
		required_args++;
	}
	if (options.recursive == Recursion::NONE) {
		required_args++;
	}

	if (remaining_args < required_args) {
		print_usage(argv[0]);
		exit(EXIT_ERROR);
	}

	unique_ptr<Regengine> re;
	if (re_engine == (RE_FIXED | RE_PCRE)) {
		err() << "--pcre and --fixed cannot be used together" << endl;
		exit(EXIT_ERROR);
	}

	auto make_regengine = [&](const string pattern) -> unique_ptr<Regengine> {
#ifdef HAVE_UNAC
		const string new_pattern = simple_unac(options, pattern);
#else
		const string new_pattern = pattern;
#endif
#ifdef HAVE_LIBPCRE
		if (re_engine == RE_PCRE) {
			return make_unique<PCRERegex>(new_pattern, options.ignore_case);
		}
#endif // HAVE_LIBPCRE
		if (re_engine == RE_FIXED) {
			return make_unique<FixedString>(new_pattern, options.ignore_case);
		}

		// RE_POSIX
		return make_unique<PosixRegex>(new_pattern, options.ignore_case);
	};

	if (patterns.empty()) {
		re = make_regengine(argv[optind++]);
	} else {
		auto patt_list = std::make_unique<PatternList>();
		for (auto const &p : patterns) {
			patt_list->add_pattern(make_regengine(p));
		}
		re = std::move(patt_list);
	}

#if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 29
	// set poppler error output function
	poppler::set_debug_error_function(handle_poppler_errors, &options);
#endif

	bool color_tty = isatty(STDOUT_FILENO) != 0
		&& getenv("TERM") != nullptr
		&& strcmp(getenv("TERM"), "dumb") != 0;

	options.outconf.color =
		use_colors == COLOR_ALWAYS
		|| (use_colors == COLOR_AUTO && color_tty);

	if (options.outconf.color) {
		read_colors_from_env(options.outconf.colors, "GREP_COLORS");
	}

	if (!explicit_filename_option) {
		if ((argc - optind) == 1 && !is_dir(argv[optind])) {
			options.outconf.filename = false;
		} else {
			options.outconf.filename = true;
		}
	}

	if (options.outconf.only_matching && (options.outconf.context_before > 0
					      || options.outconf.context_after > 0)) {
		err() << "warning: --only-matching and context options can't be used together."
		      << " Ignoring context option." << endl;

		options.outconf.context_mode = false;
	}

	if (options.count && options.outconf.pagenum) {
		err() << "warning: --count and --page-number can't be used together."
		      << " Ignoring --page-number." << endl;

		options.outconf.pagenum = false;
	}

	// TODO Warn about --files-{with-matches,without-match} and other output
	// options

	if (excludes_empty(options.includes)) {
		exclude_add(options.includes, "*.[Pp][Dd][Ff]");
	}

	// If no password has been specified on the command line, insert the
	// empty string aka "no password" into the passwords array.
	if (options.passwords.empty()) {
		options.passwords.emplace_back("");
	}

	if (options.use_cache) {
		if (find_cache_directory(options.cache_directory) != 0) {
			err() << "warning: Failed to initialize cache directory."
			      << " no cache is used!" << endl;
			options.use_cache = false;
		} else {
			char *limitstr = getenv("PDFGREP_CACHE_LIMIT");
			unsigned int limit = (limitstr != nullptr) ? strtoul(limitstr, nullptr, 10) : 200;
			limit_cachesize(options.cache_directory.c_str(), limit);
		}
	}

	bool error = false;

	for (int i = optind; i < argc; i++) {
		const string filename(argv[i]);

		if (!is_dir(filename)) {
			if (do_search_in_document(options, filename, filename, *re, false) != 0) {
				error = true;
			}
		} else if (options.recursive != Recursion::NONE) {
			if (do_search_in_directory(options, filename, *re) != 0) {
				error = true;
			}
		} else {
			err() << filename << " is a directory. Did you mean to use '--recursive'?" << endl;
			error = true;
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
