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
#include <stdarg.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "misc.h"
#include "ucoind.h"


/**************************************************************************
 * private variables
 **************************************************************************/

#ifdef APP_DEBUG_MEM
static int mcount = 0;
#endif  //APP_DEBUG_MEM


/**************************************************************************
 * public functions
 **************************************************************************/

bool misc_str2bin(uint8_t *pBin, uint32_t BinLen, const char *pStr)
{
    if (strlen(pStr) != BinLen * 2) {
        DBG_PRINTF("fail: invalid buffer size: %zu != %" PRIu32 " * 2\n", strlen(pStr), BinLen);
        return false;
    }

    bool ret = true;

    char str[3];
    str[2] = '\0';
    uint32_t lp;
    for (lp = 0; lp < BinLen; lp++) {
        str[0] = *(pStr + 2 * lp);
        str[1] = *(pStr + 2 * lp + 1);
        if (!str[0]) {
            //偶数文字で\0ならばOK
            break;
        }
        if (!str[1]) {
            //奇数文字で\0ならばNG
            DBG_PRINTF("fail: odd length\n");
            ret = false;
            break;
        }
        char *endp = NULL;
        uint8_t bin = (uint8_t)strtoul(str, &endp, 16);
        if ((endp != NULL) && (*endp != 0x00)) {
            //変換失敗
            DBG_PRINTF("fail: *endp = %p(%02x)\n", endp, *endp);
            ret = false;
            break;
        }
        pBin[lp] = bin;
    }

    return ret;
}


bool misc_str2bin_rev(uint8_t *pBin, uint32_t BinLen, const char *pStr)
{
    bool ret = misc_str2bin(pBin, BinLen, pStr);
    if (ret) {
        for (uint32_t lp = 0; lp < BinLen / 2; lp++) {
            uint8_t tmp = pBin[lp];
            pBin[lp] = pBin[BinLen - lp - 1];
            pBin[BinLen - lp - 1] = tmp;
        }
    }

    return ret;
}


void misc_datetime(char *pDateTime, size_t Len)
{
    struct tm tmval;
    time_t now = time(NULL);
    gmtime_r(&now, &tmval);
    strftime(pDateTime, Len, "%d %b %Y %T %z", &tmval);
}


void misc_save_event(const uint8_t *pChannelId, const char *pFormat, ...)
{
    char fname[256];

    if (pChannelId != NULL) {
        char chanid[LN_SZ_CHANNEL_ID * 2 + 1];
        ucoin_util_bin2str(chanid, pChannelId, LN_SZ_CHANNEL_ID);
        sprintf(fname, FNAME_EVENTCH_LOG, chanid);
    } else {
        sprintf(fname, FNAME_EVENTCH_LOG, "node");
    }
    FILE *fp = fopen(fname, "a");
    if (fp != NULL) {
        char date[50];
        misc_datetime(date, sizeof(date));
        fprintf(fp, "[%s]", date);

        va_list ap;
        va_start(ap, pFormat);
        vfprintf(fp, pFormat, ap);
        va_end(ap);

        fprintf(fp, "\n");
        fclose(fp);
    }
}


bool misc_all_zero(const void *pData, size_t Len)
{
    bool ret = true;
    const uint8_t *p = (const uint8_t *)pData;
    for (size_t lp = 0; lp < Len; lp++) {
        if (p[lp] != 0x00) {
            ret = false;
            break;
        }
    }
    return ret;
}


/**************************************************************************
 * debug functions
 **************************************************************************/

#ifdef APP_DEBUG_MEM

void *misc_dbg_malloc(size_t size)
{
    void *p = malloc(size);
    if (p) {
        mcount++;
    }
    return p;
}


//void *misc_dbg_realloc(void *ptr, size_t size)
//{
//    void *p = realloc(ptr, size);
//    if ((ptr == NULL) && p) {
//        mcount++;
//    }
//    return p;
//}


//void *misc_dbg_calloc(size_t blk, size_t size)
//{
//    void *p = calloc(blk, size);
//    if (p) {
//        mcount++;
//    }
//    return p;
//}


void misc_dbg_free(void *ptr)
{
    //NULL代入してfree()だけするパターンもあるため、NULLチェックする
    if (ptr) {
        mcount--;
    }
    free(ptr);
}


int misc_dbg_malloc_cnt(void)
{
    return mcount;
}

#endif
