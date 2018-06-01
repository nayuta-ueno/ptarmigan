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
/**
 * @file    misc.h
 * @brief   miscellaneous
 */
#ifndef MISC_H__
#define MISC_H__

#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>


#ifdef __cplusplus
extern "C" {
#endif  //__cplusplus

/**************************************************************************
 * macros
 **************************************************************************/

#ifdef APP_DEBUG_MEM
#define APP_MALLOC(a)       misc_dbg_malloc(a); DBG_PRINTF("APP_MALLOC:%d\n", misc_dbg_malloc_cnt());          ///< malloc(カウント付き)(APP_DEBUG_MEM定義時のみ有効)
//#define APP_REALLOC         misc_dbg_realloc        ///< realloc(カウント付き)(APP_DEBUG_MEM定義時のみ有効)
//#define APP_CALLOC          misc_dbg_calloc         ///< realloc(カウント付き)(APP_DEBUG_MEM定義時のみ有効)
#define APP_FREE(ptr)       { misc_dbg_free(ptr); ptr = NULL; DBG_PRINTF("APP_FREE:%d\n", misc_dbg_malloc_cnt());}        ///< free(カウント付き)(APP_DEBUG_MEM定義時のみ有効)
#else   //APP_DEBUG_MEM
#define APP_MALLOC          malloc
//#define APP_REALLOC         realloc
//#define APP_CALLOC          calloc
#define APP_FREE(ptr)       { free(ptr); ptr = NULL; }
#endif  //APP_DEBUG_MEM


/**************************************************************************
 * prototypes
 **************************************************************************/

/** sleep millisecond
 *
 * @param[in]   slp     スリープする時間[msec]
 */
static inline void misc_msleep(unsigned long slp) {
    struct timespec req = { 0, (long)(slp * 1000000UL) };
    nanosleep(&req, NULL);
}


/** 16進数文字列から変換
 *
 * @param[out]      pBin        変換結果
 * @param[out]      BinLen      pBin長
 * @param[out]      pStr        元データ
 */
bool misc_str2bin(uint8_t *pBin, uint32_t BinLen, const char *pStr);


/** 16進数文字列から変換(エンディアン反転)
 *
 * @param[out]      pBin        変換結果(エンディアン反転)
 * @param[out]      BinLen      pBin長
 * @param[out]      pStr        元データ
 */
bool misc_str2bin_rev(uint8_t *pBin, uint32_t BinLen, const char *pStr);


/** JSON-RPC送信
 *
 */
int misc_sendjson(const char *pSend, const char *pAddr, uint16_t Port);


/** 現在日時取得
 *
 * @param[out]      pDateTime       現在日時
 * @param[in]       Len             pDataTimeバッファサイズ
 */
void misc_datetime(char *pDateTime, size_t Len);


/** イベントファイル保存
 *
 * @param[in]       pChannelId          channel_id(ファイル名)(NULL時: "node")
 * @param[in]       pFormat             イベント文字列
 */
void misc_save_event(const uint8_t *pChannelId, const char *pFormat, ...);


/** 全データが0x00かのチェック
 *
 * @param[in]       pData               チェック対象
 * @param[in]       Len                 pData長
 * @retval  true    全データが0x00
 */
bool misc_all_zero(const void *pData, size_t Len);


#ifdef APP_DEBUG_MEM
void *misc_dbg_malloc(size_t size);
//void *misc_dbg_realloc(void *ptr, size_t size);
//void *misc_dbg_calloc(size_t blk, size_t size);
void misc_dbg_free(void *ptr);
int misc_dbg_malloc_cnt(void);
#endif  //APP_DEBUG_MEM

#ifdef __cplusplus
}
#endif  //__cplusplus

#endif /* MISC_H__ */
