#include <stdint.h>
#include <stdio.h>
#include "libats/libc/CATS/stdlib.cats"

static int verify(long seed, const uint64_t expected[5])
{
    atslib_libats_libc_srand48(seed);
    for (int i = 0; i < 5; ++i) {
        const double actual = atslib_libats_libc_drand48();
        const double reference = (double)expected[i] * 0x1p-48;
        if (actual != reference) {
            fprintf(stderr, "drand48 mismatch at seed %ld, index %d: %.17g != %.17g\n",
                    seed, i, actual, reference);
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    static const uint64_t zero[5] = {
        UINT64_C(0x2BBB62DC5101), UINT64_C(0xBFF993816378),
        UINT64_C(0x18ABD0152A23), UINT64_C(0xDED6CF2262F2),
        UINT64_C(0x93CA29A18D15)
    };
    static const uint64_t one[5] = {
        UINT64_C(0x0AA849495101), UINT64_C(0x74599DEA6378),
        UINT64_C(0xD5B694CA2A23), UINT64_C(0x56032E3362F2),
        UINT64_C(0x90C3E9DE8D15)
    };
    static const uint64_t large[5] = {
        UINT64_C(0xB85403F45101), UINT64_C(0x0FD1BAB96378),
        UINT64_C(0x0290D2ED2A23), UINT64_C(0x2507151A62F2),
        UINT64_C(0x65D4C4398D15)
    };
    if (verify(0, zero) != 0) return 1;
    if (verify(1, one) != 0) return 1;
    if (verify(0x12345678L, large) != 0) return 1;
    puts("drand48 sequence ok");
    return 0;
}
