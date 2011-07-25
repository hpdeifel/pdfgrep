/***************************************************************************
 *   Copyright (C) 2011 by Hans-Peter Deifel                               *
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

#include "exclude.h"

#include <string>

#include <fnmatch.h>

void exclude_add(ExcludeList &list, const char *name)
{
	list.push_front(name);
}

bool is_excluded(const ExcludeList &list, const char *name)
{
	ExcludeList::const_iterator iter = list.begin();

	for (; iter != list.end(); iter++) {
		if (!fnmatch(*iter, name, FNM_PATHNAME | FNM_PERIOD))
			return true;

	}

	return false;
}

bool is_excluded(const ExcludeList &list, const std::string &name)
{
	return is_excluded(list, name.c_str());
}

bool excludes_empty(ExcludeList &list)
{
	return list.empty();
}
