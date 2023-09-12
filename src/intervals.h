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

#ifndef INTERVALS_H
#define INTERVALS_H

#include <vector>
#include <string>

/* This file implements a interval container that supports insertion and
 * inclusion tests for integer intervals.
 *
 * Used for the --page-range feature
 */

struct Interval {
	/* from and to are inclusive. */
	Interval(int from, int to) {
		this->from = from;
		this->to = to;
	}

	int from;
	int to;

	inline bool contains(int element) const {
		return element >= from && element <= to;
	}
};

class IntervalContainer {
public:
	IntervalContainer() {}

	/** Parses a string into intervals.
	 *
	 * This accepts the format used for the --page-range option: A comma
	 * separated list of intervals, which can be either a single integer or
	 * two integers separated by a minus character. Whitespace is not
	 * allowed.
	 *
	 * More precisely, the following grammar is implemented:
	 *
	 * INTERVALS ::= 𝝴 | INTERVAL ',' INTERVALS
	 * INTERVAL  ::= int | int '-' int
	 *
	 */
	static IntervalContainer fromString(const std::string &str);

	void addInterval(Interval i);

	/** Returns true if either the element is contained in any interval or
	 * this container is empty.
	 *
	 * The latter case is there to simplify --page-range. If no intervals
	 * are specified, we want to search every page.
	 */
	bool contains(int element) const;

private:
	std::vector<Interval> intervals;
};

#endif /* INTERVALS_H */

/* Local Variables: */
/* mode: c++ */
/* End: */
