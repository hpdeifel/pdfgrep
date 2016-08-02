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

#include "search.h"
#include "output.h"

#include <iostream>
#include <math.h>
#include <string.h>

#ifdef HAVE_UNAC
#include <unac.h>
#endif
#include <cpp/poppler-page.h>


using namespace std;

struct SearchState {
	bool document_empty = true;
	// Total match count in the current PDF
	int total_count = 0;
};

// Returns the number of matches found
static int search_page(const Options &opts, const string &text, size_t pagenum,
					   const string &filename, const Regengine &re,
					   SearchState &state);

#ifdef HAVE_UNAC
/* convenience layer over libunac */
static string simple_unac(const Options &opts, string str);
#endif
static string maybe_unac(const Options &opts, string std);
static void handle_match(const Options &opts, const string &filename, size_t page,
                         vector<match> &line, vector<match> &last_line, const match &mt,
                         bool previous_matches);
static void flush_line_matches(const Options &opts, const string &filename, size_t page,
                               vector<match> &line, vector<match> &last_line,
                               bool previous_matches);

static string page_text(poppler::page &page) {
	poppler::byte_array arr = page.text().to_utf8();
	char *c_str = &arr[0];
	return string(c_str, arr.size());
}

int search_document(const Options &opts, unique_ptr<poppler::document> doc,
					unique_ptr<Cache> cache, const string &filename,
					const Regengine &re) {

	SearchState state;

	// doc->pages() returns an int, although it should be a size_t
	size_t doc_pages = static_cast<size_t>(doc->pages());

	for (size_t pagenum = 1; pagenum <= doc_pages; pagenum++) {
		string text;
		if (!opts.use_cache || !cache->get_page(pagenum, text)) {
			unique_ptr<poppler::page> page(doc->create_page(pagenum-1));

			if (!page) {
				if (!opts.quiet) {
					err() << "Could not search in page " << pagenum
						  << " of " << filename << endl;
				}
				continue;
			}

			if (!page->text().empty()) {
				// there is text on this page, document can't be empty
				state.document_empty = false;
			}

			text = page_text(*page);
			// Update the rendering cache
			if (opts.use_cache)
				cache->set_page(pagenum, text);
		}

		int page_count = search_page(opts, text, pagenum,
									 filename, re, state);

		if (page_count > 0 && opts.pagecount && !opts.quiet) {
			line_prefix(opts.outconf, filename, false, pagenum) << page_count << endl;
		}

		if (opts.max_count > 0 && state.total_count >= opts.max_count) {
			break;
		}
	}

	if (opts.count && !opts.quiet) {
		line_prefix(opts.outconf, filename, false) << state.total_count << endl;
	}

	if (opts.warn_empty && state.document_empty) {
		err() << "File does not contain text: " << filename << endl;
	}

	// Save the cache for a later use
	if (opts.use_cache)
		cache->dump();

	return state.total_count;
}

static int search_page(const Options &opts, const string &page_text,
					   size_t pagenum, const string &filename,
					   const Regengine &re, SearchState &state)
{
	// Count of matches just on this page
	int page_count = 0;

	// We need that in flush_line_matches to know if we need to print a
	// context separator.
	bool previous_matches = state.total_count > 0;

	string text = maybe_unac(opts, page_text);

	size_t index = 0;
	struct match mt = { text, 0, 0 };

	// matches found in current line
	vector<match> current;

	// last line that contained matches. We only have to store at most one
	// match, but the ability to store 0 matches is crucial for the initial
	// state.
	vector<match> last_line;

	while (re.exec(text.c_str(), index, mt)) {
		state.total_count++;
		page_count++;

		if (opts.quiet) {
			return page_count;
		}

		handle_match(opts, filename, pagenum, current, last_line, mt, previous_matches);

		if (opts.max_count > 0 && state.total_count >= opts.max_count) {
			flush_line_matches(opts, filename, pagenum, current, last_line, previous_matches);
			if (!last_line.empty() && !opts.count && !opts.pagecount) {
				// Print final context after last match
				struct context cntxt = {filename, pagenum, opts.outconf};
				print_context_after(cntxt, *last_line.rbegin());
			}
			return page_count;
		}

		index = mt.end;

		// prevent loop if match is empty
		if (mt.start == mt.end) {
			index++;
		}

		if(index >= text.size()) {
			break;
		}
	}

	flush_line_matches(opts, filename, pagenum, current, last_line, previous_matches);

	if (!last_line.empty() && !opts.count && !opts.pagecount) {
		// Print final context after last match
		struct context cntxt = {filename, pagenum, opts.outconf};
		print_context_after(cntxt, *last_line.rbegin());
	}

	return page_count;
}

static void flush_line_matches(const Options &opts, const string &filename, size_t page,
                               vector<match> &line, vector<match> &last_line, bool previous_matches){

	struct context cntxt = {filename, page, opts.outconf};

	// We don't want any output:
	if (line.empty() || opts.count || opts.pagecount)
		goto out;

	// context printing
	if (last_line.empty()) {
		// There was no previous match

		if (previous_matches) {
			print_context_separator(opts.outconf);
		}
		// (see document_empty)
		print_context_before(cntxt, *line.begin());
	} else {
		print_context_between(cntxt, *last_line.rbegin(), *line.begin());
	}

	if (opts.outconf.only_matching) {
		for (auto mt : line) {
			print_only_match(cntxt, mt);
		}
	} else {
		print_matches(cntxt, line);
	}

out:
	line.swap(last_line);
	line.clear();
}

static void handle_match(const Options &opts, const string &filename, size_t page,
                         vector<match> &line, vector<match> &last_line, const match &mt,
                         bool previous_matches) {
	if (line.empty()) {
		line.push_back(mt);
		return;
	}
	size_t end_last = line.back().end;
	// TODO: Don't search whole string, just up to mt.start
	auto next_newline = mt.string.find('\n', end_last);
	if (next_newline == string::npos || next_newline > mt.start) {
		line.push_back(mt);
	} else {
		flush_line_matches(opts, filename, page, line, last_line, previous_matches);
		line.push_back(mt);
	}
}

#ifdef HAVE_UNAC
static string simple_unac(const Options &opts, string str)
{
	if (!opts.use_unac)
		return str;

	char *res = NULL;
	size_t reslen = 0;

	if (unac_string("UTF-8", str.c_str(), str.size(), &res, &reslen)) {
		perror("pdfgrep: Failed to remove accents: ");
		return str;
	}

	return res;
}
#endif

static string maybe_unac(const Options &opts, string str) {
#ifdef HAVE_UNAC
	return simple_unac(opts, str);
#else
	(void) opts;
	return str;
#endif
}
