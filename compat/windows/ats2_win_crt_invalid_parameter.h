#ifndef ATS2_WIN_CRT_INVALID_PARAMETER_H
#define ATS2_WIN_CRT_INVALID_PARAMETER_H

#include <stdlib.h>

#if defined(__MINGW32__)
__declspec(dllimport) _invalid_parameter_handler __cdecl
_set_thread_local_invalid_parameter_handler(_invalid_parameter_handler handler);
#endif

#endif
