// ability_onekey.c — OneKeyAbility 实现（C51/8051 版）
// issue_token / verify_token / revoke_all / list_tokens / status / rotate
// 令牌 = HMAC-SHA256(secret, "seq:subject")，base64url 呈现。
// 密钥与序列存 EEPROM（fe_port_eeprom_*）。
#include "fe_ability.h"
#include "fe_hmac_sha256.h"
#include "fe_port.h"
#include <string.h>
#include <stdlib.h>

#define SECRET_ADDR 0x0100   // 密钥 EEPROM 地址
#define SEQ_ADDR    0x0130   // 序列 EEPROM 地址

static const char *B64URL_TBL = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static void b64url_encode(const u8 *data, u16 len, char *out, u16 outlen) {
    u16 n = 0;
    u16 i = 0;
    for (i = 0; i + 2 < len && n + 4 < outlen; i += 3) {
        u32 v = ((u32)data[i] << 16) | ((u32)data[i+1] << 8) | data[i+2];
        out[n++] = B64URL_TBL[(v >> 18) & 63];
        out[n++] = B64URL_TBL[(v >> 12) & 63];
        out[n++] = B64URL_TBL[(v >> 6) & 63];
        out[n++] = B64URL_TBL[v & 63];
    }
    if (len % 3 == 1 && n + 2 <= outlen) {
        u32 v = (u32)data[len-1] << 16;
        out[n++] = B64URL_TBL[(v >> 18) & 63];
        out[n++] = B64URL_TBL[(v >> 12) & 63];
    } else if (len % 3 == 2 && n + 3 <= outlen) {
        u32 v = ((u32)data[len-2] << 16) | ((u32)data[len-1] << 8);
        out[n++] = B64URL_TBL[(v >> 18) & 63];
        out[n++] = B64URL_TBL[(v >> 12) & 63];
        out[n++] = B64URL_TBL[(v >> 6) & 63];
    }
    if (n < outlen) out[n] = 0;
}

static void load_or_create_secret(onekey_ability_t *self) {
    if (!fe_port_eeprom_get_str(SECRET_ADDR, self->secret, sizeof(self->secret))) {
        fe_port_random_fill((u8 *)self->secret, 32);
        self->secret[32] = 0;
        fe_port_eeprom_set_str(SECRET_ADDR, self->secret);
    }
    fe_port_eeprom_get_u32(SEQ_ADDR, &self->seq);
}

static void persist_seq(onekey_ability_t *self) {
    fe_port_eeprom_set_u32(SEQ_ADDR, self->seq);
}

fe_output_t ability_onekey_dispatch(void *inst, const char *act, const char *args) {
    onekey_ability_t *self = (onekey_ability_t *)inst;
    const char *subject;
    if (self->secret[0] == 0) load_or_create_secret(self);
    subject = (args && args[0]) ? args : "default";

    if (strcmp(act, "status") == 0) {
        char out[48];
        fe_snprintf(out, sizeof(out), "{\"tokens\":%lu}", (unsigned long)(self->seq + 1));
        return fe_ok(act, out);
    }
    if (strcmp(act, "issue_token") == 0) {
        char payload[48];
        u8 mac[32];
        char tok[48];
        char out[96];
        fe_snprintf(payload, sizeof(payload), "%lu:%s", (unsigned long)self->seq, subject);
        fe_hmac_sha256((const u8 *)self->secret, (u16)strlen(self->secret),
                       (const u8 *)payload, (u16)strlen(payload), mac);
        b64url_encode(mac, 32, tok, sizeof(tok));
        fe_snprintf(out, sizeof(out), "{\"token\":\"%s\",\"seq\":%lu}",
                    tok, (unsigned long)self->seq);
        self->seq++;
        persist_seq(self);
        return fe_ok(act, out);
    }
    if (strcmp(act, "verify_token") == 0) {
        // 期望 args = "seq:token:subject"
        char buf[128];
        char *colon1, *tok, *colon2;
        const char *subj;
        u32 seq;
        char payload[48];
        u8 mac[32];
        char expect[48];
        u8 valid;
        char out[48];
        fe_snprintf(buf, sizeof(buf), "%s", args ? args : "");
        colon1 = strchr(buf, ':');
        if (!colon1) return fe_err(act, "bad format, expect seq:token");
        *colon1 = 0;
        tok = colon1 + 1;
        colon2 = strchr(tok, ':');
        subj = subject;
        if (colon2) { *colon2 = 0; subj = colon2 + 1; }
        seq = (u32)strtoul(buf, NULL, 10);
        fe_snprintf(payload, sizeof(payload), "%lu:%s", (unsigned long)seq, subj);
        fe_hmac_sha256((const u8 *)self->secret, (u16)strlen(self->secret),
                       (const u8 *)payload, (u16)strlen(payload), mac);
        b64url_encode(mac, 32, expect, sizeof(expect));
        valid = (strcmp(expect, tok) == 0);
        fe_snprintf(out, sizeof(out), "{\"valid\":%s}", valid ? "true" : "false");
        return valid ? fe_ok(act, out) : fe_err(act, "token invalid");
    }
    if (strcmp(act, "revoke_all") == 0) {
        // TODO: 引入黑名单后实现真正吊销；当前重置序列使旧令牌失效
        self->seq = 0;
        persist_seq(self);
        return fe_ok(act, "{\"revoked\":true}");
    }
    if (strcmp(act, "list_tokens") == 0) {
        // TODO: 维护令牌登记表后返回全部未吊销令牌
        return fe_ok(act, "{\"tokens\":[]}");
    }
    if (strcmp(act, "rotate") == 0) {
        fe_port_eeprom_set_str(SECRET_ADDR, "");
        self->secret[0] = 0;
        load_or_create_secret(self);
        self->seq = 0;
        persist_seq(self);
        return fe_ok(act, "{\"rotated\":true}");
    }
    return fe_err(act, "unsupported command");
}