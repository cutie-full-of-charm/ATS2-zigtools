#include <stdint.h>

static uint64_t atslib_win_rand48_state = UINT64_C(0x1234ABCD330E);

void atslib_libats_win_srand48(long seed)
{
    atslib_win_rand48_state =
        ((uint64_t)(uint32_t)seed << 16) | UINT64_C(0x330E);
}

double atslib_libats_win_drand48(void)
{
    atslib_win_rand48_state =
        (atslib_win_rand48_state * UINT64_C(0x5DEECE66D) + UINT64_C(0xB)) &
        UINT64_C(0xFFFFFFFFFFFF);
    return (double)atslib_win_rand48_state * 0x1p-48;
}

#if defined(_WIN32)
void srand48(long seed)
{
    atslib_libats_win_srand48(seed);
}
#endif
