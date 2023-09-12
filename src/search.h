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

#ifndef SEARCH_H
#define SEARCH_H

#include <memory>
#include <cpp/poppler-document.h>

#include "pdfgrep.h"
#include "regengine.h"
#include "cache.h"

// Returns the number of matches found in this document
int search_document(const Options &opts, std::unique_ptr<poppler::document> doc,
                    std::unique_ptr<Cache> cache, const std::string &filename,
                    const Regengine &re);


#endif /* SEARCH_H */

/* Local Variables: */
/* mode: c++ */
/* End: */
