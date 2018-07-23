/*
 *  Copyright (C) 2017, Nayuta, Inc. All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed to the Apache Software Foundation (ASF) under one
 *  or more contributor license agreements.  See the NOTICE file
 *  distributed with this work for additional information
 *  regarding copyright ownership.  The ASF licenses this file
 *  to you under the Apache License, Version 2.0 (the
 *  "License"); you may not use this file except in compliance
 *  with the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an
 *  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *  KIND, either express or implied.  See the License for the
 *  specific language governing permissions and limitations
 *  under the License.
 */
/** @file   ptarm_keys.c
 *  @brief  bitcoin処理: 鍵関連
 *  @author ueno@nayuta.co
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "mbedtls/asn1write.h"
#include "mbedtls/ecdsa.h"
#include "libbase58.h"
#include "segwit_addr.h"

#include "ptarm_local.h"


/********************************************************************
 * prototypes
 ********************************************************************/

static int spk2prefix(const uint8_t **ppPkh, const ptarm_buf_t *pScriptPk);


/**************************************************************************
 * public functions
 **************************************************************************/

bool ptarm_keys_wif2priv(uint8_t *pPrivKey, ptarm_chain_t *pChain, const char *pWifPriv)
{
    // [1byte][32bytes:privkey][1byte][4bytes]
    // プレフィクスの1byteは「圧縮された秘密鍵」
    uint8_t b58dec[1 + PTARM_SZ_PRIVKEY + 1 + 4];
    size_t sz_priv = sizeof(b58dec);
    bool ret = b58tobin(b58dec, &sz_priv, pWifPriv, strlen(pWifPriv));
    if (ret) {
        //chain
        switch (b58dec[0]) {
        case 0x80:
            *pChain = PTARM_MAINNET;
            break;
        case 0xef:
            *pChain = PTARM_TESTNET;
            break;
        default:
            *pChain = PTARM_UNKNOWN;
        }
        //checksum
        uint8_t buf_sha256[PTARM_SZ_HASH256];
        int tail = (sz_priv == sizeof(b58dec)) ? 1 : 0;
        ptarm_util_hash256(buf_sha256, b58dec, 1 + PTARM_SZ_PRIVKEY + tail);
        ret = (memcmp(buf_sha256, b58dec + 1 + PTARM_SZ_PRIVKEY + tail, 4) == 0);
    } else {
        ret = false;
        assert(0);
    }
    if (ret) {
        memcpy(pPrivKey, b58dec + 1, PTARM_SZ_PRIVKEY);
    }
    memset(b58dec, 0, sizeof(b58dec));      //clear for security

    if (ret) {
        ret = ptarm_keys_chkpriv(pPrivKey);
    }

    return ret;
}


bool ptarm_keys_priv2wif(char *pWifPriv, const uint8_t *pPrivKey)
{
    bool ret;
    uint8_t b58[1 + PTARM_SZ_PRIVKEY + 1 + 4];
    uint8_t buf_sha256[PTARM_SZ_HASH256];

    ret = ptarm_keys_chkpriv(pPrivKey);
    if (!ret) {
        return false;
    }

    b58[0] = mPref[PTARM_PREF_WIF];
    memcpy(b58 + 1, pPrivKey, PTARM_SZ_PRIVKEY);
    b58[1 + PTARM_SZ_PRIVKEY] = 0x01;        //圧縮された秘密鍵のみ対応
    ptarm_util_hash256(buf_sha256, b58, 1 + PTARM_SZ_PRIVKEY + 1);
    memcpy(b58 + 1 + PTARM_SZ_PRIVKEY + 1, buf_sha256, 4);

    size_t sz = PTARM_SZ_WIF_MAX;
    ret = b58enc(pWifPriv, &sz, b58, sizeof(b58));
    memset(b58, 0, sizeof(b58));        //clear for security
    return ret;
}


bool ptarm_keys_priv2pub(uint8_t *pPubKey, const uint8_t *pPrivKey)
{
    int ret;

    mbedtls_ecp_point P;
    mbedtls_mpi m;
    mbedtls_ecp_keypair keypair;

    mbedtls_ecp_point_init(&P);
    mbedtls_mpi_init(&m);
    mbedtls_ecp_keypair_init(&keypair);
    mbedtls_ecp_group_load(&(keypair.grp), MBEDTLS_ECP_DP_SECP256K1);

    //P:result, m:掛ける数値, grp.G:point
    ret = mbedtls_mpi_read_binary(&m, pPrivKey, PTARM_SZ_PRIVKEY);
    if (ret) {
        assert(0);
        goto LABEL_EXIT;
    }
    ret = mbedtls_ecp_mul(&keypair.grp, &P, &m, &keypair.grp.G, NULL, NULL);
    if (ret) {
        assert(0);
        goto LABEL_EXIT;
    }

    size_t sz;
    ret = mbedtls_ecp_point_write_binary(&keypair.grp, &P, MBEDTLS_ECP_PF_COMPRESSED, &sz, pPubKey, PTARM_SZ_PUBKEY);

LABEL_EXIT:
    mbedtls_ecp_keypair_free(&keypair);
    mbedtls_ecp_point_free(&P);
    mbedtls_mpi_lset(&m, 0);            //clear for security
    mbedtls_mpi_free(&m);

    return ret == 0;
}


bool ptarm_keys_pub2p2pkh(char *pAddr, const uint8_t *pPubKey)
{
    uint8_t pkh[PTARM_SZ_PUBKEYHASH];

    ptarm_util_hash160(pkh, pPubKey, PTARM_SZ_PUBKEY);
    return ptarm_util_keys_pkh2addr(pAddr, pkh, PTARM_PREF_P2PKH);
}


bool ptarm_keys_pub2p2wpkh(char *pWAddr, const uint8_t *pPubKey)
{
    bool ret;
    uint8_t pkh[PTARM_SZ_PUBKEYHASH];
    uint8_t pref;

    //BIP142のテストデータが非圧縮公開鍵だったので、やむなくこうした
    ptarm_util_hash160(pkh, pPubKey, (pPubKey[0] == 0x04) ? PTARM_SZ_PUBKEY_UNCOMP+1 : PTARM_SZ_PUBKEY);
    if (mNativeSegwit) {
        pref = PTARM_PREF_NATIVE;
    } else {
        ptarm_util_create_pkh2wpkh(pkh, pkh);
        pref = PTARM_PREF_P2SH;
    }
    ret = ptarm_util_keys_pkh2addr(pWAddr, pkh, pref);
    return ret;
}


bool ptarm_keys_addr2p2wpkh(char *pWAddr, const char *pAddr)
{
    bool ret;
        uint8_t pkh[PTARM_SZ_PUBKEYHASH];
        int pref;

        ret = ptarm_keys_addr2pkh(pkh, &pref, pAddr);
    if (!ret || (pref != PTARM_PREF_P2PKH)) {
        return false;
        }

    if (mNativeSegwit) {
        uint8_t hrp_type;

        switch (ptarm_get_chain()) {
        case PTARM_MAINNET:
            hrp_type = SEGWIT_ADDR_MAINNET;
            break;
        case PTARM_TESTNET:
            hrp_type = SEGWIT_ADDR_TESTNET;
            break;
        default:
            return false;
        }
        ret = segwit_addr_encode(pWAddr, hrp_type, 0x00, pkh, PTARM_SZ_HASH160);
    } else {
        ptarm_util_create_pkh2wpkh(pkh, pkh);
        ret = ptarm_util_keys_pkh2addr(pWAddr, pkh, PTARM_PREF_P2SH);
    }
    return ret;
}


bool ptarm_keys_wit2waddr(char *pWAddr, const ptarm_buf_t *pWitScript)
{
    bool ret;

    if (mNativeSegwit) {
        uint8_t sha[PTARM_SZ_SHA256];
        uint8_t hrp_type;

        switch (ptarm_get_chain()) {
        case PTARM_MAINNET:
            hrp_type = SEGWIT_ADDR_MAINNET;
            break;
        case PTARM_TESTNET:
            hrp_type = SEGWIT_ADDR_TESTNET;
            break;
        default:
            return false;
        }
        ptarm_util_sha256(sha, pWitScript->buf, pWitScript->len);
        ret = segwit_addr_encode(pWAddr, hrp_type, 0x00, sha, PTARM_SZ_SHA256);
    } else {
        uint8_t wit_prog[LNL_SZ_WITPROG_WSH];
        uint8_t pkh[PTARM_SZ_PUBKEYHASH];

        wit_prog[0] = 0x00;
        wit_prog[1] = PTARM_SZ_HASH256;
        ptarm_util_sha256(wit_prog + 2, pWitScript->buf, pWitScript->len);
        ptarm_util_hash160(pkh, wit_prog, sizeof(wit_prog));
        ret = ptarm_util_keys_pkh2addr(pWAddr, pkh, PTARM_PREF_P2SH);
    }
    return ret;
}


bool ptarm_keys_pubuncomp(uint8_t *pUncomp, const uint8_t *pPubKey)
{
    mbedtls_ecp_keypair keypair;
    mbedtls_ecp_keypair_init(&keypair);
    mbedtls_ecp_group_load(&(keypair.grp), MBEDTLS_ECP_DP_SECP256K1);

    int ret = ptarm_util_set_keypair(&keypair, pPubKey);
    if (!ret) {
        mbedtls_mpi_write_binary(&(keypair.Q.X), pUncomp, PTARM_SZ_PUBKEY - 1);
        mbedtls_mpi_write_binary(&(keypair.Q.Y), pUncomp + PTARM_SZ_PUBKEY - 1, PTARM_SZ_PUBKEY - 1);
    }
    mbedtls_ecp_keypair_free(&keypair);

    return ret == 0;
}


bool ptarm_keys_chkpriv(const uint8_t *pPrivKey)
{
    bool cmp;
    mbedtls_mpi priv;
    mbedtls_ecp_keypair keypair;
    mbedtls_ecp_keypair_init(&keypair);
    mbedtls_ecp_group_load(&(keypair.grp), MBEDTLS_ECP_DP_SECP256K1);

    mbedtls_mpi_init(&priv);
    mbedtls_mpi_read_binary(&priv, pPrivKey, PTARM_SZ_PRIVKEY);

    //pPrivKey = [0x01,  0xFFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFE BAAE DCE6 AF48 A03B BFD2 5E8C D036 4140]
    cmp = (mbedtls_mpi_cmp_int(&priv, (mbedtls_mpi_sint)0) == 1);
    if (cmp) {
        cmp = (mbedtls_mpi_cmp_mpi(&priv, &keypair.grp.N) == -1);
    }

    mbedtls_mpi_free(&priv);
    mbedtls_ecp_keypair_free(&keypair);

    return cmp;
}


bool ptarm_keys_chkpub(const uint8_t *pPubKey)
{
    mbedtls_ecp_keypair keypair;
    mbedtls_ecp_keypair_init(&keypair);
    mbedtls_ecp_group_load(&(keypair.grp), MBEDTLS_ECP_DP_SECP256K1);

    int ret = ptarm_util_set_keypair(&keypair, pPubKey);
    mbedtls_ecp_keypair_free(&keypair);

    return ret == 0;
}


bool ptarm_keys_create2of2(ptarm_buf_t *pRedeem, const uint8_t *pPubKey1, const uint8_t *pPubKey2)
{
    ptarm_buf_alloc(pRedeem, LNL_SZ_2OF2);

    uint8_t *p = pRedeem->buf;

    /*
     * OP_2
     *   21 (pubkey1[33])
     *   21 (pubkey2[33])
     * OP_2
     * OP_CHECKMULTISIG
     */
    *p++ = OP_2;
    *p++ = (uint8_t)PTARM_SZ_PUBKEY;
    memcpy(p, pPubKey1, PTARM_SZ_PUBKEY);
    p += PTARM_SZ_PUBKEY;
    *p++ = (uint8_t)PTARM_SZ_PUBKEY;
    memcpy(p, pPubKey2, PTARM_SZ_PUBKEY);
    p += PTARM_SZ_PUBKEY;
    *p++ = OP_2;
    *p++ = OP_CHECKMULTISIG;
    return true;
}


bool ptarm_keys_createmulti(ptarm_buf_t *pRedeem, const uint8_t *pPubKeys[], int Num, int M)
{
    ptarm_buf_alloc(pRedeem, 3 + Num * (PTARM_SZ_PUBKEY + 1));

    uint8_t *p = pRedeem->buf;

    /*
     * OP_n
     *   21 (pubkey1[33])
     *   ...
     *   21 (pubkeyn[33])
     * OP_m
     * OP_CHECKMULTISIG
     */
    *p++ = OP_x + M;
    for (int lp = 0; lp < Num; lp++) {
        *p++ = (uint8_t)PTARM_SZ_PUBKEY;
        memcpy(p, pPubKeys[lp], PTARM_SZ_PUBKEY);
        p += PTARM_SZ_PUBKEY;
    }
    *p++ = OP_x + Num;
    *p++ = OP_CHECKMULTISIG;
    return true;
}


bool ptarm_keys_addr2pkh(uint8_t *pPubKeyHash, int *pPrefix, const char *pAddr)
{
    uint8_t bin[3 + PTARM_SZ_HASH160 + 4];
    uint8_t *p_bin;
    uint8_t *p_pkh;
    size_t sz = sizeof(bin);
    bool ret = b58tobin(bin, &sz, pAddr, strlen(pAddr));
    if (ret) {
        if (sz == 1 + PTARM_SZ_HASH160 + 4) {
            p_bin = bin + 2;
            p_pkh = p_bin + 1;
            if (p_bin[0] == mPref[PTARM_PREF_P2PKH]) {
                *pPrefix = PTARM_PREF_P2PKH;
            } else if (p_bin[0] == mPref[PTARM_PREF_P2SH]) {
                *pPrefix = PTARM_PREF_P2SH;
            } else {
                ret = false;
            }
        } else {
            ret = false;
        }
    if (ret) {
        //CRC check
        uint8_t buf_sha256[PTARM_SZ_HASH256];
        ptarm_util_hash256(buf_sha256, p_bin, sz - 4);
        ret = memcmp(buf_sha256, p_bin + sz - 4, 4) == 0;
    }
    if (ret) {
            memcpy(pPubKeyHash, p_pkh, PTARM_SZ_HASH160);
        }
    } else {
        //BECH32?
        uint8_t witprog[40];
        size_t witprog_len;
        int witver;
        uint8_t hrp_type;
        switch (ptarm_get_chain()) {
        case PTARM_MAINNET:
            hrp_type = SEGWIT_ADDR_MAINNET;
            break;
        case PTARM_TESTNET:
            hrp_type = SEGWIT_ADDR_TESTNET;
            break;
        default:
            return false;
        }
        ret = segwit_addr_decode(&witver, witprog, &witprog_len, hrp_type, pAddr);
        if (ret && (witver == 0x00)) {
            //witver==0ではwitness programとpubKeyHashは同じ
            if (witprog_len == PTARM_SZ_HASH160) {
                *pPrefix = PTARM_PREF_NATIVE;
            } else if (witprog_len == PTARM_SZ_SHA256) {
                *pPrefix = PTARM_PREF_NATIVE_SH;
            } else {
                ret = false;
            }
            if (ret) {
                memcpy(pPubKeyHash, witprog, witprog_len);
            }
        } else {
            //witver!=0は未サポート
            ret = false;
        }
    }

    return ret;
}


bool ptarm_keys_addr2spk(ptarm_buf_t *pScriptPk, const char *pAddr)
{
    bool ret;
    uint8_t pkh[PTARM_SZ_PUBKEYHASH];

    int pref;
    ret = ptarm_keys_addr2pkh(pkh, &pref, pAddr);
    if (ret) {
        ptarm_util_create_scriptpk(pScriptPk, pkh, pref);
    }

    return ret;
}


bool ptarm_keys_spk2addr(char *pAddr, const ptarm_buf_t *pScriptPk)
{
    bool ret;
    const uint8_t *pkh;
    int prefix = spk2prefix(&pkh, pScriptPk);
    if (prefix != PTARM_PREF_MAX) {
        ptarm_util_keys_pkh2addr(pAddr, pkh, prefix);
        ret = true;
    } else {
        ret = false;
    }
    return ret;
}


/********************************************************************
 * private functions
 ********************************************************************/

/** scriptPubKeyからPREF変換
 *
 */
static int spk2prefix(const uint8_t **ppPkh, const ptarm_buf_t *pScriptPk)
{
    if ( (pScriptPk->len == 25) &&
         (pScriptPk->buf[0] == OP_DUP) &&
         (pScriptPk->buf[1] == OP_HASH160) &&
         (pScriptPk->buf[2] == PTARM_SZ_HASH160) &&
         (pScriptPk->buf[23] == OP_EQUALVERIFY) &&
         (pScriptPk->buf[24] == OP_CHECKSIG) ) {
        *ppPkh = pScriptPk->buf + 3;
        return PTARM_PREF_P2PKH;
    }
    else if ( (pScriptPk->len == 23) &&
         (pScriptPk->buf[0] == OP_HASH160) &&
         (pScriptPk->buf[1] == PTARM_SZ_HASH160) &&
         (pScriptPk->buf[22] == OP_EQUAL) ) {
        *ppPkh = pScriptPk->buf + 2;
        return PTARM_PREF_P2SH;
    }
    else if ( (pScriptPk->len == 22) &&
         (pScriptPk->buf[0] == 0x00) &&
         (pScriptPk->buf[1] == PTARM_SZ_HASH160) ) {
        *ppPkh = pScriptPk->buf + 2;
        return PTARM_PREF_NATIVE;
    }
    else if ( (pScriptPk->len == 34) &&
         (pScriptPk->buf[0] == 0x00) &&
         (pScriptPk->buf[1] == PTARM_SZ_SHA256) ) {
        *ppPkh = pScriptPk->buf + 2;
        return PTARM_PREF_NATIVE_SH;
    }
    return PTARM_PREF_MAX;
}
