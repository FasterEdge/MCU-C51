// ability_onekey.c — OneKeyAbility 实现（C51/8051 版）
// 令牌 = HMAC-SHA256(secret, "seq:subject")，并在 EEPROM 中登记活动序列与主题。
#include "fe_ability.h"
#include "fe_hmac_sha256.h"
#include "fe_port.h"
#include <string.h>
#include <stdlib.h>

/* ConfigData 使用 0x0000..0x02ff；令牌区从 0x0300 开始，适配 R3 1KB EEPROM。 */
#define SECRET_ADDR     0x0300
#define SEQ_ADDR        0x0324
#define TOKEN_ADDR      0x0330
#define TOKEN_SLOTS     4
#define TOKEN_SUBJ_LEN  16
#define TOKEN_ENTRY     (4 + TOKEN_SUBJ_LEN)

static const char *B64URL_TBL = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static void b64url_encode(const u8 *data, u16 len, char *out, u16 outlen) {
    u16 n = 0, i = 0;
    for (i = 0; i + 2 < len && n + 4 < outlen; i += 3) {
        u32 v = ((u32)data[i] << 16) | ((u32)data[i+1] << 8) | data[i+2];
        out[n++] = B64URL_TBL[(v >> 18) & 63]; out[n++] = B64URL_TBL[(v >> 12) & 63];
        out[n++] = B64URL_TBL[(v >> 6) & 63]; out[n++] = B64URL_TBL[v & 63];
    }
    if (len % 3 == 1 && n + 2 < outlen) {
        u32 v = (u32)data[len-1] << 16;
        out[n++] = B64URL_TBL[(v >> 18) & 63]; out[n++] = B64URL_TBL[(v >> 12) & 63];
    } else if (len % 3 == 2 && n + 3 < outlen) {
        u32 v = ((u32)data[len-2] << 16) | ((u32)data[len-1] << 8);
        out[n++] = B64URL_TBL[(v >> 18) & 63]; out[n++] = B64URL_TBL[(v >> 12) & 63];
        out[n++] = B64URL_TBL[(v >> 6) & 63];
    }
    out[n] = 0;
}

static u16 token_addr(u8 slot) { return (u16)(TOKEN_ADDR + (u16)slot * TOKEN_ENTRY); }

static void clear_tokens(void) {
    u8 i;
    for (i = 0; i < TOKEN_SLOTS; i++) fe_port_eeprom_set_str(token_addr(i) + 4, "");
}

static u8 read_token(u8 slot, u32 *seq, char *subject) {
    u8 first;
    if (!fe_port_eeprom_get_str(token_addr(slot) + 4, subject, TOKEN_SUBJ_LEN)) return FALSE;
    first = (u8)subject[0];
    if (first == 0 || first == 0xff) return FALSE;
    return fe_port_eeprom_get_u32(token_addr(slot), seq);
}

static int find_token(u32 seq, const char *subject) {
    u8 i;
    for (i = 0; i < TOKEN_SLOTS; i++) {
        u32 stored_seq; char stored_subject[TOKEN_SUBJ_LEN];
        if (read_token(i, &stored_seq, stored_subject) && stored_seq == seq && strcmp(stored_subject, subject) == 0)
            return i;
    }
    return -1;
}

static int find_empty_token(void) {
    u8 i;
    for (i = 0; i < TOKEN_SLOTS; i++) {
        u32 seq; char subject[TOKEN_SUBJ_LEN];
        if (!read_token(i, &seq, subject)) return i;
    }
    return -1;
}

static void load_or_create_secret(onekey_ability_t *self) {
    u8 raw[16], i;
    char stored[33];
    fe_port_eeprom_get_str(SECRET_ADDR, stored, sizeof(stored));
    if ((u8)stored[0] == 0xff || strlen(stored) != 32) {
        static const char hex[] = "0123456789abcdef";
        fe_port_random_fill(raw, sizeof(raw));
        for (i = 0; i < 16; i++) {
            self->secret[i * 2] = hex[raw[i] >> 4];
            self->secret[i * 2 + 1] = hex[raw[i] & 15];
        }
        self->secret[32] = 0;
        fe_port_eeprom_set_str(SECRET_ADDR, self->secret);
        self->seq = 0;
        fe_port_eeprom_set_u32(SEQ_ADDR, self->seq);
        clear_tokens();
    } else {
        memcpy(self->secret, stored, sizeof(self->secret));
        fe_port_eeprom_get_u32(SEQ_ADDR, &self->seq);
        if (self->seq == 0xffffffffUL) self->seq = 0;
    }
}

static u8 active_count(void) {
    u8 i, count = 0;
    for (i = 0; i < TOKEN_SLOTS; i++) { u32 seq; char subject[TOKEN_SUBJ_LEN]; if (read_token(i, &seq, subject)) count++; }
    return count;
}

fe_output_t ability_onekey_dispatch(void *inst, const char *act, const char *args) {
    onekey_ability_t *self = (onekey_ability_t *)inst;
    const char *subject = (args && args[0]) ? args : "default";
    if (self->secret[0] == 0) load_or_create_secret(self);

    if (strcmp(act, "status") == 0) {
        char out[64];
        fe_snprintf(out, sizeof(out), "{\"tokens\":%u,\"nextSeq\":%lu}", active_count(), (unsigned long)self->seq);
        return fe_ok(act, out);
    }
    if (strcmp(act, "issue_token") == 0) {
        int slot = find_empty_token();
        char stored_subject[TOKEN_SUBJ_LEN], payload[48], tok[48], out[96];
        u8 mac[32];
        if (slot < 0) return fe_err(act, "token registry full");
        fe_snprintf(stored_subject, sizeof(stored_subject), "%s", subject);
        if (stored_subject[0] == 0) return fe_err(act, "invalid subject");
        fe_snprintf(payload, sizeof(payload), "%lu:%s", (unsigned long)self->seq, stored_subject);
        fe_hmac_sha256((const u8 *)self->secret, (u16)strlen(self->secret), (const u8 *)payload, (u16)strlen(payload), mac);
        b64url_encode(mac, 32, tok, sizeof(tok));
        fe_port_eeprom_set_u32(token_addr((u8)slot), self->seq);
        fe_port_eeprom_set_str(token_addr((u8)slot) + 4, stored_subject);
        fe_snprintf(out, sizeof(out), "{\"token\":\"%s\",\"seq\":%lu}", tok, (unsigned long)self->seq);
        self->seq++;
        fe_port_eeprom_set_u32(SEQ_ADDR, self->seq);
        return fe_ok(act, out);
    }
    if (strcmp(act, "verify_token") == 0) {
        char buf[128], *colon1, *tok, *colon2, payload[48], expect[48];
        const char *subj; u32 seq; u8 mac[32];
        fe_snprintf(buf, sizeof(buf), "%s", args ? args : "");
        colon1 = strchr(buf, ':');
        if (!colon1) return fe_err(act, "bad format, expect seq:token:subject");
        *colon1 = 0; tok = colon1 + 1; colon2 = strchr(tok, ':');
        if (!colon2) return fe_err(act, "missing subject");
        *colon2 = 0; subj = colon2 + 1; seq = (u32)strtoul(buf, NULL, 10);
        if (find_token(seq, subj) < 0) return fe_err(act, "token revoked or unknown");
        fe_snprintf(payload, sizeof(payload), "%lu:%s", (unsigned long)seq, subj);
        fe_hmac_sha256((const u8 *)self->secret, (u16)strlen(self->secret), (const u8 *)payload, (u16)strlen(payload), mac);
        b64url_encode(mac, 32, expect, sizeof(expect));
        return strcmp(expect, tok) == 0 ? fe_ok(act, "{\"valid\":true}") : fe_err(act, "token invalid");
    }
    if (strcmp(act, "revoke_all") == 0) {
        u8 count = active_count(); char out[40];
        clear_tokens();
        fe_snprintf(out, sizeof(out), "{\"revoked\":%u}", count);
        return fe_ok(act, out);
    }
    if (strcmp(act, "list_tokens") == 0) {
        char out[80]; u16 n = 0; u8 i, first = TRUE;
        n += (u16)fe_snprintf(out + n, sizeof(out) - n, "{\"tokens\":[");
        for (i = 0; i < TOKEN_SLOTS; i++) {
            u32 seq; char subject[TOKEN_SUBJ_LEN];
            if (!read_token(i, &seq, subject)) continue;
            n += (u16)fe_snprintf(out + n, sizeof(out) - n, "%s%lu", first ? "" : ",", (unsigned long)seq);
            first = FALSE;
        }
        fe_snprintf(out + n, sizeof(out) - n, "]}");
        return fe_ok(act, out);
    }
    if (strcmp(act, "rotate") == 0) {
        fe_port_eeprom_set_str(SECRET_ADDR, ""); self->secret[0] = 0;
        load_or_create_secret(self);
        return fe_ok(act, "{\"rotated\":true}");
    }
    return fe_err(act, "unsupported command");
}
