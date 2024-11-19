/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#ifndef _GATTS_TABLE_H_
#define _GATTS_TABLE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Attributes State Machine */
enum
{
    IDX_SVC,
    IDX_CHAR_A,
    IDX_CHAR_VAL_A,
    IDX_CHAR_CFG_A,

    // IDX_CHAR_B,
    // IDX_CHAR_VAL_B,

    // IDX_CHAR_C,
    // IDX_CHAR_VAL_C,

    HRS_IDX_NB,
};

#endif /* _GATTS_TABLE_H_ */