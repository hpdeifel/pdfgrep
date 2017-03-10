/***************************************************************************
 *   Copyright (C) 2017 by Hans-Peter Deifel                               *
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

/* Inclusive interval
 */
struct Interval {
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

	static IntervalContainer fromString(const std::string str);

	void addInterval(Interval i);
	bool contains(int element) const;

private:
	std::vector<Interval> intervals;
};

#endif /* INTERVALS_H */
