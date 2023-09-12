/***************************************************************************
 *   Copyright (C) 2011-2023 by Hans-Peter Deifel                          *
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

#ifndef EXCLUDE_H
#define EXCLUDE_H

#include <list>
#include <string>

typedef std::list<const char*> ExcludeList;

void exclude_add(ExcludeList &list, const char *name);
bool is_excluded(const ExcludeList &list, const char *name);
bool is_excluded(const ExcludeList &list, const std::string &name);
bool excludes_empty(ExcludeList &list);

#endif

/* Local Variables: */
/* mode: c++ */
/* End: */
