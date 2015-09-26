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

#ifndef REGENGINE_H
#define REGENGINE_H

#include "config.h"

#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#ifdef HAVE_LIBPCRE
#include <pcre.h>
#endif
#include <vector>


struct match;

class Regengine
{
public:
	virtual int exec(const char *str, size_t offset, struct match *m) const = 0;
	virtual ~Regengine() {}
};

class PosixRegex : public Regengine
{
public:
	PosixRegex(const char *pattern, bool case_insensitive);
	~PosixRegex();
	int exec(const char *str, size_t offset, struct match *m) const override;
private:
	regex_t regex;
};

#ifdef HAVE_LIBPCRE
class PCRERegex : public Regengine
{
public:
	PCRERegex(const char *pattern, bool case_insensitive);
	~PCRERegex();
	int exec(const char *str, size_t offset, struct match *m) const override;
private:
	pcre *regex;
};
#endif

class FixedString : public Regengine
{
public:
	FixedString(char *pattern, bool case_insensitive);
	int exec(const char *str, size_t offset, struct match *m) const override;
private:
	std::vector<const char*> patterns;
	bool case_insensitive;
};

#endif /* REGENGINE_H */
