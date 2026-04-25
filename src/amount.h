// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2019 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef HEMP0X_AMOUNT_H
#define HEMP0X_AMOUNT_H

#include <stdint.h>

/** Amount in corbies (Can be negative) */
typedef int64_t CAmount;

static const CAmount COIN = 100000000;
static const CAmount CENT = 1000000;

/** No amount larger than this (in satoshi) is valid.
 *
 * Note that this constant is *not* the total HEMP coin supply. It serves as a
 * sanity check ceiling for validation, particularly for asset issuance
 * quantities where individual assets may have up to 21B units.
 *
 * The HEMP coin supply is governed by the emission schedule in GetBlockSubsidy():
 * 10 HEMP initial subsidy, halving every 25,000,000 blocks via integer
 * right-shift, producing approximately 450,000,000 HEMP total across four eras
 * (10 + 5 + 2 + 1 = 18 HEMP per halving cycle x 25,000,000 blocks).
 *
 * As this is used by consensus-critical validation code, the exact value of
 * MAX_MONEY is consensus critical; modification could lead to a fork.
 * */
static const CAmount MAX_MONEY = 21000000000 * COIN;
inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

#endif //  HEMP0X_AMOUNT_H
