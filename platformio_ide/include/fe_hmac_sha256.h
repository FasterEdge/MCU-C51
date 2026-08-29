// fe_hmac_sha256.h — HMAC-SHA256 纯 C 实现（零依赖，C51/8051 版）
// 与 Arduino/Keil 版同一算法，类型改用 u8/u16/u32/u64 以兼容 Keil C51。
#ifndef FE_HMAC_SHA256_H
#define FE_HMAC_SHA256_H

#include "fe.h"   // u8/u16/u32/u64

#ifdef __cplusplus
extern "C" {
#endif

// SHA-256 上下文
typedef struct {
    u32 state[8];
    u64 bitlen;
    u8  data[64];
    u32 datalen;
} fe_sha256_ctx;

// HMAC-SHA256 上下文（封装两层 SHA-256 内垫）
typedef struct {
    fe_sha256_ctx inner;
    fe_sha256_ctx outer;
    u8 key_pad[64];
} fe_hmac_sha256_ctx;

void fe_sha256_init(fe_sha256_ctx *ctx);
void fe_sha256_update(fe_sha256_ctx *ctx, const u8 *data, u16 len);
void fe_sha256_final(fe_sha256_ctx *ctx, u8 hash[32]);

// 便捷：一次性计算 SHA-256
void fe_sha256(const u8 *data, u16 len, u8 out[32]);

// HMAC-SHA256：key 长度任意（<=64 直接使用，>64 先哈希）
void fe_hmac_sha256(const u8 *key, u16 key_len,
                    const u8 *msg, u16 msg_len,
                    u8 out[32]);

#ifdef __cplusplus
}
#endif

#endif // FE_HMAC_SHA256_H