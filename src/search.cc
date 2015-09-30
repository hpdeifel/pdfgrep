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
static int search_page(const Options &opts, unique_ptr<poppler::page> page, size_t pagenum,
                        const string &filename, const Regengine &re, SearchState &state);

static void print_match(const Options &opts, const string &filename, size_t page, struct match &mt);

#ifdef HAVE_UNAC
/* convenience layer over libunac */
static string simple_unac(const Options &opts, string str);
#endif
static string maybe_unac(const Options &opts, string std);

int search_document(const Options &opts, unique_ptr<poppler::document> doc,
                    const string &filename, const Regengine &re) {

	SearchState state;

	// doc->pages() returns an int, although it should be a size_t
	size_t doc_pages = static_cast<size_t>(doc->pages());

	for (size_t pagenum = 1; pagenum <= doc_pages; pagenum++) {
		unique_ptr<poppler::page> page(doc->create_page(pagenum-1));

		if (!page) {
			if (!opts.quiet) {
				err() << "Could not search in page " << pagenum
				      << " of " << filename << endl;
			}
			continue;
		}

		int page_count = search_page(opts, move(page), pagenum, filename, re, state);

		if (page_count > 0 && opts.pagecount && !opts.quiet) {
			line_prefix(opts.outconf, filename, pagenum) << page_count << endl;
		}

		if (opts.max_count > 0 && state.total_count >= opts.max_count) {
			break;
		}
	}

	if (opts.count && !opts.quiet) {
		line_prefix(opts.outconf, filename) << state.total_count << endl;
	}

	if (opts.warn_empty && state.document_empty) {
		err() << "File does not contain text: " << filename << endl;
	}

	return state.total_count;
}

static string page_text(poppler::page &page) {
	poppler::byte_array arr = page.text().to_utf8();
	char *c_str = &arr[0];
	return string(c_str, arr.size());
}

static int search_page(const Options &opts, unique_ptr<poppler::page> page, size_t pagenum,
                       const string &filename, const Regengine &re, SearchState &state)
{
	// Count of matches just on this page
	int page_count = 0;

	if (page->text().empty()) {
		return 0;
	}

	// there is text on this page, document can't be empty
	state.document_empty = false;

	string text = maybe_unac(opts, page_text(*page));

	size_t index = 0;
	struct match mt = { text.c_str(), text.size(), 0, 0 };

	while (!re.exec(text.c_str(), index, &mt)) {
		state.total_count++;
		page_count++;

		if (opts.quiet) {
			return page_count;
		}

		print_match(opts, filename, pagenum, mt);

		if (opts.max_count > 0 && state.total_count >= opts.max_count) {
			return page_count;
		}

		index = mt.end;

		// prevent loop if match is empty
		if (mt.start == mt.end) {
			index++;
		}

		if(index >= text.size())
			break;
	}

	return page_count;
}

static void print_match(const Options &opts, const string &filename, size_t page, struct match &mt) {
	struct context cntxt = {(char*)filename.c_str(), (int)page, opts.outconf};

	if (opts.count || opts.pagecount)
		return;

	if (opts.outconf.only_matching) {
		print_only_match(&cntxt, &mt);
		return;
	}

	print_context_line(&cntxt, &mt);
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
