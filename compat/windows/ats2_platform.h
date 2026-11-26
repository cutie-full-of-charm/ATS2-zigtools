#ifndef ATS2_PLATFORM_H
#define ATS2_PLATFORM_H

#ifdef _WIN32
#include <direct.h>
#define ats2_platform_getcwd _getcwd
#else
#include <unistd.h>
#define ats2_platform_getcwd getcwd
#endif

#endif
