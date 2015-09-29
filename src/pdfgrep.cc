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
#include <sys/ioctl.h>
#include <fnmatch.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <vector>

#include <cpp/poppler-document.h>
#include <cpp/poppler-page.h>
#include <cpp/poppler-version.h>


#ifdef HAVE_UNAC
#include <unac.h>
#endif

#ifdef HAVE_TERMIOS_H
/* for TIOCGWINSZ on some platforms */
#include <termios.h>
#endif

#include <memory>

#include "pdfgrep.h"
#include "output.h"
#include "exclude.h"
#include "regengine.h"

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
	{"context", 1, 0, 'C'},
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
	{0, 0, 0, 0}
};

#ifdef HAVE_UNAC
/* convenience layer over libunac. The result has to be freed with
 * simple_unac_free */
char *simple_unac(char *string)
{
	if (!use_unac)
		return string;

	char *res = NULL;
	size_t reslen = 0;

	if (unac_string("UTF-8", string, strlen(string), &res, &reslen)) {
		perror("pdfgrep: Failed to remove accents: ");
		return strdup(string);
	}

	return res;
}

void simple_unac_free(char *string)
{
	if (use_unac)
		free(string);
}
#endif

int search_in_document(const Options &opts, poppler::document *doc, const string &filename, Regengine &re)
{
	int count_matches = 0;
	int page_matches = 0;
	int length = 0;
	struct context cntxt = {opts.context_chars, 0, (char*)filename.c_str(), 0, &opts.outconf};

	bool max_count_reached = false;

	// Tracks if there is text on any of the pages
	bool document_empty = true;

	for (int i = 1; i <= doc->pages() && !max_count_reached; i++) {
		unique_ptr<poppler::page> doc_page(doc->create_page(i - 1));
		if (!doc_page.get()) {
			if (!opts.quiet) {
				fprintf(stderr, "pdfgrep: Could not search in page %d of %s\n", i, filename.c_str());
			}
			continue;
		}

		cntxt.pagenum = i;

		// page not empty, set document_empty to false
		if (doc_page->text().empty() == false) {
			document_empty = false;
		}


		poppler::byte_array str = doc_page->text().to_utf8();
		str.resize(str.size() + 1, '\0');
		size_t str_len = str.size() - 1;
#ifdef HAVE_UNAC
		char *unac_str = simple_unac(&str[0]);
		char *str_start = unac_str;
#else
		char *str_start = &str[0];
#endif
		size_t index = 0;
		struct match mt = { .string = str_start, .strlen = str_len };

		while (!max_count_reached && !re.exec(str_start, index, &mt)) {
			count_matches++;
			if (opts.max_count > 0 && count_matches >= opts.max_count)
			{
				max_count_reached = true;
			}
			if (opts.quiet) {
#ifdef HAVE_UNAC
				simple_unac_free(unac_str);
#endif
				goto clean;
			} else if (!opts.count && !opts.pagecount && opts.outconf.only_matching) {
				print_only_match(&cntxt, &mt);
			} else if (!opts.count && !opts.pagecount) {
				switch (opts.context_mode) {
				case ContextMode::WHOLE_LINE:
					print_context_line(&cntxt, &mt);
					break;

				case ContextMode::TERMINAL_WIDTH:
					/* Calculate the length of the line prefix:
					 * filename:linenumber: */
					length = 0;

					if (opts.outconf.filename)
						length += 1 + filename.size();
					if (opts.outconf.pagenum)
						length += 1 + (int)log10((double)i);

					length += mt.end - mt.start;

					cntxt.before = opts.line_width - length;

					print_context_chars(&cntxt, &mt);
					break;

				case ContextMode::FIXED:
					print_context_chars(&cntxt, &mt);
					break;
				}
			}

			found_something = 1;

			index = mt.end;

			// prevent loop if match is empty
			if (mt.start == mt.end) {
				index++;
			}

			if(index >= str_len)
				break;
		}

#ifdef HAVE_UNAC
		simple_unac_free(unac_str);
#endif
		if(!opts.quiet && opts.pagecount && count_matches > page_matches) {
			print_line_prefix(&opts.outconf, filename.c_str(), i);
			printf("%d\n", count_matches-page_matches);
			page_matches = count_matches;
		}
	}

	if (opts.count && !opts.quiet) {
		print_line_prefix(&opts.outconf, filename.c_str(), -1);
		printf("%d\n", count_matches);
	}

	if (opts.warn_empty && document_empty) {
		fprintf(stderr, "pdfgrep: File does not contain text: %s\n",
		        filename.c_str());
	}

clean:
	return count_matches;
}

/* parses a color pair like "foo=bar" to "foo" and "bar" */
void parse_env_color_pair(char* pair, char** name, char** value)
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
void read_colors_from_env(Colorconf &colors, const char* env_var)
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

void set_default_colors(Colorconf &colors)
{
	// The grep(1) manpage documents the default value of GREP_COLORS to be
	// ms=01;31:mc=01;31:sl=:cx=:fn=35:ln=32:bn=32:se=36
	// which corresponds to these values below:

	colors.filename = strdup("35");
	colors.pagenum = strdup("32");
	colors.highlight = strdup("01;31");
	colors.separator = strdup("36");
}

void free_colors(Colorconf &colors)
{
	free(colors.filename);
	free(colors.pagenum);
	free(colors.highlight);
	free(colors.separator);
}

void init_colors(Colorconf &colors)
{
	set_default_colors(colors);
	// TODO Free colors on exit
}

/* return the terminal line width or line_width
 * if the former is not available */
int get_line_width(int default_width)
{
	const char *v = getenv("COLUMNS");
	int width = default_width;

	if (v && *v)
		width = atoi(v);

	struct winsize ws;

	if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && 0 < ws.ws_col)
		width = ws.ws_col;

	if (width > 0)
		return width;


	return default_width;
}

void print_usage(char *self)
{
	printf("Usage: %s [OPTION]... PATTERN FILE...\n", self);
	printf("\nSee '%s --help' for more information\n", self);
}

void print_help(char *self)
{
	printf("Usage: %s [OPTION]... PATTERN FILE...\n"
"\nSearch for PATTERN in each FILE.\n"
"PATTERN is, by default, an extended regular expression.\n\n"

"Options:\n"
" -i, --ignore-case\t\tIgnore case distinctions\n"
" -P, --pcre\t\t\tUse Perl compatible regular expressions (PCRE)\n"
" -H, --with-filename\t\tPrint the file name for each match\n"
" -h, --no-filename\t\tSuppress the prefixing of file name on output\n"
" -n, --page-number\t\tPrint page number with output lines\n"
" -c, --count\t\t\tPrint only a count of matches per file\n"
" -C, --context NUM\t\tPrint at most NUM chars of context\n"
"     --color WHEN\t\tUse colors for highlighting;\n"
"\t\t\t\tWHEN can be `always', `never' or `auto'\n"
" -p, --page-count\t\tPrint only a count of matches per page\n"
" -m, --max-count NUM\t\tStop reading after NUM matching lines (per file)\n"
" -q, --quiet\t\t\tSuppress normal output\n"
" -r, --recursive\t\tSearch directories recursively\n"
" -R, --dereference-recursive\tLikewise, but follow all symlinks\n"
"     --help\t\t\tPrint this help\n"
" -V, --version\t\t\tShow version information\n", self);
}

void print_version()
{
	printf("This is %s version %s.\n", PACKAGE, VERSION);
	printf("\nUsing poppler version %s\n", poppler::version_string().c_str());
#ifdef HAVE_UNAC
	printf("Using unac version %s\n", unac_version());
#endif
#ifdef HAVE_LIBPCRE
	printf("Using libpcre version %s\n", pcre_version());
#endif
#ifdef PDFGREP_GIT_HEAD
	printf("Built from git-commit %s\n", PDFGREP_GIT_HEAD);
#endif
}

bool is_dir(const string &filename)
{
	struct stat st;

	return stat(filename.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

int do_search_in_document(const Options &opts, const string &path, const string &filename,
                          Regengine &re, bool check_excludes = true)
{
	if (check_excludes &&
	    (!is_excluded(opts.includes, filename) || is_excluded(opts.excludes, filename)))
		return 0;

	shared_ptr<poppler::document> doc;

	if (opts.passwords.empty()) {
		fprintf(stderr, "pdfgrep: Internal error, password vector empty!\n");
		abort();
	}

	for (string password : opts.passwords) {
		doc = shared_ptr<poppler::document>(
			poppler::document::load_from_file(path, string(password),
							  string(password))
			);
	}

	if (!doc.get() || doc->is_locked()) {
		fprintf(stderr, "pdfgrep: Could not open %s\n",
			path.c_str());
		return 1;
	}

	if (search_in_document(opts, doc.get(), path, re) && opts.quiet) {
		exit(EXIT_SUCCESS); // FIXME: Handle this with return value
	}

	return 0;
}

int do_search_in_directory(const Options &opts, const string &filename, Regengine &re)
{
	DIR *ptrDir = NULL;

	ptrDir = opendir(filename.c_str());
	if (!ptrDir) {
		fprintf(stderr, "pdfgrep: %s: %s\n", filename.c_str(),
			strerror(errno));
		return 1;
	}

	while(1) {
		string path(filename);
		errno = 0;
		struct dirent *ptrDirent = ptrDirent = readdir(ptrDir);    //not sorted, in order as `ls -f`
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
			fprintf(stderr, "pdfgrep: %s: %s\n", filename.c_str(),
				strerror(errno));
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

bool parse_int(const char *str, int *i)
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
void handle_poppler_errors(const string &msg, void *_opts)
{
	Options *opts = static_cast<Options*>(_opts);
	if (opts->debug) {
		fprintf(stderr, "pdfgrep: %s\n", msg.c_str());
	}
}
#endif

int main(int argc, char** argv)
{
	Options options;
	init_colors(options.outconf.colors);

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
		int c = getopt_long(argc, argv, "icC:nrRhHVPpqm:FoZ",
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
					fprintf(stderr, "pdfgrep: Invalid argument '%s' for --color. "
						"Candidates are: always, never or auto\n", optarg);
					exit(EXIT_ERROR);
				}
				break;
			case 'C': {
				if (!strcmp(optarg, "line")) {
					options.context_mode = ContextMode::WHOLE_LINE;
					break;
				}
				int context_chars;
				if (!parse_int(optarg, &context_chars)) {
					fprintf(stderr, "pdfgrep: Could not parse number: %s.\n",
						optarg);
					exit(EXIT_ERROR);
				} else if (context_chars <= 0) {
					fprintf(stderr, "pdfgrep: --context must be positive.\n");
					exit(EXIT_ERROR);
				}
				options.context_chars = context_chars;
				break;
			}
			case EXCLUDE_OPTION:
				exclude_add(options.excludes, optarg);
				break;
			case INCLUDE_OPTION:
				exclude_add(options.includes, optarg);
				break;
			case 'P':
#ifndef HAVE_LIBPCRE
				fprintf(stderr, "pdfgrep: PCRE support disabled at compile time!\n");
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
					fprintf(stderr, "pdfgrep: Could not parse number: %s.\n",
						optarg);
					exit(EXIT_ERROR);
				} else if (options.max_count <= 0) {
					fprintf(stderr, "pdfgrep: --max-count must be positive.\n");
					exit(EXIT_ERROR);
				}
				break;
			case DEBUG_OPTION:
				options.debug = true;
				break;

			case UNAC_OPTION:
#ifndef HAVE_UNAC
				fprintf(stderr, "pdfgrep: UNAC support disabled at compile time!\n");
				exit(EXIT_ERROR);
#else
				options.use_unac = true;
#endif
				break;
			case 'F':
				re_engine |= RE_FIXED;
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

	if (argc == optind || (argc - optind < 2 && options.recursive != Recursion::NONE)) {
		print_usage(argv[0]);
		exit(EXIT_ERROR);
	}

	char *pattern = argv[optind++];
#ifdef HAVE_UNAC
	pattern = simple_unac(pattern);
#endif

	unique_ptr<Regengine> re;
	if (re_engine == (RE_FIXED | RE_PCRE)) {
		fprintf(stderr, "pdfgrep: --pcre and --fixed cannot be used together\n");
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

	if (isatty(STDOUT_FILENO)) {
		options.line_width = get_line_width(options.line_width);
	} else if (options.context_mode == ContextMode::TERMINAL_WIDTH) {
		// on non-terminals, always print the whole line
		options.context_mode = ContextMode::WHOLE_LINE;
	}

	if (excludes_empty(options.includes))
		exclude_add(options.includes, "*.pdf");

	// If no password has been specified on the command line, insert the
	// empty string aka "no password" into the passwords array.
	if (options.passwords.empty()) {
		options.passwords.push_back("");
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
			fprintf(stderr, "pdfgrep: %s is a directory\n", filename.c_str());
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
