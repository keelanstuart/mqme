// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER				// Allow use of features specific to Windows XP or later.
#define WINVER 0x0501		// Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#ifndef _WIN32_WINDOWS		// Allow use of features specific to Windows 98 or later.
#define _WIN32_WINDOWS 0x0410 // Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE			// Allow use of features specific to IE 6.0 or later.
#define _WIN32_IE 0x0600	// Change this to the appropriate value to target other versions of IE.
#endif

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <winsock2.h>
#include <stdlib.h>
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
