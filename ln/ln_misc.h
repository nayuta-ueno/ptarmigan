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
/** @file   ln_misc.h
 *  @brief  [LN]雑多
 */
#ifndef LN_MISC_H__
#define LN_MISC_H__

#include "ln.h"


/**************************************************************************
 * prototypes
 **************************************************************************/

/** DER形式秘密鍵を64byte展開
 *
 * @param[out]      pSig        展開先(64byte)
 * @param[in]       pBuf        DER形式秘密鍵
 * @retval      true    成功
 * @note
 *      - SIGHASH_ALLのチェックは行わない
 */
bool HIDDEN ln_misc_sigtrim(uint8_t *pSig, const uint8_t *pBuf);


/** 64bit形式秘密鍵をDER形式展開
 *
 * @param[out]      pSig        展開先(DER形式秘密鍵)
 * @param[in]       pBuf        64byte形式秘密鍵
 * @note
 *      - SIGHASH_ALLを付加する
 */
void HIDDEN ln_misc_sigexpand(utl_buf_t *pSig, const uint8_t *pBuf);


#endif /* LN_MISC_H__ */
