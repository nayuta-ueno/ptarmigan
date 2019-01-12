#include "gtest/gtest.h"
#include <string.h>
#include "tests/fff.h"
DEFINE_FFF_GLOBALS;


extern "C" {
#undef LOG_TAG
#include "../../utl/utl_thread.c"
#undef LOG_TAG
#include "../../utl/utl_log.c"
#include "../../utl/utl_dbg.c"
#include "../../utl/utl_buf.c"
#include "../../utl/utl_push.c"
#include "../../utl/utl_time.c"
#include "../../utl/utl_int.c"
#include "../../utl/utl_str.c"
#undef LOG_TAG
#include "../../btc/btc.c"
#include "../../btc/btc_buf.c"
#include "../../btc/btc_extkey.c"
#include "../../btc/btc_keys.c"
#include "../../btc/btc_sw.c"
#include "../../btc/btc_sig.c"
#include "../../btc/btc_script.c"
#include "../../btc/btc_tx.c"
#include "../../btc/btc_tx_buf.c"
#include "../../btc/btc_crypto.c"
#include "../../btc/segwit_addr.c"
#include "../../btc/btc_segwit_addr.c"
#include "../../btc/btc_test_util.c"

#undef LOG_TAG
#include "ln_derkey.c"
#include "ln_misc.c"
#include "ln_msg_anno.c"
#include "ln_msg_close.c"
//#include "ln_msg_establish.c"
// #include "ln_msg_normalope.c"
//#include "ln_msg_setupctl.c"
#include "ln_node.c"
// #include "ln_onion.c"
// #include "ln_script.c"
#include "ln_enc_auth.c"
#include "ln_signer.c"
// #include "ln_segwit_addr.c"
// #include "ln_print.c"

#include "ln.c"
}

////////////////////////////////////////////////////////////////////////
//FAKE関数

FAKE_VALUE_FUNC(bool, ln_msg_init_read, ln_init_t *, const uint8_t *, uint16_t );
FAKE_VALUE_FUNC(bool, ln_msg_error_write, utl_buf_t *, const ln_error_t *);
FAKE_VALUE_FUNC(bool, ln_msg_error_read, ln_error_t *, const uint8_t *, uint16_t );


////////////////////////////////////////////////////////////////////////

class ln: public testing::Test {
protected:
    virtual void SetUp() {
        //utl_log_init_stderr();
        RESET_FAKE(ln_msg_init_read)
        RESET_FAKE(ln_msg_error_write)
        RESET_FAKE(ln_msg_error_read)

        ln_msg_init_read_fake.return_val = true;
        ln_msg_error_write_fake.return_val = true;
        ln_msg_error_read_fake.return_val = true;
        utl_dbg_malloc_cnt_reset();
        btc_init(BTC_TESTNET, true);
    }

    virtual void TearDown() {
        ln_node_term();
        btc_term();
        ASSERT_EQ(0, utl_dbg_malloc_cnt());
    }

public:
    static void DumpBin(const uint8_t *pData, uint16_t Len)
    {
        for (uint16_t lp = 0; lp < Len; lp++) {
            printf("%02x", pData[lp]);
        }
        printf("\n");
    }
    static bool DumpCheck(const void *pData, uint32_t Len, uint8_t Fill)
    {
        bool ret = true;
        const uint8_t *p = (const uint8_t *)pData;
        for (uint32_t lp = 0; lp < Len; lp++) {
            if (p[lp] != Fill) {
                ret = false;
                break;
            }
        }
        return ret;
    }
    static void LnCallbackType(ln_self_t *self, ln_cb_t type, void *p_param) {
        (void)self; (void)p_param;
        const char *p_str;
        switch (type) {
        case LN_CB_ERROR: p_str = "LN_CB_ERROR"; break;
        case LN_CB_INIT_RECV: p_str = "LN_CB_INIT_RECV"; break;
        case LN_CB_REESTABLISH_RECV: p_str = "LN_CB_REESTABLISH_RECV"; break;
        case LN_CB_SIGN_FUNDINGTX_REQ: p_str = "LN_CB_SIGN_FUNDINGTX_REQ"; break;
        case LN_CB_FUNDINGTX_WAIT: p_str = "LN_CB_FUNDINGTX_WAIT"; break;
        case LN_CB_FUNDINGLOCKED_RECV: p_str = "LN_CB_FUNDINGLOCKED_RECV"; break;
        case LN_CB_UPDATE_ANNODB: p_str = "LN_CB_UPDATE_ANNODB"; break;
        case LN_CB_ADD_HTLC_RECV_PREV: p_str = "LN_CB_ADD_HTLC_RECV_PREV"; break;
        case LN_CB_ADD_HTLC_RECV: p_str = "LN_CB_ADD_HTLC_RECV"; break;
        case LN_CB_FWD_ADDHTLC_START: p_str = "LN_CB_FWD_ADDHTLC_START"; break;
        case LN_CB_FULFILL_HTLC_RECV: p_str = "LN_CB_FULFILL_HTLC_RECV"; break;
        case LN_CB_FAIL_HTLC_RECV: p_str = "LN_CB_FAIL_HTLC_RECV"; break;
        case LN_CB_REV_AND_ACK_EXCG: p_str = "LN_CB_REV_AND_ACK_EXCG"; break;
        case LN_CB_PAYMENT_RETRY: p_str = "LN_CB_PAYMENT_RETRY"; break;
        case LN_CB_UPDATE_FEE_RECV: p_str = "LN_CB_UPDATE_FEE_RECV"; break;
        case LN_CB_SHUTDOWN_RECV: p_str = "LN_CB_SHUTDOWN_RECV"; break;
        case LN_CB_CLOSED_FEE: p_str = "LN_CB_CLOSED_FEE"; break;
        case LN_CB_CLOSED: p_str = "LN_CB_CLOSED"; break;
        case LN_CB_SEND_REQ: p_str = "LN_CB_SEND_REQ"; break;
        case LN_CB_SEND_QUEUE: p_str = "LN_CB_SEND_QUEUE"; break;
        case LN_CB_GET_LATEST_FEERATE: p_str = "LN_CB_GET_LATEST_FEERATE"; break;
        case LN_CB_GETBLOCKCOUNT: p_str = "LN_CB_GETBLOCKCOUNT"; break;
        default:
            p_str = "unknown";
        }
        printf("*** callback: %s(%d)\n", p_str, type);
    }
    static void LnInit(ln_self_t *self)
    {
        uint8_t seed[LN_SZ_SEED];
        ln_anno_prm_t annoprm;

        memset(self, 0xcc, sizeof(ln_self_t));
        memset(seed, 1, sizeof(seed));
        annoprm.cltv_expiry_delta = 10;
        annoprm.htlc_minimum_msat = 1000;
        annoprm.fee_base_msat = 20;
        annoprm.fee_prop_millionths = 200;

        ln_init(self, seed, &annoprm, (ln_callback_t)0x123456);
        self->commit_local.dust_limit_sat = BTC_DUST_LIMIT;
        self->commit_local.htlc_minimum_msat = 0;
        self->commit_local.max_accepted_htlcs = 10;
        self->commit_remote.dust_limit_sat = BTC_DUST_LIMIT;
        self->commit_remote.htlc_minimum_msat = 0;
        self->commit_remote.max_accepted_htlcs = 10;
        self->our_msat = 1000000;
        self->their_msat = 1000000;
        btc_tx_init(&self->tx_funding);
        utl_buf_init(&self->redeem_fund);
        self->p_callback = LnCallbackType;
    }
};

////////////////////////////////////////////////////////////////////////

TEST_F(ln, recv_init_ok)
{
    ln_self_t self;
    LnInit(&self);

    static bool b_called;
    static bool b_initial_routing_sync;
    class dummy {
    public:
        // static bool ln_msg_init_read(ln_init_t *pMsg, const uint8_t *pData, uint16_t Len) {
        //     return false;
        // }
        static void callback(ln_self_t *self, ln_cb_t type, void *p_param) {
            (void)self;
            if (type == LN_CB_INIT_RECV) {
                b_called = true;
                b_initial_routing_sync = *(bool *)p_param;
            }
        }
    };
    self.p_callback = dummy::callback;

    bool ret = recv_init(&self, NULL, 0);
    ASSERT_TRUE(ret);
    ASSERT_EQ(0x00, self.lfeature_remote);
    ASSERT_EQ(M_INIT_FLAG_RECV, self.init_flag);
    ASSERT_TRUE(b_called);
    ASSERT_FALSE(b_initial_routing_sync);
}


TEST_F(ln, recv_init_fail)
{
    ln_self_t self;
    LnInit(&self);

    static bool b_called;
    static bool b_initial_routing_sync;
    class dummy {
    public:
        // static bool ln_msg_init_read(ln_init_t *pMsg, const uint8_t *pData, uint16_t Len) {
        //     return false;
        // }
        static void callback(ln_self_t *self, ln_cb_t type, void *p_param) {
            (void)self;
            if (type == LN_CB_INIT_RECV) {
                b_called = true;
                b_initial_routing_sync = *(bool *)p_param;
            }
        }
    };
    ln_msg_init_read_fake.return_val = false;
    self.p_callback = dummy::callback;

    bool ret = recv_init(&self, NULL, 0);
    ASSERT_FALSE(ret);
    ASSERT_FALSE(b_called);
}


TEST_F(ln, recv_init_gf1)
{
    ln_self_t self;
    LnInit(&self);

    static bool b_called;
    static bool b_initial_routing_sync;
    static uint8_t gf;
    class dummy {
    public:
        static bool ln_msg_init_read(ln_init_t *pMsg, const uint8_t *pData, uint16_t Len) {
            utl_buf_alloc(&pMsg->globalfeatures, 1);
            pMsg->globalfeatures.buf[0] = gf;
            return true;
        }
        static void callback(ln_self_t *self, ln_cb_t type, void *p_param) {
            (void)self;
            if (type == LN_CB_INIT_RECV) {
                b_called = true;
                b_initial_routing_sync = *(bool *)p_param;
            }
        }
    };
    self.p_callback = dummy::callback;
    ln_msg_init_read_fake.custom_fake = dummy::ln_msg_init_read;

    bool ret;
    
    for (int lp = 1; lp <= 0x0f; lp++) {
        self.init_flag = 0;
        self.lfeature_remote = 0;
        b_called = false;
        b_initial_routing_sync = false;

        //odd bits(7, 5, 3, 1)
        //          abcd
        //      a0b0c0d0
        gf = (lp & 0x08) << 4 | (lp & 0x04) << 3 | (lp & 0x02) << 2 | (lp & 0x01) << 1;
        ret = recv_init(&self, NULL, 0);
        ASSERT_TRUE(ret);
        ASSERT_EQ(0x00, self.lfeature_remote);
        ASSERT_EQ(M_INIT_FLAG_RECV, self.init_flag);
        ASSERT_TRUE(b_called);
        ASSERT_FALSE(b_initial_routing_sync);
    }
}


TEST_F(ln, recv_init_gf2)
{
    ln_self_t self;
    LnInit(&self);

    static bool b_called;
    static bool b_initial_routing_sync;
    static uint8_t gf;
    class dummy {
    public:
        static bool ln_msg_init_read(ln_init_t *pMsg, const uint8_t *pData, uint16_t Len) {
            utl_buf_alloc(&pMsg->globalfeatures, 1);
            pMsg->globalfeatures.buf[0] = gf;
            return true;
        }
        static void callback(ln_self_t *self, ln_cb_t type, void *p_param) {
            (void)self;
            if (type == LN_CB_INIT_RECV) {
                b_called = true;
                b_initial_routing_sync = *(bool *)p_param;
            }
        }
    };
    self.p_callback = dummy::callback;
    ln_msg_init_read_fake.custom_fake = dummy::ln_msg_init_read;

    bool ret;
    
    for (int lp = 1; lp <= 0x0f; lp++) {
        self.init_flag = 0;
        self.lfeature_remote = 0;
        b_called = false;
        b_initial_routing_sync = false;

        //even bits(6, 4, 2, 0)
        //          abcd
        //      0a0b0c0d
        gf = (lp & 0x08) << 3 | (lp & 0x04) << 2 | (lp & 0x02) << 1 | (lp & 0x01);
        ret = recv_init(&self, NULL, 0);
        ASSERT_FALSE(ret);
        ASSERT_EQ(0x00, self.lfeature_remote);
        ASSERT_EQ(0, self.init_flag);
        ASSERT_FALSE(b_called);
        ASSERT_FALSE(b_initial_routing_sync);
    }
}


TEST_F(ln, recv_init_lf1)
{
    ln_self_t self;
    LnInit(&self);

    static bool b_called;
    static bool b_initial_routing_sync;
    static uint8_t lf;
    class dummy {
    public:
        static bool ln_msg_init_read(ln_init_t *pMsg, const uint8_t *pData, uint16_t Len) {
            utl_buf_alloc(&pMsg->localfeatures, 1);
            pMsg->localfeatures.buf[0] = lf;
            return true;
        }
        static void callback(ln_self_t *self, ln_cb_t type, void *p_param) {
            (void)self;
            if (type == LN_CB_INIT_RECV) {
                b_called = true;
                b_initial_routing_sync = *(bool *)p_param;
            }
        }
    };
    self.p_callback = dummy::callback;
    ln_msg_init_read_fake.custom_fake = dummy::ln_msg_init_read;

    bool ret;
    
    for (int lp = 1; lp <= 0x0f; lp++) {
        self.init_flag = 0;
        self.lfeature_remote = 0;
        b_called = false;
        b_initial_routing_sync = false;

        //odd bits(7, 5, 3, 1)
        //          abcd
        //      a0b0c0d0
        lf = (lp & 0x08) << 4 | (lp & 0x04) << 3 | (lp & 0x02) << 2 | (lp & 0x01) << 1;
        ret = recv_init(&self, NULL, 0);
        ASSERT_TRUE(ret);
        ASSERT_EQ(lf, self.lfeature_remote);
        ASSERT_EQ(M_INIT_FLAG_RECV, self.init_flag);
        ASSERT_TRUE(b_called);
        bool initsync = ((lp & 0x02) << 2) != 0;
        ASSERT_EQ(initsync, b_initial_routing_sync);
    }
}


TEST_F(ln, recv_init_lf2)
{
    ln_self_t self;
    LnInit(&self);

    static bool b_called;
    static bool b_initial_routing_sync;
    static uint8_t lf;
    class dummy {
    public:
        static bool ln_msg_init_read(ln_init_t *pMsg, const uint8_t *pData, uint16_t Len) {
            utl_buf_alloc(&pMsg->localfeatures, 1);
            pMsg->localfeatures.buf[0] = lf;
            return true;
        }
        static void callback(ln_self_t *self, ln_cb_t type, void *p_param) {
            (void)self;
            if (type == LN_CB_INIT_RECV) {
                b_called = true;
                b_initial_routing_sync = *(bool *)p_param;
            }
        }
    };
    self.p_callback = dummy::callback;
    ln_msg_init_read_fake.custom_fake = dummy::ln_msg_init_read;

    bool ret;
    
    for (int lp = 1; lp <= 0x0f; lp++) {
        self.init_flag = 0;
        self.lfeature_remote = 0;
        b_called = false;
        b_initial_routing_sync = false;

        //even bits(6, 4, 2, 0)
        //          abcd
        //      0a0b0c0d
        lf = (lp & 0x08) << 3 | (lp & 0x04) << 2 | (lp & 0x02) << 1 | (lp & 0x01);
        ret = recv_init(&self, NULL, 0);
        if (lf == 0x01) {
            //option_data_loss_protect
            ASSERT_TRUE(ret);
            ASSERT_EQ(lf, self.lfeature_remote);
            ASSERT_EQ(M_INIT_FLAG_RECV, self.init_flag);
            ASSERT_TRUE(b_called);
            ASSERT_FALSE(b_initial_routing_sync);
        } else {
            ASSERT_FALSE(ret);
            ASSERT_EQ(0x00, self.lfeature_remote);
            ASSERT_EQ(0, self.init_flag);
            ASSERT_FALSE(b_called);
            ASSERT_FALSE(b_initial_routing_sync);
        }
    }
}