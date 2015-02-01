/***************************************************************************
 *   Copyright (C) 2014 by Hans-Peter Deifel                               *
 *   hpdeifel@gmx.de                                                       *
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
#include <regex.h>
#include <getopt.h>
#include <unistd.h>
#include <math.h>
#include <sys/ioctl.h>
#include <fnmatch.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

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

#include "output.h"
#include "exclude.h"

/* set this to 1 if any match was found. Used for the exit status */
int found_something = 0;

/* default options */

int ignore_case = 0;
int f_recursive_search = 0;
int follow_symlinks = 1;
/* characters of context to put around each match.
 * -1 means: print whole line
 * -2 means: try to fit context into terminal */
int context = -2;
int line_width = 80;
int count = 0;
int pagecount = 0;
int quiet = 0;
char *password = (char*)"";
int max_count = 0;
int debug = 0;

#ifdef HAVE_UNAC
int use_unac = 0;
#endif

struct outconf outconf = {
	-1,			/* filename */
	0,			/* pagenum */
	1,			/* color */
};

enum {
	HELP_OPTION,
	COLOR_OPTION,
	EXCLUDE_OPTION,
	INCLUDE_OPTION,
	PASSWORD,
	DEBUG_OPTION,
#ifdef HAVE_UNAC
	UNAC_OPTION,
#endif
};

struct option long_options[] =
{
	{"ignore-case", 0, 0, 'i'},
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
#ifdef HAVE_UNAC
	{"unac", 0, 0, UNAC_OPTION},
#endif
	{0, 0, 0, 0}
};

ExcludeList excludes;
ExcludeList includes;

void regmatch_to_match(const regmatch_t match, int index, struct match *mt)
{
	mt->start = match.rm_so + index;
	mt->end   = match.rm_eo + index;
}

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

int search_in_document(poppler::document *doc, const std::string &filename, regex_t *needle)
{
	regmatch_t match[] = {{0, 0}};
	int count_matches = 0;
	int page_matches = 0;
	int length = 0;
	struct context cntxt = {context, 0, (char*)filename.c_str(), 0, &outconf};

	bool max_count_reached = false;

	for (int i = 1; i <= doc->pages() && !max_count_reached; i++) {
		std::auto_ptr<poppler::page> doc_page(doc->create_page(i - 1));
		if (!doc_page.get()) {
			if (!quiet) {
				fprintf(stderr, "pdfgrep: Could not search in page %d of %s\n", i, filename.c_str());
			}
			continue;
		}

		cntxt.pagenum = i;

		poppler::byte_array str = doc_page->text().to_utf8();
		str.resize(str.size() + 1, '\0');
#ifdef HAVE_UNAC
		char *unac_str = simple_unac(&str[0]);
		char *str_start = unac_str;
#else
		char *str_start = &str[0];
#endif
		int index = 0;
		struct match mt = {str_start, str.size()};

		while (!max_count_reached && !regexec(needle, str_start+index, 1, match, 0)) {
			regmatch_to_match(match[0], index, &mt);

			count_matches++;
			if (max_count > 0 && count_matches >= max_count)
			{
				max_count_reached = true;
			}
			if (quiet) {
#ifdef HAVE_UNAC
				simple_unac_free(unac_str);
#endif
				goto clean;
			} else if (!count && !pagecount) {
				switch (context) {
				/* print whole line */
				case -1:
					print_context_line(&cntxt, &mt);
					break;
				/* try to guess the terminal width */
				case -2:
					/* Calculate the length of the line prefix:
					 * filename:linenumber: */
					length = 0;

					if (outconf.filename)
						length += 1 + filename.size();
					if (outconf.pagenum)
						length += 1 + (int)log10((double)i);

					length += match[0].rm_eo - match[0].rm_so;

					cntxt.before = line_width - length;

					print_context_chars(&cntxt, &mt);
					break;
				/* print a fixed context */
				default:
					print_context_chars(&cntxt, &mt);
					break;
				}

				printf("\n");
			}

			found_something = 1;

			index += match[0].rm_so + 1;
		}

#ifdef HAVE_UNAC
		simple_unac_free(unac_str);
#endif
		if(!quiet && pagecount && count_matches > page_matches) {
			print_line_prefix(&outconf, filename.c_str(), -1);
			printf("%d:%d\n", i, count_matches-page_matches);
			page_matches = count_matches;
		}
	}

	if (count && !quiet) {
		print_line_prefix(&outconf, filename.c_str(), -1);
		printf("%d\n", count_matches);
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
void read_colors_from_env(const char* env_var)
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
		PARSE_COLOR("mt", outconf.colors.highlight)
		else PARSE_COLOR("ms", outconf.colors.highlight)
		else PARSE_COLOR("mc", outconf.colors.highlight)
		else PARSE_COLOR("fn", outconf.colors.filename)
		else PARSE_COLOR("ln", outconf.colors.pagenum)
		else PARSE_COLOR("se", outconf.colors.separator)
#undef PARSE_COLOR
	}
	/* free our copy of the environment var */
	free(colors_list);
}

void set_default_colors()
{
	outconf.colors.filename = strdup("35");
	outconf.colors.pagenum = strdup("32");
	outconf.colors.highlight = strdup("01;31");
	outconf.colors.separator = strdup("36");
}

void free_colors()
{
	free(outconf.colors.filename);
	free(outconf.colors.pagenum);
	free(outconf.colors.highlight);
	free(outconf.colors.separator);
}

void init_colors()
{
	set_default_colors();
	/* free colors, when programm exits */
	atexit(free_colors);
}

/* return the terminal line width or line_width
 * if the former is not available */
int get_line_width()
{
	const char *v = getenv("COLUMNS");
	int width = line_width;

	if (v && *v)
		width = atoi(v);

	struct winsize ws;

	if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && 0 < ws.ws_col)
		width = ws.ws_col;

	if (width > 0)
		return width;


	return line_width;
}

void print_usage(char *self)
{
	printf("Usage: %s [OPTION]... PATTERN FILE...\n", self);
}

void print_help(char *self)
{
	print_usage(self);
	printf("\nSearch for PATTERN in each FILE.\n"
"PATTERN is an extended regular expression.\n\n"

"Options:\n"
" -i, --ignore-case\t\tIgnore case distinctions\n"
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
" -V, --version\t\t\tShow version information\n");
}

void print_version()
{
	printf("This is %s version %s.\n", PACKAGE, VERSION);
	printf("\nUsing poppler version %s\n", poppler::version_string().c_str());
#ifdef HAVE_UNAC
	printf("\nUsing unac version %s\n", unac_version());
#endif
#ifdef PDFGREP_GIT_HEAD
	printf("Built from git-commit %s\n", PDFGREP_GIT_HEAD);
#endif
}

int is_dir(const std::string &filename)
{
	struct stat st;

	if (stat(filename.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
		return 1;
	else
		return 0;
}

int do_search_in_document(const std::string &path, const std::string &filename,
			  regex_t *ptrRegex, bool check_excludes = true)
{
	if (check_excludes &&
	    (!is_excluded(includes, filename) || is_excluded(excludes, filename)))
		return 0;

	std::auto_ptr<poppler::document>
		doc(poppler::document::load_from_file(path, std::string(password),
						      std::string(password)));

	if (!doc.get() || doc->is_locked()) {
		fprintf(stderr, "pdfgrep: Could not open %s\n",
			path.c_str());
		return 1;
	}

	if (search_in_document(doc.get(), path, ptrRegex) && quiet) {
		exit(0);
	}

	return 0;
}

int do_search_in_directory(const std::string &filename, regex_t *ptrRegex)
{
	DIR *ptrDir = NULL;
	struct dirent *ptrDirent = NULL;

	ptrDir = opendir(filename.c_str());
	if (!ptrDir) {
		fprintf(stderr, "pdfgrep: %s: %s\n", filename.c_str(),
			strerror(errno));
		return 1;
	}

	while(1) {
		std::string path(filename);
		errno = 0;
		ptrDirent = readdir(ptrDir);    //not sorted, in order as `ls -f`
		if (!ptrDirent)
			break;

		if (!strcmp(ptrDirent->d_name, ".") || !strcmp(ptrDirent->d_name, ".."))
			continue;

		path += "/";
		path += ptrDirent->d_name;

		struct stat st;
		int statret;

		if (follow_symlinks) statret = stat(path.c_str(), &st);
		else                 statret = lstat(path.c_str(), &st);

		if (statret) {
			fprintf(stderr, "pdfgrep: %s: %s\n", filename.c_str(),
				strerror(errno));
			continue;
		}

		if (S_ISLNK(st.st_mode))
			continue;

		if (S_ISDIR(st.st_mode)) {
			do_search_in_directory(path, ptrRegex);
		} else {
			do_search_in_document(path, ptrDirent->d_name, ptrRegex);
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
void handle_poppler_errors(const std::string &msg, void *)
{
	if (debug) {
		fprintf(stderr, "pdfgrep: %s\n", msg.c_str());
	}
}
#endif

int main(int argc, char** argv)
{
	regex_t regex;
	init_colors();

	while (1) {
		int c = getopt_long(argc, argv, "icC:nrRhHVpqm:",
				long_options, NULL);

		if (c == -1)
			break;

		switch (c) {
			case HELP_OPTION:
				print_help(argv[0]);
				exit(0);
			case 'V':
				print_version();
				exit(0);
			case 'n':
				outconf.pagenum = 1;
				break;
			case 'r':
				follow_symlinks = 0;
			case 'R':
				f_recursive_search = 1;
				break;
			case 'h':
				outconf.filename = 0;
				break;
			case 'H':
				outconf.filename = 1;
				break;
			case 'i':
				ignore_case = REG_ICASE;
				break;
			case 'c':
				count = 1;
				break;
			case COLOR_OPTION:
				if (!optarg)
					break;
				if (!strcmp("always", optarg)) {
					outconf.color = 2;
				} else if (!strcmp("never", optarg)) {
					outconf.color = 0;
				} else {
					outconf.color = 1;
				}
				break;
			case 'C':
				if (!strcmp(optarg, "line")) {
					context = -1;
					break;
				}
				if (!parse_int(optarg, &context)) {
					fprintf(stderr, "pdfgrep: Could not parse number: %s.\n",
						optarg);
					exit(2);
				} else if (context <= 0) {
					fprintf(stderr, "pdfgrep: --context must be positive.\n");
					exit(2);
				}
				break;
			case EXCLUDE_OPTION:
				exclude_add(excludes, optarg);
				break;
			case INCLUDE_OPTION:
				exclude_add(includes, optarg);
				break;
			case 'p':
				pagecount = 1;
				break;

			case 'q':
				quiet = 1;
				break;

			case PASSWORD:
				password = strdup(optarg);
				break;

			case 'm':
				if (!parse_int(optarg, &max_count)) {
					fprintf(stderr, "pdfgrep: Could not parse number: %s.\n",
						optarg);
					exit(2);
				} else if (max_count <= 0) {
					fprintf(stderr, "pdfgrep: --max-count must be positive.\n");
					exit(2);
				}
				break;
			case DEBUG_OPTION:
				debug = 1;
				break;
#ifdef HAVE_UNAC
			case UNAC_OPTION:
				use_unac = 1;
				break;
#endif

			/* In these two cases, getopt already prints an
			 * error message
			 */
			case '?': // unknown option
			case ':': // missing argument
				exit(2);
				break;
			default:
				break;
		}
	}

	if (argc == optind || (argc - optind < 2 && !f_recursive_search)) {
		print_usage(argv[0]);
		exit(2);
	}

	char *pattern = argv[optind++];
#ifdef HAVE_UNAC
	pattern = simple_unac(pattern);
#endif

	int error = regcomp(&regex, pattern, REG_EXTENDED | ignore_case);
	if (error) {
		char err_msg[256];
		regerror(error, &regex, err_msg, 256);
		fprintf(stderr, "pdfgrep: %s\n", err_msg);
		exit(2);
	}

#if POPPLER_VERSION_MAJOR > 0 || POPPLER_VERSION_MINOR >= 29
	// set poppler error output function
	poppler::set_debug_error_function(handle_poppler_errors, NULL);
#endif

	bool color_tty = isatty(STDOUT_FILENO) && getenv("TERM") &&
		strcmp(getenv("TERM"), "dumb");
	if (outconf.color == 1 && !color_tty) {
		outconf.color = 0;
	}

	if (outconf.color) read_colors_from_env("GREP_COLORS");

	if (outconf.filename < 0) {
		if ((argc - optind) == 1 && !is_dir(argv[optind])) {
			outconf.filename = 0;
		} else
			outconf.filename = 1;
	}

	if (isatty(STDOUT_FILENO)) {
		line_width = get_line_width();
	} else if (context == -2) {
		// on non-terminals, always print the whole line
		context = -1;
	}

	if (excludes_empty(includes))
		exclude_add(includes, "*.pdf");

	error = 0;

	for (int i = optind; i < argc; i++) {
		const std::string filename(argv[i]);

		if (!is_dir(filename)) {
			do_search_in_document(filename, filename, &regex,false);
		} else if (f_recursive_search) {
			do_search_in_directory(filename, &regex);
		} else { // TODO: report errors
			error = 1;
		}
	}

	if (argc == optind && f_recursive_search) {
		do_search_in_directory(".", &regex);
	}

	if (error) {
		exit(2);
	} else if (found_something) {
		exit(0);
	} else {
		exit(1);
	}
}

/* vim: set noet: */
