/**********************************************************************************************************
 * Copyright(C)2026,
 * File Name     : EmvUtils.c
 * Version       : v1.0
 * Author        :
 * Description   : EMV utils
 **********************************************************************************************************/

#include "LibApi.h"

#include "global.h"
#include "EmvDemo.h"     /* Compatibility layer: legacy SDK macros such as FUN_IN / TRACE / OsLog */
#include "EmvUtils.h"

int app_rsa_public_key(unsigned char *n, unsigned int nlen, unsigned char *e, unsigned int elen, unsigned char *ibuf, unsigned int ilen, unsigned char *obuf)
{
    FUN_IN;
    /*
    if (OsRSA(n, nlen, e, elen, ibuf, obuf, 1) == 0)
    {
        return 1;
    }*/
    return 0;
}
int app_sha1(unsigned char *ibuf, unsigned int ilen, unsigned char *obuf)
{
    FUN_IN;
    Lib_AlgHash(ibuf, ilen, obuf);
    return 1;
}
int app_des_decrypt(unsigned char *key, unsigned int key_len, unsigned char *fill, unsigned char *ibuf, unsigned int ilen, unsigned char *obuf)
{
    FUN_IN;
    TRACE("need debug");
    unsigned char ucMode = MODE_ECB | DECRYPTION;
    if (key_len == 8)
        ucMode |= DES_SINGLE_KEY;
    else if (key_len == 16)
        ucMode |= DES_DOUBLE_KEY;
    else
        ucMode |= DES_TRIPLE_KEY;
    Lib_AlgDes(NULL, key, ibuf, ilen, ucMode);
    return 1;
}
int app_des_encrypt(unsigned char *key, unsigned int key_len, unsigned char *fill, unsigned char *ibuf, unsigned int ilen, unsigned char *obuf)
{
    FUN_IN;
    TRACE("need debug");
    unsigned char ucMode = MODE_ECB | ENCRYPTION;
    if (key_len == 8)
        ucMode |= DES_SINGLE_KEY;
    else if (key_len == 16)
        ucMode |= DES_DOUBLE_KEY;
    else
        ucMode |= DES_TRIPLE_KEY;
    Lib_AlgDes(NULL, key, ibuf, ilen, ucMode);
    return 1;
}
int app_aes_decrypt(unsigned char *key, unsigned int key_len, unsigned char *ibuf, unsigned int ilen, unsigned char *obuf)
{
    FUN_IN;
    TRACE("need debug");
    unsigned char ucMode = MODE_ECB | DECRYPTION;
    if (key_len == 16)
        ucMode |= AES_KEYLEN_128;
    else if (key_len == 24)
        ucMode |= AES_KEYLEN_192;
    else
        ucMode |= AES_KEYLEN_256;
    Lib_AlgAes(NULL, key, ibuf, ilen, ucMode);
    return 1;
}
int app_aes_encrypt(unsigned char *key, unsigned int key_len, unsigned char *ibuf, unsigned int ilen, unsigned char *obuf)
{
    FUN_IN;
    TRACE("need debug");
    unsigned char ucMode = MODE_ECB | ENCRYPTION;
    if (key_len == 16)
        ucMode |= AES_KEYLEN_128;
    else if (key_len == 24)
        ucMode |= AES_KEYLEN_192;
    else
        ucMode |= AES_KEYLEN_256;
    Lib_AlgAes(NULL, key, ibuf, ilen, ucMode);
    return 1;
}

/* Print first, then execute, when the kernel passes an abnormal length (negative or huge):
   once the TLV length field is corrupted, memset/memcpy moves an astronomical amount, and the symptom is a whole-device hang with no log at all. */
static void app_check_size(const char *fn, int n)
{
    if (n < 0 || n > 2048)
        Lib_Trace("emv: %s size %d", fn, n);
}

void *app_memset(void *str, int c, int n)
{
    // FUN_IN;
    app_check_size("memset", n);
    return memset(str, c, (size_t)n);
}
void *app_memcpy(void *dest, const void *src, int n)
{
    // FUN_IN;
    app_check_size("memcpy", n);
    return memcpy(dest, src, (size_t)n);
}
int app_memcmp(const void *str1, const void *str2, int n)
{
    FUN_IN;
    return memcmp(str1, str2, (size_t)n);
}
int app_strlen(const char *str)
{
    FUN_IN;
    return (int)strlen(str);
}
void app_log(const char *str)
{
    // OsLog(LOG_ERROR, "[EMV KERNEL LOG]: %s", str);
    TRACE("[EMV KERNEL LOG]: %s", str);
}