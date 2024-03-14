/***************************************************************************
 *   Copyright (C) 2015-2023 by Hans-Peter Deifel                          *
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

#include <algorithm>
#include <iostream>
#include <cmath>
#include <cstring>

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
static int search_page(const Options& opts,
                       const string& page_text,
                       size_t pagenum,
                       const string& page_label,
                       const string& filename,
                       const Regengine& re,
                       SearchState& state);

static string maybe_unac(const Options &opts, string str);

static void handle_match(const Options& opts,
                         const string& filename,
                         size_t page,
                         const string& page_label,
                         vector<match>& line,
                         vector<match>& last_line,
                         const match& mt,
                         bool previous_matches);

static void flush_line_matches(const Options& opts,
                               const string& filename,
                               size_t page,
                               const string& page_label,
                               vector<match>& line,
                               vector<match>& last_line,
                               bool previous_matches);

static string ustring_to_string(const poppler::ustring& str) {
	poppler::byte_array arr = str.to_utf8();
	if (arr.empty()) {
		return string();
	} else {
		return string(arr.data(), arr.size());
	}
}

int search_document(const Options &opts, unique_ptr<poppler::document> doc,
		    unique_ptr<Cache> cache, const string &filename,
		    const Regengine &re) {

	SearchState state;

	// doc->pages() returns an int, although it should be a size_t
	auto doc_pages = static_cast<size_t>(doc->pages());

	for (size_t pagenum = 1; pagenum <= doc_pages; pagenum++) {
		if (!opts.page_range.contains(pagenum)) {
			continue;
		}

		CachePage cachepage;

		if (!opts.use_cache || !cache->get_page(pagenum, cachepage)) {
			unique_ptr<poppler::page> page(doc->create_page(pagenum-1));

			if (!page) {
				if (!opts.quiet) {
					err() << "Could not search in page " << pagenum
					      << " of " << filename << endl;
				}
				continue;
			}

			cachepage.text = ustring_to_string(page->text(page->page_rect(poppler::media_box)));
			string& pagetext = cachepage.text;

			// newer versions of poppler generate spurious
			// whitespace at the end of pages. Since in a pdf
			// trailing whitespace text is visually identical to no
			// text, we can just remove it.
			auto whitespace_start =
				std::find_if(pagetext.rbegin(),
					     pagetext.rend(), [](unsigned char ch) {
						     return !std::isspace(ch);
					     });
			pagetext.erase(whitespace_start.base(), pagetext.end());


			if (!pagetext.empty()) {
				// there is text on this page, document can't be empty
				state.document_empty = false;
			}

			// TODO Don't read label if we don't need it
			cachepage.label = ustring_to_string(page->label());

			// Update the rendering cache
			if (opts.use_cache) {
				cache->set_page(pagenum, cachepage);
			}
		}

		string& text = cachepage.text;
		string& label = cachepage.label;

		if (!text.empty()) {
			// there is text on this page, document can't be empty
			state.document_empty = false;
		}


		int page_count = search_page(opts, text, pagenum, label, filename, re, state);

		if (page_count > 0 && opts.quiet) {
			break;
		}

		if (opts.only_filenames == OnlyFilenames::WITH_MATCHES
		    && page_count > 0) {
			if (!opts.quiet) {
				print_only_filename(opts.outconf, filename);
			}
			break;
		}
		if (page_count > 0 && opts.pagecount &&
		    opts.only_filenames == OnlyFilenames::NOPE && !opts.quiet) {
			line_prefix(context { filename, pagenum, label, opts.outconf }, false)
				<< page_count << endl;
		}

		if (opts.max_count > 0 && state.total_count >= opts.max_count) {
			break;
		}
	}

	if (opts.only_filenames == OnlyFilenames::WITHOUT_MATCH
	    && state.total_count == 0
	    && !opts.quiet) {
		print_only_filename(opts.outconf, filename);
	}

	if (opts.count && opts.only_filenames == OnlyFilenames::NOPE && !opts.quiet) {
		line_prefix(context {filename, 0, "", opts.outconf}, false) << state.total_count << endl;
	}

	if (opts.warn_empty && state.document_empty) {
		err() << "File does not contain text: " << filename << endl;
	}

	// Save the cache for a later use
	if (opts.use_cache) {
		cache->dump();
	}

	return state.total_count;
}

static int search_page(const Options& opts,
                       const string& page_text,
                       size_t pagenum,
                       const string& page_label,
                       const string& filename,
                       const Regengine& re,
                       SearchState& state) {
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

	while (re.exec(text, index, mt)) {
		state.total_count++;
		page_count++;

		if (opts.quiet || opts.only_filenames == OnlyFilenames::WITH_MATCHES) {
			return page_count;
		}

		handle_match(opts, filename, pagenum, page_label, current, last_line, mt, previous_matches);

		if (opts.max_count > 0 && state.total_count >= opts.max_count) {
			flush_line_matches(opts, filename, pagenum, page_label, current, last_line, previous_matches);
			if (!last_line.empty() && !opts.count && !opts.pagecount) {
				// Print final context after last match
				struct context cntxt = {filename, pagenum, page_label, opts.outconf};
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

	flush_line_matches(opts, filename, pagenum, page_label, current, last_line, previous_matches);

	if (!last_line.empty() && !opts.count && !opts.pagecount) {
		// Print final context after last match
		struct context cntxt = {filename, pagenum, page_label, opts.outconf};
		print_context_after(cntxt, *last_line.rbegin());
	}

	return page_count;
}

static void flush_line_matches(const Options& opts,
                               const string& filename,
                               size_t page,
                               const string& page_label,
                               vector<match>& line,
                               vector<match>& last_line,
                               bool previous_matches) {

	struct context cntxt = {filename, page, page_label, opts.outconf};

	// We don't want any output:
	if (line.empty() || opts.count || opts.pagecount
	    || opts.only_filenames != OnlyFilenames::NOPE) {
		goto out;
	}

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
		for (auto const &mt : line) {
			print_only_match(cntxt, mt);
		}
	} else {
		print_matches(cntxt, line);
	}

out:
	line.swap(last_line);
	line.clear();
}

static void handle_match(const Options& opts,
                         const string& filename,
                         size_t page,
                         const string& page_label,
                         vector<match>& line,
                         vector<match>& last_line,
                         const match& mt,
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
		flush_line_matches(opts, filename, page, page_label, line, last_line, previous_matches);
		line.push_back(mt);
	}
}

static string maybe_unac(const Options &opts, string str) {
#ifdef HAVE_UNAC
	return simple_unac(opts, str);
#else
	(void) opts;
	return str;
#endif
}
