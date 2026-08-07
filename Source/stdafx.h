/*
	mqme Library Source File

	Copyright © 2009-2026, Keelan Stuart. All rights reserved.

	mqme (pronounced "make me") is a Windows-only C++ API and library that facilitates easy
	distribution of network	packets	with multiple connection end-points. One-to-many is just
	as easy as one-to-one. Handling different types of incoming data is as simple as writing
	a callback that	recognizes a four character code (the 'CODE' form is the easiest way
	to use it).

	mqme was written as a response to the (IMO) confusing popularity
	of RabbitMQ, ActiveMQ, ZeroMQ, etc. RabbitMQ is written in Erlang and requires
	multiple support installations and configuration files to function -- which, I think,
	is bad for commercial products. ActiveMQ, to my knowledge, is similar. ZeroMQ forces
	the user to conform to transactional patterns that are not conducive to parallel
	processing of requests and does not allow comprehensive, complex, or numerous
	subscriptions. In essence, it was my opinion that none of those packages was
	"good enough" for me, making me write my own.

	mqme is free software; you can redistribute it and/or modify it under
	the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	mqme is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.
	See <http://www.gnu.org/licenses/>.
*/

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently

#pragma once

#if defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <winsock2.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <iostream>
#include <xstring>
#include <deque>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <assert.h>

typedef std::basic_string<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR> > tstring;

#if defined(_UNICODE) || defined(UNICODE)

#define LOCAL_TCS2MBCS(wcs, mbcs) {               \
  size_t origsize = _tcslen(wcs) + 1;             \
  size_t newsize = (origsize * 2) * sizeof(char); \
  mbcs = (char *)_alloca(newsize);                \
  wcstombs(mbcs, wcs, newsize); }

#define LOCAL_TCS2WCS(mbcs, wcs) wcs = mbcs

#else

#define LOCAL_TCS2MBCS(wcs, mbcs) mbcs = wcs

#define LOCAL_TCS2WCS(mbcs, wcs) {                \
  size_t origsize = strlen(mbcs) + 1;             \
  size_t newsize = origsize * sizeof(TCHAR);      \
  wcs = (TCHAR *)_alloca(newsize);                \
  mbstowcs(wcs, mbcs, newsize); }

#endif
