
/*
 *  FIPS-197 compliant AES implementation
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
/*
 *  The AES block cipher was designed by Vincent Rijmen and Joan Daemen.
 *
 *  http://csrc.nist.gov/encryption/aes/rijndael/Rijndael.pdf
 *  http://csrc.nist.gov/publications/fips/fips197/fips-197.pdf
 */
#include <string.h>
#include "aes.h"
#include "esp_crypto.h"

/* Implementation that should never be optimized out by the compiler */
//static void bzero( void *v, size_t n ) {
//    volatile unsigned char *p = v; while( n-- ) *p++ = 0;
//}

void esp_aes_init( AES_CTX *ctx )
{
    memset( ctx, 0, sizeof( AES_CTX ) );

    AES_LOCK();
    AES_TAKE();
    ets_aes_enable();
    AES_UNLOCK();
}

void esp_aes_free( AES_CTX *ctx )
{
    if ( ctx == NULL ) {
        return;
    }

    bzero( ctx, sizeof( AES_CTX ) );

    AES_LOCK();
    AES_GIVE();

    if (false == AES_IS_USED()) {
        ets_aes_disable();
    }

    AES_UNLOCK();
}

/*
 * AES key schedule (encryption)
 */
int esp_aes_setkey_enc( AES_CTX *ctx, const unsigned char *key,
                        unsigned int keybits )
{
    enum AES_BITS   keybit;
    uint16_t keybyte = keybits / 8;

    switch (keybits) {
    case 128:
        keybit = AES128;
        break;
    case 192:
        keybit = AES192;
        break;
    case 256:
        keybit = AES256;
        break;
    default:
        return ( ERR_AES_INVALID_KEY_LENGTH );
    }

    if (ctx->enc.keyflag == false) {
        ctx->enc.keyflag = true;
        ctx->enc.keybits = keybits;
        memset(ctx->enc.key, 0, sizeof(ctx->enc.key));
        memcpy(ctx->enc.key, key, keybyte);
    } else {
        ets_aes_setkey_enc(key, keybit);
    }

    return 0;
}

/*
 * AES key schedule (decryption)
 */
int esp_aes_setkey_dec( AES_CTX *ctx, const unsigned char *key,
                        unsigned int keybits )
{
    enum AES_BITS   keybit;
    uint16_t keybyte = keybits / 8;

    switch (keybits) {
    case 128:
        keybit = AES128;
        break;
    case 192:
        keybit = AES192;
        break;
    case 256:
        keybit = AES256;
        break;
    default:
        return ( ERR_AES_INVALID_KEY_LENGTH );
    }

    if (ctx->dec.keyflag == false) {
        ctx->dec.keyflag = true;
        ctx->dec.keybits = keybits;
        memset(ctx->dec.key, 0, sizeof(ctx->dec.key));
        memcpy(ctx->dec.key, key, keybyte);
    } else {
        ets_aes_setkey_dec(key, keybit);
    }

    return 0;
}

static void esp_aes_process_enable(AES_CTX *ctx, int mode)
{
    if ( mode == AES_ENCRYPT ) {
        esp_aes_setkey_enc(ctx, ctx->enc.key, ctx->enc.keybits);
    } else {
        esp_aes_setkey_dec(ctx, ctx->dec.key, ctx->dec.keybits);
    }

    return;
}

static void esp_aes_process_disable(AES_CTX *ctx, int mode)
{

}

/*
 * AES-ECB block encryption
 */

void esp_aes_encrypt( AES_CTX *ctx,
                      const unsigned char input[16],
                      unsigned char output[16] )
{
    ets_aes_crypt(input, output);

    return ;
}


/*
 * AES-ECB block decryption
 */

void esp_aes_decrypt( AES_CTX *ctx,
                      const unsigned char input[16],
                      unsigned char output[16] )
{
    ets_aes_crypt(input, output);

    return ;
}


/*
 * AES-ECB block encryption/decryption
 */
int esp_aes_crypt_ecb( AES_CTX *ctx,
                       int mode,
                       const unsigned char input[16],
                       unsigned char output[16] )
{
    AES_LOCK();

    esp_aes_process_enable(ctx, mode);

    if ( mode == AES_ENCRYPT ) {
        esp_aes_encrypt( ctx, input, output );
    } else {
        esp_aes_decrypt( ctx, input, output );
    }

    esp_aes_process_disable(ctx, mode);

    AES_UNLOCK();

    return 0;
}


/*
 * AES-CBC buffer encryption/decryption
 */
int esp_aes_crypt_cbc( AES_CTX *ctx,
                       int mode,
                       size_t length,
                       unsigned char iv[16],
                       const unsigned char *input,
                       unsigned char *output )
{
    int i;
    unsigned char temp[16];

    if ( length % 16 ) {
        return ( ERR_AES_INVALID_INPUT_LENGTH );
    }

    if ( mode == AES_DECRYPT ) {
        while ( length > 0 ) {
            memcpy( temp, input, 16 );
            esp_aes_crypt_ecb( ctx, mode, input, output );

            for ( i = 0; i < 16; i++ ) {
                output[i] = (unsigned char)( output[i] ^ iv[i] );
            }

            memcpy( iv, temp, 16 );

            input  += 16;
            output += 16;
            length -= 16;
        }
    } else {
        while ( length > 0 ) {
            for ( i = 0; i < 16; i++ ) {
                output[i] = (unsigned char)( input[i] ^ iv[i] );
            }

            esp_aes_crypt_ecb( ctx, mode, output, output );
            memcpy( iv, output, 16 );

            input  += 16;
            output += 16;
            length -= 16;
        }
    }

    return 0;
}

/*
 * AES-CFB128 buffer encryption/decryption
 */
int esp_aes_crypt_cfb128( AES_CTX *ctx,
                          int mode,
                          size_t length,
                          size_t *iv_off,
                          unsigned char iv[16],
                          const unsigned char *input,
                          unsigned char *output )
{
    int c;
    size_t n = *iv_off;

    if ( mode == AES_DECRYPT ) {
        while ( length-- ) {
            if ( n == 0 ) {
                esp_aes_crypt_ecb( ctx, AES_ENCRYPT, iv, iv );
            }

            c = *input++;
            *output++ = (unsigned char)( c ^ iv[n] );
            iv[n] = (unsigned char) c;

            n = ( n + 1 ) & 0x0F;
        }
    } else {
        while ( length-- ) {
            if ( n == 0 ) {
                esp_aes_crypt_ecb( ctx, AES_ENCRYPT, iv, iv );
            }

            iv[n] = *output++ = (unsigned char)( iv[n] ^ *input++ );

            n = ( n + 1 ) & 0x0F;
        }
    }

    *iv_off = n;

    return 0;
}

/*
 * AES-CFB8 buffer encryption/decryption
 */
int esp_aes_crypt_cfb8( AES_CTX *ctx,
                        int mode,
                        size_t length,
                        unsigned char iv[16],
                        const unsigned char *input,
                        unsigned char *output )
{
    unsigned char c;
    unsigned char ov[17];

    while ( length-- ) {
        memcpy( ov, iv, 16 );
        esp_aes_crypt_ecb( ctx, AES_ENCRYPT, iv, iv );

        if ( mode == AES_DECRYPT ) {
            ov[16] = *input;
        }

        c = *output++ = (unsigned char)( iv[0] ^ *input++ );

        if ( mode == AES_ENCRYPT ) {
            ov[16] = c;
        }

        memcpy( iv, ov + 1, 16 );
    }

    return 0;
}

/*
 * AES-CTR buffer encryption/decryption
 */
int esp_aes_crypt_ctr( AES_CTX *ctx,
                       size_t length,
                       size_t *nc_off,
                       unsigned char nonce_counter[16],
                       unsigned char stream_block[16],
                       const unsigned char *input,
                       unsigned char *output )
{
    int c, i;
    size_t n = *nc_off;

    while ( length-- ) {
        if ( n == 0 ) {
            esp_aes_crypt_ecb( ctx, AES_ENCRYPT, nonce_counter, stream_block );

            for ( i = 16; i > 0; i-- )
                if ( ++nonce_counter[i - 1] != 0 ) {
                    break;
                }
        }
        c = *input++;
        *output++ = (unsigned char)( c ^ stream_block[n] );

        n = ( n + 1 ) & 0x0F;
    }

    *nc_off = n;

    return 0;
}

