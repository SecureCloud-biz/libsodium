
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "crypto_hash_sha512.h"
#include "crypto_sign_ed25519.h"
#include "crypto_verify_32.h"
#include "utils.h"
#include "../../../crypto_core/curve25519/ref10/curve25519_ref10.h"

#ifndef ED25519_COMPAT
static int
crypto_sign_check_S_lt_l(const unsigned char *S)
{
    static const unsigned char l[32] =
      { 0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
        0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 };
    unsigned char c = 0;
    unsigned char n = 1;
    unsigned int  i = 32;

    do {
        i--;
        c |= ((S[i] - l[i]) >> 8) & n;
        n &= ((S[i] ^ l[i]) - 1) >> 8;
    } while (i != 0);

    return -(c == 0);
}
#endif

int
crypto_sign_ed25519_verify_detached(const unsigned char *sig,
                                    const unsigned char *m,
                                    unsigned long long mlen,
                                    const unsigned char *pk)
{
    crypto_hash_sha512_state hs;
    unsigned char h[64];
    unsigned char rcheck[32];
    unsigned int  i;
    unsigned char d = 0;
    ge_p3 A;
    ge_p2 R;

#ifndef ED25519_COMPAT
    if (crypto_sign_check_S_lt_l(sig + 32) != 0) {
        return -1;
    }
#else
    if (sig[63] & 224) {
        return -1;
    }
#endif
    if (ge_frombytes_negate_vartime(&A, pk) != 0) {
        return -1;
    }
    for (i = 0; i < 32; ++i) {
        d |= pk[i];
    }
    if (d == 0) {
        return -1;
    }
    crypto_hash_sha512_init(&hs);
    crypto_hash_sha512_update(&hs, sig, 32);
    crypto_hash_sha512_update(&hs, pk, 32);
    crypto_hash_sha512_update(&hs, m, mlen);
    crypto_hash_sha512_final(&hs, h);
    sc_reduce(h);

    ge_double_scalarmult_vartime(&R, h, &A, sig + 32);
    ge_tobytes(rcheck, &R);

    return crypto_verify_32(rcheck, sig) | (-(rcheck == sig)) |
           sodium_memcmp(sig, rcheck, 32);
}

int
crypto_sign_ed25519_open(unsigned char *m, unsigned long long *mlen_p,
                         const unsigned char *sm, unsigned long long smlen,
                         const unsigned char *pk)
{
    unsigned long long mlen;

    if (smlen < 64 || smlen > SIZE_MAX) {
        goto badsig;
    }
    mlen = smlen - 64;
    if (crypto_sign_ed25519_verify_detached(sm, sm + 64, mlen, pk) != 0) {
        memset(m, 0, mlen);
        goto badsig;
    }
    if (mlen_p != NULL) {
        *mlen_p = mlen;
    }
    memmove(m, sm + 64, mlen);

    return 0;

badsig:
    if (mlen_p != NULL) {
        *mlen_p = 0;
    }
    return -1;
}
