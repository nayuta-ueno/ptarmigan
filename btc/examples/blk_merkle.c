#define LOG_TAG "ex"
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include "utl_log.h"
#include "utl_dbg.h"

#include "btc_crypto.h"
#include "btc_tx.h"
#include "btc.h"
#include "btc_script.h"
#include "btc_test_util.h"
#include "btc_buf.h"
#include "btc_tx_buf.h"

#include "blk_merkle.h"

int main(void)
{
#pragma pack(0)
    struct blk_header_t {
        uint32_t version;
        uint8_t prev_blkhash[32];
        uint8_t merkle_root[32];
        uint32_t timestamp;
        uint32_t nbits;
        uint32_t nonce;
        uint8_t txs[];
    };
#pragma pack()

    utl_log_init_stdout();
    struct blk_header_t *hdr = (struct blk_header_t *)BLK_DATA;
    printf("version=%08x\n", hdr->version);
    printf("prev_blkhash=");
    TXIDD(hdr->prev_blkhash);
    printf("merkle_root=");
    TXIDD(hdr->merkle_root);
    printf("timestamp=%08x\n", hdr->timestamp);
    printf("nBits=%08x\n", hdr->nbits);
    printf("nonce=%08x\n", hdr->nonce);
    size_t len = sizeof(BLK_DATA) - sizeof(struct blk_header_t);

    uint64_t txs;
    btc_buf_r_t rbuf;
    btc_buf_r_init(&rbuf, hdr->txs, len);
    bool ret = btc_tx_buf_r_read_varint(&rbuf, &txs);
    if (!ret) {
        return EXIT_FAILURE;
    }
    printf("tx_cnt=%lu\n", txs);
    struct txhash_t {
        uint8_t txid[BTC_SZ_TXID];
    };
    const uint64_t TXS = (txs / 2 + 1) * 2;
    struct txhash_t *txids = (struct txhash_t *)malloc(sizeof(struct txhash_t) * TXS);
    for (uint64_t cnt = 0; cnt < txs; cnt++) {
        btc_tx_t tx = BTC_TX_INIT;

        uint32_t txlen = btc_tx_read_with_len(&tx, btc_buf_r_get_pos(&rbuf), btc_buf_r_remains(&rbuf));
        if (txlen > 0) {
            btc_tx_txid(&tx, txids[cnt].txid);
            printf("%ld= ", cnt);
            TXIDD(txids[cnt].txid);
            btc_buf_r_seek(&rbuf, txlen);
            btc_tx_free(&tx);
        } else {
            printf("error(tx=#%ld)\n", cnt);
            break;
        }
    }
    return 0;
}
