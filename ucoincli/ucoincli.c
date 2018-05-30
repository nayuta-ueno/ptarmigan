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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <jansson.h>

#include "ucoind.h"
#include "conf.h"
#include "misc.h"
#include "segwit_addr.h"


/**************************************************************************
 * macros
 **************************************************************************/

#define M_OPTIONS_INIT  (0xff)
#define M_OPTIONS_CONN  (0xf0)
#define M_OPTIONS_EXEC  (2)
#define M_OPTIONS_STOP  (1)
#define M_OPTIONS_HELP  (0)
#define M_OPTIONS_ERR   (-1)

#define BUFFER_SIZE     (256 * 1024)

#define M_NEXT              ","
#define M_QQ(str)           "\"" str "\""
#define M_STR(item,value)   M_QQ(item) ":" M_QQ(value)
#define M_VAL(item,value)   M_QQ(item) ":" value


#define M_CHK_INIT      {\
    if (*pOption != M_OPTIONS_INIT) {           \
        printf("fail: too many options\n");     \
        *pOption = M_OPTIONS_HELP;              \
        return;                                 \
    }                                           \
}

#define M_CHK_CONN      {\
    if (*pOption != M_OPTIONS_CONN) {           \
        printf("need -c option first\n");       \
        *pOption = M_OPTIONS_HELP;              \
        return;                                 \
    }                                           \
}


/**************************************************************************
 * static variables
 **************************************************************************/

static char         mPeerAddr[INET6_ADDRSTRLEN];
static uint16_t     mPeerPort;
static char         mPeerNodeId[UCOIN_SZ_PUBKEY * 2 + 1];
static char         mBuf[BUFFER_SIZE];
static bool         mTcpSend;
static char         mAddr[256];
static char         mErrStr[256];


/********************************************************************
 * prototypes
 ********************************************************************/

static void optfunc_help(int *pOption, bool *pConn);
static void optfunc_test(int *pOption, bool *pConn);
static void optfunc_addr(int *pOption, bool *pConn);
static void optfunc_conn_param(int *pOption, bool *pConn);
static void optfunc_getinfo(int *pOption, bool *pConn);
static void optfunc_disconnect(int *pOption, bool *pConn);
static void optfunc_funding(int *pOption, bool *pConn);
static void optfunc_invoice(int *pOption, bool *pConn);
static void optfunc_erase(int *pOption, bool *pConn);
static void optfunc_listinvoice(int *pOption, bool *pConn);
static void optfunc_payment(int *pOption, bool *pConn);
static void optfunc_routepay(int *pOption, bool *pConn);
static void optfunc_routepay_prevskip(int *pOption, bool *pConn);
static void optfunc_close(int *pOption, bool *pConn);
static void optfunc_getlasterr(int *pOption, bool *pConn);
static void optfunc_debug(int *pOption, bool *pConn);
static void optfunc_getcommittx(int *pOption, bool *pConn);
static void optfunc_disable_autoconn(int *pOption, bool *pConn);
static void optfunc_remove_channel(int *pOption, bool *pConn);
static void optfunc_setfeerate(int *pOption, bool *pConn);

static void connect_rpc(void);
static void stop_rpc(void);
static void routepay(int *pOption, bool bPrevSkip);

static int msg_send(char *pRecv, const char *pSend, const char *pAddr, uint16_t Port, bool bSend);


static const struct {
    char        opt;
    void        (*func)(int *pOption, bool *pConn);
} OPTION_FUNCS[] = {
    { 'h', optfunc_help },
    { 't', optfunc_test },
    { 'a', optfunc_addr },

    { 'c', optfunc_conn_param },
    { 'l', optfunc_getinfo },
    { 'q', optfunc_disconnect },
    { 'f', optfunc_funding },
    { 'i', optfunc_invoice },
    { 'e', optfunc_erase },
    { 'm', optfunc_listinvoice },
    { 'p', optfunc_payment },
    { 'r', optfunc_routepay },
    { 'R', optfunc_routepay_prevskip },
    { 'x', optfunc_close },
    { 'w', optfunc_getlasterr },
    { 'd', optfunc_debug },
    { 'g', optfunc_getcommittx },
    { 's', optfunc_disable_autoconn },
    { 'X', optfunc_remove_channel },
    { 'b', optfunc_setfeerate },
};


/********************************************************************
 * public functions
 ********************************************************************/

int main(int argc, char *argv[])
{
#ifndef NETKIND
#error not define NETKIND
#endif
#if NETKIND==0
    ucoin_init(UCOIN_MAINNET, true);
#elif NETKIND==1
    ucoin_init(UCOIN_TESTNET, true);
#endif

    const struct option OPTIONS[] = {
        { "setfeerate", required_argument, NULL, 'b' },
        { 0, 0, 0, 0 }
    };

    int option = M_OPTIONS_INIT;
    bool conn = false;
    mAddr[0] = '\0';
    mTcpSend = true;
    int opt;
    while ((opt = getopt_long(argc, argv, "c:hta:lq::f:i:e:mp:r:R:xwd:gs:X:b:", OPTIONS, NULL)) != -1) {
        for (size_t lp = 0; lp < ARRAY_SIZE(OPTION_FUNCS); lp++) {
            if (opt == OPTION_FUNCS[lp].opt) {
                (*OPTION_FUNCS[lp].func)(&option, &conn);
                break;
            }
        }
    }

    if (option == M_OPTIONS_ERR) {
        printf("{ " M_QQ("error") ": {" M_QQ("code") ": -1," M_QQ("message") ":" M_QQ("%s") "} }\n", mErrStr);
        return -1;
    }
    if ((option == M_OPTIONS_INIT) || (option == M_OPTIONS_HELP) || (!conn && (option == M_OPTIONS_CONN))) {
        printf("[usage]\n");
        printf("\t%s [-t] [OPTIONS...] [JSON-RPC port(not ucoind port)]\n", argv[0]);
        printf("\t\t-h : help\n");
        printf("\t\t-t : test(not send command)\n");
        printf("\t\t-q : quit ucoind\n");
        printf("\t\t-l : list channels\n");
        printf("\t\t-i AMOUNT_MSAT : add preimage, and show payment_hash\n");
        printf("\t\t-e PAYMENT_HASH : erase payment_hash\n");
        printf("\t\t-e ALL : erase all payment_hash\n");
        printf("\t\t-r BOLT#11_INVOICE[,ADDITIONAL AMOUNT_MSAT][,ADDITIONAL MIN_FINAL_CLTV_EXPIRY] : payment(don't put a space before or after the comma)\n");
        printf("\t\t-R BOLT#11_INVOICE[,ADDITIONAL AMOUNT_MSAT][,ADDITIONAL MIN_FINAL_CLTV_EXPIRY] : payment keep prev skip channel(don't put a space before or after the comma)\n");
        printf("\t\t-m : show payment_hashs\n");
        printf("\t\t-s<1 or 0> : 1=stop auto channel connect\n");
        printf("\t\t-c PEER.CONF : connect node\n");
        printf("\t\t-c PEER NODE_ID or PEER.CONF -f FUND.CONF : funding\n");
        printf("\t\t-c PEER NODE_ID or PEER.CONF -x : mutual/unilateral close channel\n");
        printf("\t\t-c PEER NODE_ID or PEER.CONF -w : get last error\n");
        printf("\t\t-c PEER NODE_ID or PEER.CONF -q : disconnect node\n");
        printf("\n");
        printf("\t\t--setfeerate FEERATE_PER_KW : set feerate_per_kw\n");
        printf("\n");
        // printf("\t\t-a <IP address> : [debug]JSON-RPC send address\n");
        printf("\t\t-d VALUE : [debug]debug option\n");
        printf("\t\t\tb0 ... no update_fulfill_htlc\n");
        printf("\t\t\tb1 ... no closing transaction\n");
        printf("\t\t\tb2 ... force payment_preimage mismatch\n");
        printf("\t\t\tb3 ... no node auto connect\n");
        printf("\t\t-c PEER NODE_ID or PEER.CONF -g : [debug]get commitment transaction\n");
        printf("\t\t-X CHANNEL_ID : [debug]delete channel from DB\n");
        return -1;
    }

    if (conn) {
        connect_rpc();
    }

    uint16_t port;
    if (optind == argc) {
        port = 9736;
    } else {
        port = (uint16_t)atoi(argv[optind]);
    }

    int ret = msg_send(mBuf, mBuf, mAddr, port, mTcpSend);

    ucoin_term();

    return ret;
}


/********************************************************************
 * commands
 ********************************************************************/

static void optfunc_help(int *pOption, bool *pConn)
{
    (void)pConn;

    *pOption = M_OPTIONS_HELP;
}


static void optfunc_test(int *pOption, bool *pConn)
{
    (void)pOption; (void)pConn;

    mTcpSend = false;
}


static void optfunc_addr(int *pOption, bool *pConn)
{
    (void)pOption; (void)pConn;

    strcpy(mAddr, optarg);
}


static void optfunc_conn_param(int *pOption, bool *pConn)
{
    if (*pOption != M_OPTIONS_INIT) {
        printf("fail: '-c' must first\n");
        *pOption = M_OPTIONS_HELP;
        return;
    }

    size_t optlen = strlen(optarg);
    peer_conf_t peer;
    bool bret = load_peer_conf(optarg, &peer);
    if (bret) {
        //peer.conf
        *pConn = true;
        strcpy(mPeerAddr, peer.ipaddr);
        mPeerPort = peer.port;
        ucoin_util_bin2str(mPeerNodeId, peer.node_id, UCOIN_SZ_PUBKEY);
        *pOption = M_OPTIONS_CONN;
    } else if (optlen >= (UCOIN_SZ_PUBKEY * 2 + 1 + 7 + 1 + 1)) {
        // <pubkey>@<ipaddr>:<port>
        // (33 * 2)@x.x.x.x:x
        int results = sscanf(optarg, "%66s@%" SZ_IPV4_LEN_STR "[^:]:%" SCNu16,
            mPeerNodeId,
            mPeerAddr,
            &mPeerPort);
        printf("id: %s\n", mPeerNodeId);
        printf("addr: %s\n", mPeerAddr);
        printf("port: %" PRIu16 "\n", mPeerPort);
        if (results == 3) {
            *pConn = true;
            *pOption = M_OPTIONS_CONN;
        } else {
            printf("fail: peer configuration file\n");
            *pOption = M_OPTIONS_HELP;
        }
    } else if (optlen == UCOIN_SZ_PUBKEY * 2) {
        //node_idを直で指定した可能性あり(connectとしては使用できない)
        strcpy(mPeerAddr, "0.0.0.0");
        mPeerPort = 0;
        strcpy(mPeerNodeId, optarg);
        *pOption = M_OPTIONS_CONN;
    } else {
        printf("fail: peer configuration file\n");
        *pOption = M_OPTIONS_HELP;
    }
}


static void optfunc_getinfo(int *pOption, bool *pConn)
{
    (void)pConn;

    M_CHK_INIT

    snprintf(mBuf, BUFFER_SIZE,
        "{"
            M_STR("method", "getinfo") M_NEXT
            M_QQ("params") ":[]"
        "}");

    *pOption = M_OPTIONS_EXEC;
}


static void optfunc_disconnect(int *pOption, bool *pConn)
{
    if (*pOption == M_OPTIONS_CONN) {
        //特定接続を切る
        snprintf(mBuf, BUFFER_SIZE,
            "{"
                M_STR("method", "disconnect") M_NEXT
                M_QQ("params") ":[ "
                    //peer_nodeid, peer_addr, peer_port
                    M_QQ("%s") "," M_QQ("%s") ",%d"
                " ]"
            "}",
                mPeerNodeId, mPeerAddr, mPeerPort);

        *pOption = M_OPTIONS_EXEC;
        *pConn = false;
    } else {
        //ucoind終了
        stop_rpc();
        *pOption = M_OPTIONS_STOP;
    }
}


static void optfunc_funding(int *pOption, bool *pConn)
{
    M_CHK_CONN

    funding_conf_t fundconf;
    bool bret = load_funding_conf(optarg, &fundconf);
    if (bret) {
        char txid[UCOIN_SZ_TXID * 2 + 1];

        ucoin_util_bin2str_rev(txid, fundconf.txid, UCOIN_SZ_TXID);
        snprintf(mBuf, BUFFER_SIZE,
            "{"
                M_STR("method", "fund") M_NEXT
                M_QQ("params") ":[ "
                    //peer_nodeid, peer_addr, peer_port
                    M_QQ("%s") "," M_QQ("%s") ",%d,"
                    //txid, txindex, signaddr, funding_sat, push_sat
                    M_QQ("%s") ",%d," M_QQ("%s") ",%" PRIu64 ",%" PRIu64 ",%" PRIu32
                " ]"
            "}",
                mPeerNodeId, mPeerAddr, mPeerPort,
                txid, fundconf.txindex, fundconf.signaddr,
                fundconf.funding_sat, fundconf.push_sat, fundconf.feerate_per_kw);

        *pConn = false;
        *pOption = M_OPTIONS_EXEC;
    } else {
        printf("fail: funding configuration file\n");
        *pOption = M_OPTIONS_HELP;
    }
}


static void optfunc_invoice(int *pOption, bool *pConn)
{
    (void)pConn;

    M_CHK_INIT

    errno = 0;
    uint64_t amount = (uint64_t)strtoull(optarg, NULL, 10);
    if (errno == 0) {
        snprintf(mBuf, BUFFER_SIZE,
            "{"
                M_STR("method", "invoice") M_NEXT
                M_QQ("params") ":[ "
                    //invoice
                    "%" PRIu64
                " ]"
            "}",
                amount);

        *pOption = M_OPTIONS_EXEC;
    } else {
        sprintf(mErrStr, "%s", strerror(errno));
        *pOption = M_OPTIONS_ERR;
    }
}


static void optfunc_erase(int *pOption, bool *pConn)
{
    (void)pConn;

    M_CHK_INIT

    const char *pPaymentHash = NULL;
    if (strcmp(optarg, "ALL") == 0) {
        pPaymentHash = "";
    } else if (strlen(optarg) == LN_SZ_HASH * 2) {
        pPaymentHash = optarg;
    } else {
        //error
    }
    if (pPaymentHash != NULL) {
        snprintf(mBuf, BUFFER_SIZE,
            "{"
                M_STR("method", "eraseinvoice") M_NEXT
                M_QQ("params") ":[ "
                    M_QQ("%s")
                " ]"
            "}",
                pPaymentHash);

        *pOption = M_OPTIONS_EXEC;
    } else {
        strcpy(mErrStr, "invalid param");
        *pOption = M_OPTIONS_ERR;
    }
}


static void optfunc_listinvoice(int *pOption, bool *pConn)
{
    (void)pConn;

    M_CHK_INIT

    snprintf(mBuf, BUFFER_SIZE,
        "{"
            M_STR("method", "listinvoice") M_NEXT
            M_QQ("params") ":[]"
        "}");
    *pOption = M_OPTIONS_EXEC;
}


static void optfunc_payment(int *pOption, bool *pConn)
{
    (void)pConn;

    M_CHK_INIT

    payment_conf_t payconf;
    const char *path = strtok(optarg, ",");
    const char *hash = strtok(NULL, ",");
    bool bret = load_payment_conf(path, &payconf);
    if (hash) {
        bret &= misc_str2bin(payconf.payment_hash, LN_SZ_HASH, hash);
    }
    if (!bret) {
        strcpy(mErrStr, "payment configuration file");
        *pOption = M_OPTIONS_ERR;
        return;
    }

    char payhash[LN_SZ_HASH * 2 + 1];
    //node_id(33*2),short_channel_id(8*2),amount(21),cltv(5)
    char forward[UCOIN_SZ_PUBKEY*2 + sizeof(uint64_t)*2 + 21 + 5 + 50];

    ucoin_util_bin2str(payhash, payconf.payment_hash, LN_SZ_HASH);
    snprintf(mBuf, BUFFER_SIZE,
        "{"
            M_STR("method", "PAY") M_NEXT
            M_QQ("params") ":[ "
                //payment_hash, hop_num
                M_QQ("%s") ",%d, [\n",
            payhash, payconf.hop_num);

    for (int lp = 0; lp < payconf.hop_num; lp++) {
        char node_id[UCOIN_SZ_PUBKEY * 2 + 1];

        ucoin_util_bin2str(node_id, payconf.hop_datain[lp].pubkey, UCOIN_SZ_PUBKEY);
        snprintf(forward, sizeof(forward), "[" M_QQ("%s") "," M_QQ("%" PRIx64) ",%" PRIu64 ",%d]",
                node_id,
                payconf.hop_datain[lp].short_channel_id,
                payconf.hop_datain[lp].amt_to_forward,
                payconf.hop_datain[lp].outgoing_cltv_value
        );
        strcat(mBuf, forward);
        if (lp != payconf.hop_num - 1) {
            strcat(mBuf, ",");
        }
    }
    strcat(mBuf, "] ]}");

    *pOption = M_OPTIONS_EXEC;
}


/* BOLT#11 invoiceによる支払い
 *
 *  前回skipしたshort_channel_idをクリアする
 */
static void optfunc_routepay(int *pOption, bool *pConn)
{
    (void)pConn;

    M_CHK_INIT

    routepay(pOption, false);
}


/* BOLT#11 invoiceによる支払い
 *
 *  前回skipしたshort_channel_idをクリアしない
 */
static void optfunc_routepay_prevskip(int *pOption, bool *pConn)
{
    (void)pConn;

    M_CHK_INIT

    routepay(pOption, true);
}


static void optfunc_close(int *pOption, bool *pConn)
{
    M_CHK_CONN

    snprintf(mBuf, BUFFER_SIZE,
        "{"
            M_STR("method", "close") M_NEXT
            M_QQ("params") ":[ "
                //peer_nodeid, peer_addr, peer_port
                M_QQ("%s") "," M_QQ("%s") ",%d"
            " ]"
        "}",
            mPeerNodeId, mPeerAddr, mPeerPort);

    *pConn = false;
    *pOption = M_OPTIONS_EXEC;
}


static void optfunc_getlasterr(int *pOption, bool *pConn)
{
    M_CHK_CONN

    snprintf(mBuf, BUFFER_SIZE,
        "{"
            M_STR("method", "getlasterror") M_NEXT
            M_QQ("params") ":[ "
                //peer_nodeid, peer_addr, peer_port
                M_QQ("%s") "," M_QQ("%s") ",%d"
            " ]"
        "}",
            mPeerNodeId, mPeerAddr, mPeerPort);

    *pConn = false;
    *pOption = M_OPTIONS_EXEC;
}


static void optfunc_debug(int *pOption, bool *pConn)
{
    (void)pConn;

    int debug = (int)strtol(optarg, NULL, 10);
    snprintf(mBuf, BUFFER_SIZE,
        "{"
            M_STR("method", "debug") M_NEXT
            M_QQ("params") ":[ %d ]"
        "}", debug);

    *pOption = M_OPTIONS_EXEC;
}


static void optfunc_getcommittx(int *pOption, bool *pConn)
{
    M_CHK_CONN

    snprintf(mBuf, BUFFER_SIZE,
        "{"
            M_STR("method", "getcommittx") M_NEXT
            M_QQ("params") ":[ "
                //peer_nodeid, peer_addr, peer_port
                M_QQ("%s") "," M_QQ("%s") ",%d"
            " ]"
        "}",
            mPeerNodeId, mPeerAddr, mPeerPort);

    *pConn = false;
    *pOption = M_OPTIONS_EXEC;
}


static void optfunc_disable_autoconn(int *pOption, bool *pConn)
{
    (void)pConn;

    M_CHK_INIT

    if ((strlen(optarg) == 1) && ((optarg[0] == '1') || (optarg[0] == '0'))) {
        snprintf(mBuf, BUFFER_SIZE,
            "{"
                M_STR("method", "disautoconn") M_NEXT
                M_QQ("params") ":[ \"%s\" ]"
            "}", optarg);

        *pOption = M_OPTIONS_EXEC;
    } else {
        printf("fail: invalid option\n");
        *pOption = M_OPTIONS_HELP;
    }
}


static void optfunc_remove_channel(int *pOption, bool *pConn)
{
    (void)pConn;

    M_CHK_INIT

    if (strlen(optarg) != LN_SZ_CHANNEL_ID * 2) {
        printf("fail: invalid option: %s\n", optarg);
        *pOption = M_OPTIONS_HELP;
        return;
    }

    snprintf(mBuf, BUFFER_SIZE,
        "{"
            M_STR("method", "removechannel") M_NEXT
            M_QQ("params") ":[ "
                M_QQ("%s")
            " ]"
        "}",
            optarg);

    *pOption = M_OPTIONS_EXEC;
}


static void optfunc_setfeerate(int *pOption, bool *pConn)
{
    (void)pConn;

    M_CHK_INIT

    errno = 0;
    uint64_t feerate_per_kw = strtoull(optarg, NULL, 10);
    if (feerate_per_kw > UINT32_MAX) {
        strcpy(mErrStr, "feerate_per_kw too high");
        *pOption = M_OPTIONS_ERR;
        return;
    }
    if (errno == 0) {
        snprintf(mBuf, BUFFER_SIZE,
            "{"
                M_STR("method", "setfeerate") M_NEXT
                M_QQ("params") ":[ "
                    //feerate_per_kw
                    "%" PRIu32
                " ]"
            "}",
                (uint32_t)feerate_per_kw);

        *pOption = M_OPTIONS_EXEC;
    } else {
        sprintf(mErrStr, "%s", strerror(errno));
        *pOption = M_OPTIONS_ERR;
    }
}


/********************************************************************
 * others
 ********************************************************************/

static void connect_rpc(void)
{
    snprintf(mBuf, BUFFER_SIZE,
        "{"
            M_STR("method", "connect") M_NEXT
            M_QQ("params") ":[ "
                //peer_nodeid, peer_addr, peer_port
                M_QQ("%s") "," M_QQ("%s") ",%d"
            " ]"
        "}",
            mPeerNodeId, mPeerAddr, mPeerPort);
}


static void stop_rpc(void)
{
    snprintf(mBuf, BUFFER_SIZE,
        "{"
            M_STR("method", "stop") M_NEXT
            M_QQ("params") ":[]"
        "}");
}


/**
 *  @param[out]     pOption
 *  @param[in]      bPrevSkip       true:前回skipしたshort_channel_idを維持する
 */
static void routepay(int *pOption, bool bPrevSkip)
{
    const char *invoice = strtok(optarg, ",");
    const char *add_amount_str = strtok(NULL, ",");
    const char *cltv_offset_str = strtok(NULL, ",");


/////////////////////

    //確認用のログ出力
    ln_invoice_t *p_invoice_data = NULL;
    bool bret = ln_invoice_decode(&p_invoice_data, invoice);
    if (!bret) {
        sprintf(mErrStr, "fail decode invoice");
        *pOption = M_OPTIONS_ERR;
        return;
    }

    printf("---------------------------------\n");
    switch (p_invoice_data->hrp_type) {
    case LN_INVOICE_MAINNET:
        printf("blockchain: bitcoin mainnet\n");
        break;
    case LN_INVOICE_TESTNET:
        printf("blockchain: bitcoin testnet\n");
        break;
    case LN_INVOICE_REGTEST:
        printf("blockchain: bitcoin regtest\n");
        break;
    default:
        printf("unknown hrp_type\n");
    }
    printf("amount_msat=%" PRIu64 "\n", p_invoice_data->amount_msat);
    time_t tm = (time_t)p_invoice_data->timestamp;
    printf("timestamp= %" PRIu64 " : %s", (uint64_t)p_invoice_data->timestamp, ctime(&tm));
    printf("min_final_cltv_expiry=%u\n", p_invoice_data->min_final_cltv_expiry);
    printf("payee=");
    for (int lp = 0; lp < UCOIN_SZ_PUBKEY; lp++) {
        printf("%02x", p_invoice_data->pubkey[lp]);
    }
    printf("\n");
    printf("payment_hash=");
    for (int lp = 0; lp < UCOIN_SZ_SHA256; lp++) {
        printf("%02x", p_invoice_data->payment_hash[lp]);
    }
    printf("\n");
    if (p_invoice_data->r_field_num > 0) {
        for (int lp = 0; lp < p_invoice_data->r_field_num; lp++) {
            printf("    ------------------------\n");
            printf("    ");
            for (int lp2 = 0; lp2 < UCOIN_SZ_PUBKEY; lp2++) {
                printf("%02x", p_invoice_data->r_field[lp].node_id[lp2]);
            }
            printf("\n");
            printf("    short_channel_id=%" PRIx64 "\n", p_invoice_data->r_field[lp].short_channel_id);
            printf("    fee_base_msat=%" PRIu32 "\n", p_invoice_data->r_field[lp].fee_base_msat);
            printf("    fee_proportional_millionths=%" PRIu32 "\n", p_invoice_data->r_field[lp].fee_prop_millionths);
            printf("    cltv_expiry_delta=%" PRIu16 "\n", p_invoice_data->r_field[lp].cltv_expiry_delta);
        }
        printf("    ------------------------\n");
    }
    printf("---------------------------------\n");

////////////////////

    uint64_t add_amount_msat = 0;
    if (add_amount_str != NULL) {
        //additional amount_msat
        //  invoiceで要求されたamountに追加で支払えるようにしている
        errno = 0;
        add_amount_msat = (uint64_t)strtoull(add_amount_str, NULL, 10);
        if (errno != 0) {
            sprintf(mErrStr, "%s", strerror(errno));
            *pOption = M_OPTIONS_ERR;
        }
    }

    uint32_t cltv_offset = 0;
    if (cltv_offset_str != NULL) {
        errno = 0;
        cltv_offset = (uint32_t)strtoull(cltv_offset_str, NULL, 10);
        if (errno != 0) {
            sprintf(mErrStr, "%s", strerror(errno));
            *pOption = M_OPTIONS_ERR;
        }
    }

    if (*pOption != M_OPTIONS_ERR) {
        const char *p_method;
        if (bPrevSkip) {
            p_method = "routepay_cont";
        } else {
            p_method = "routepay";
        }
        snprintf(mBuf, BUFFER_SIZE,
            "{"
                M_STR("method", "%s") M_NEXT
                M_QQ("params") ":[ "
                    //bolt11, add_amount_msat, cltv_offset
                    M_QQ("%s") ",%" PRIu64 ", %" PRIu32 "]}",
                p_method,
                invoice, add_amount_msat, cltv_offset);

        *pOption = M_OPTIONS_EXEC;
    }
}


static int msg_send(char *pRecv, const char *pSend, const char *pAddr, uint16_t Port, bool bSend)
{
    int retval = -1;

    if (bSend) {
        struct sockaddr_in sv_addr;

        //fprintf(stderr, "%s\n", pSend);
        int sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            fprintf(stderr, "fail socket: %s\n", strerror(errno));
            return retval;
        }
        memset(&sv_addr, 0, sizeof(sv_addr));
        sv_addr.sin_family = AF_INET;
        if (strlen(pAddr) == 0) {
            sv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        } else {
            sv_addr.sin_addr.s_addr = inet_addr(pAddr);
        }
        sv_addr.sin_port = htons(Port);
        retval = connect(sock, (struct sockaddr *)&sv_addr, sizeof(sv_addr));
        if (retval < 0) {
            fprintf(stderr, "fail connect: %s\n", strerror(errno));
            close(sock);
            return retval;
        }
        write(sock, pSend, strlen(pSend));
        ssize_t len = read(sock, pRecv, BUFFER_SIZE);
        if (len > 0) {
            retval = -1;
            pRecv[len] = '\0';
            printf("%s\n", pRecv);

            json_t *p_root;
            json_error_t error;
            p_root = json_loads(pRecv, 0, &error);
            if (p_root) {
                json_t *p_result;
                p_result = json_object_get(p_root, "result");
                if (p_result) {
                    //戻り値正常
                    retval = 0;
                }
            }
        } else if (len < 0) {
            fprintf(stderr, "fail read: %s\n", strerror(errno));
        }
        close(sock);
    } else {
        fprintf(stdout, "sendto: %s:%" PRIu16 "\n", (strlen(pAddr) != 0) ? pAddr : "localhost", Port);
        fprintf(stdout, "%s\n", pSend);
        retval = 0;
    }

    return retval;
}
