// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

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
