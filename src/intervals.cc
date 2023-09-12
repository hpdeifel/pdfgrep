/***************************************************************************
 *   Copyright (C) 2017-2023 by Hans-Peter Deifel                          *
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

#include "intervals.h"

#include <algorithm>
#include <sstream>
#include <iostream>
#include <stdexcept>

#include "output.h"

// The interval container data structure could be much more sophisticated (e.g
// by using interval trees or even sorted lists), but as the number of intervals
// in typical use cases is small, this shouldn't make much difference and could
// actually be harmful.
//
// tl;dr Measure before you optimize

void IntervalContainer::addInterval(Interval i) {
	intervals.push_back(i);
}

bool IntervalContainer::contains(int element) const {
	// We interpret the empty container as one interval containing
	// everything. This makes sense in the pdfgrep case: If the user doesn't
	// restrict the intervals she want's all pages to be searched.
	if (intervals.empty()) {
		return true;
	}

	auto predicate = [=](const Interval &a) {return a.contains(element);};

	return std::find_if(intervals.begin(), intervals.end(), predicate)
		!= intervals.end();
}

static Interval parse_interval(const std::string &str) {
	size_t minus = str.find('-');

	int from, to;
	try {
		// only one int, not a range
		if (minus == std::string::npos) {
			from = to = std::stoi(str);
		} else {
			auto from_str = str.substr(0, minus);
			auto to_str = str.substr(minus+1, str.length()-minus);

			from = std::stoi(from_str);
			to = std::stoi(to_str);
		}
	} catch (std::invalid_argument e) {
		err() << "Invalid page range \"" << str << "\". "
		      << "Expected a single page or a range PAGE1-PAGE2." << std::endl;
		exit(EXIT_ERROR);
	}

	if (from <= 0 || to <= 0) {
		err() << "Invalid page range \"" << str << "\". "
		      << "Page numbers must be positive." << std::endl;
		exit(EXIT_ERROR);
	}

	if (to < from) {
		err() << "warning: Page range is empty: " << str << std::endl;
	}

	return {from, to};
}

IntervalContainer IntervalContainer::fromString(const std::string &str) {
	IntervalContainer c;
	std::stringstream tokens(str);

	std::string interval;
	while (std::getline(tokens, interval, ',')) {
		c.addInterval(parse_interval(interval));
	}

	return c;
}
