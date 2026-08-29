// fe_hmac_sha256.c — HMAC-SHA256 纯 C 实现（零依赖，C51/8051 版）
#include "fe_hmac_sha256.h"
#include <string.h>

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static const u32 K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

void fe_sha256_init(fe_sha256_ctx *ctx) {
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
    ctx->bitlen = 0; ctx->datalen = 0;
}

static void transform(fe_sha256_ctx *ctx, const u8 *data) {
    u32 m[64], a, b, c, d, e, f, g, h, t1, t2;
    u8 i;
    for (i = 0; i < 16; i++)
        m[i] = ((u32)data[i*4] << 24) | ((u32)data[i*4+1] << 16) |
               ((u32)data[i*4+2] << 8) | (u32)data[i*4+3];
    for (i = 16; i < 64; i++) {
        u32 s0 = ROTR(m[i-15],7) ^ ROTR(m[i-15],18) ^ (m[i-15] >> 3);
        u32 s1 = ROTR(m[i-2],17) ^ ROTR(m[i-2],19) ^ (m[i-2] >> 10);
        m[i] = m[i-16] + s0 + m[i-7] + s1;
    }
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (i = 0; i < 64; i++) {
        u32 S1 = ROTR(e,6) ^ ROTR(e,11) ^ ROTR(e,25);
        u32 ch = (e & f) ^ (~e & g);
        u32 t0 = h + S1 + ch + K[i] + m[i];
        u32 S0 = ROTR(a,2) ^ ROTR(a,13) ^ ROTR(a,22);
        u32 maj = (a & b) ^ (a & c) ^ (b & c);
        t1 = t0; t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void fe_sha256_update(fe_sha256_ctx *ctx, const u8 *data, u16 len) {
    u16 i;
    for (i = 0; i < len; i++) {
        ctx->data[ctx->datalen++] = data[i];
        if (ctx->datalen == 64) {
            transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

void fe_sha256_final(fe_sha256_ctx *ctx, u8 hash[32]) {
    u32 i = ctx->datalen;
    u8 j;
    ctx->data[i++] = 0x80;
    if (i > 56) {
        while (i < 64) ctx->data[i++] = 0;
        transform(ctx, ctx->data);
        i = 0;
    }
    while (i < 56) ctx->data[i++] = 0;
    {
        u64 bitlen = ctx->bitlen + (u64)ctx->datalen * 8;
        for (j = 0; j < 8; j++)
            ctx->data[56 + j] = (u8)(bitlen >> (56 - j * 8));
    }
    transform(ctx, ctx->data);
    for (j = 0; j < 8; j++) {
        hash[j*4]   = (u8)(ctx->state[j] >> 24);
        hash[j*4+1] = (u8)(ctx->state[j] >> 16);
        hash[j*4+2] = (u8)(ctx->state[j] >> 8);
        hash[j*4+3] = (u8)(ctx->state[j]);
    }
}

void fe_sha256(const u8 *data, u16 len, u8 out[32]) {
    fe_sha256_ctx ctx;
    fe_sha256_init(&ctx);
    fe_sha256_update(&ctx, data, len);
    fe_sha256_final(&ctx, out);
}

void fe_hmac_sha256(const u8 *key, u16 key_len,
                    const u8 *msg, u16 msg_len,
                    u8 out[32]) {
    u8 k[64];
    u8 ipad[64], opad[64];
    u8 inner_hash[32];
    u8 i;

    if (key_len > 64) {
        fe_sha256(key, key_len, k);
        for (i = 32; i < 64; i++) k[i] = 0;
    } else {
        memset(k, 0, sizeof(k));
        memcpy(k, key, key_len);
    }
    for (i = 0; i < 64; i++) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }

    {
        fe_sha256_ctx ctx;
        fe_sha256_init(&ctx);
        fe_sha256_update(&ctx, ipad, 64);
        fe_sha256_update(&ctx, msg, msg_len);
        fe_sha256_final(&ctx, inner_hash);

        fe_sha256_init(&ctx);
        fe_sha256_update(&ctx, opad, 64);
        fe_sha256_update(&ctx, inner_hash, 32);
        fe_sha256_final(&ctx, out);
    }
}