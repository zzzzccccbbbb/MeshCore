/*!
 * @file      smtc_shield_lr2021mb1ldzcs.c
 *
 * @brief     LR2021 mbed shield @868/915MHz for EU/US LoRa subGHz and @2.4GHz Frequency Bands – Switchless version for
 * each RF path
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2025. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include <stddef.h>
#include <stdint.h>
#include "lr20xx_system_types.h"
#include "lr20xx_radio_common_types.h"
#include "smtc_shield_lr20xx_types.h"
#include "smtc_shield_lr20xx_common.h"
#include "smtc_shield_lr20xx_interface.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

#define SMTC_SHIELD_LR2021_SUPPORTED_DIO_NUM ( 7 )

/* W20_LORA_HAS_TCXO：1=有源 TCXO（默认），0=无源 XTAL。由 platformio.ini -D 覆盖。 */
#ifndef W20_LORA_HAS_TCXO
#define W20_LORA_HAS_TCXO 1
#endif

#if W20_LORA_HAS_TCXO
const smtc_shield_lr20xx_xosc_cfg_t smtc_shield_lr2021_xosc_cfg = {
    /* 有源 TCXO：板级 VTCXO 悬空，由芯片 VTCXO 脚供电。
     * tick 单位=1/32MHz；300 过短会误报 HF_XOSC_START，用 ~5ms。 */
    .has_tcxo             = true,
    .supply               = LR20XX_SYSTEM_TCXO_CTRL_3_0V,
    .startup_time_in_tick = 160000,
};
#else
const smtc_shield_lr20xx_xosc_cfg_t smtc_shield_lr2021_xosc_cfg = {
    /* 无源晶振：不调用 set_tcxo_mode，芯片走内部 XOSC。 */
    .has_tcxo             = false,
    .supply               = LR20XX_SYSTEM_TCXO_CTRL_3_0V,
    .startup_time_in_tick = 300,
};
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

typedef struct smtc_shield_lr20xx_dio_cfg_s
{
    lr20xx_system_dio_func_t          dio_function;
    lr20xx_system_dio_drive_t         dio_drive_sleep;
    lr20xx_system_dio_rf_switch_cfg_t rf_switch_cfg;
    lr20xx_system_irq_mask_t          irq_mask;
} smtc_shield_lr20xx_dio_cfg_t;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

const smtc_shield_lr20xx_pa_pwr_cfg_t
    smtc_shield_lr2021_pa_lf_pwr_cfg[ SMTC_SHIELD_LR20XX_MAX_PWR - SMTC_SHIELD_LR20XX_MIN_PWR + 1 ] = {
        { // Expected output power = -10dBm
          .power     = 1,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 0,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = -9dBm
          .power     = 3,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 0,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = -8dBm
          .power     = 5,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 0,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = -7dBm
          .power     = 7,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 0,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = -6dBm
          .power     = 2,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 3,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = -5dBm
          .power     = 6,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 3,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = -4dBm
          .power     = 8,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 1,
                         .pa_lf_slices     = 1,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = -3dBm
          .power     = 13,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 1,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = -2dBm
          .power     = 13,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 0,
                         .pa_lf_slices     = 2,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = -1dBm
          .power     = 13,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 3,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 0dBm
          .power     = 20,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 0,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 1dBm
          .power     = 22,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 0,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 2dBm
          .power     = 21,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 1,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 3dBm
          .power     = 21,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 2,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 4dBm
          .power     = 26,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 1,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 5dBm
          .power     = 26,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 2,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 6dBm
          .power     = 28,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 2,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 7dBm
          .power     = 31,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 2,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 8dBm
          .power     = 30,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 4,
                         .pa_lf_slices     = 0,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 9dBm
          .power     = 32,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 2,
                         .pa_lf_slices     = 1,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 10dBm
          .power     = 28,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 3,
                         .pa_lf_slices     = 2,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 11dBm
          .power     = 30,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 3,
                         .pa_lf_slices     = 2,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 12dBm
          .power     = 31,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 3,
                         .pa_lf_slices     = 3,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 13dBm
          .power     = 31,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 4,
                         .pa_lf_slices     = 3,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 14dBm
          .power     = 30,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 3,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 15dBm
          .power     = 31,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 7,
                         .pa_lf_slices     = 3,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 16dBm
          .power     = 33,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 7,
                         .pa_lf_slices     = 3,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 17dBm
          .power     = 36,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 5,
                         .pa_lf_slices     = 5,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 18dBm
          .power     = 36,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 6,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 19dBm
          .power     = 38,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 6,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 20dBm
          .power     = 39,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 7,
                         .pa_lf_slices     = 6,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 21dBm
          .power     = 41,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 7,
                         .pa_lf_slices     = 6,
                         .pa_hf_duty_cycle = 16 } },
        { // Expected output power = 22dBm
          .power     = 43,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 7,
                         .pa_lf_slices     = 6,
                         .pa_hf_duty_cycle = 16 } },
};

const smtc_shield_lr20xx_pa_pwr_cfg_t smtc_shield_lr2021_pa_hf_default_pwr_cfg = {
    .power     = 0,
    .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                  .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                  .pa_lf_duty_cycle = 7,
                  .pa_lf_slices     = 6,
                  .pa_hf_duty_cycle = 16 }
};

const smtc_shield_lr20xx_pa_pwr_cfg_t smtc_shield_lr2021_pa_lf_default_pwr_cfg = {
    .power     = 0,
    .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                  .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                  .pa_lf_duty_cycle = 7,
                  .pa_lf_slices     = 6,
                  .pa_hf_duty_cycle = 16 }
};

const smtc_shield_lr20xx_pa_pwr_cfg_t
    smtc_shield_lr2021_pa_hf_pwr_cfg[ SMTC_SHIELD_LR20XX_MAX_PWR_HF - SMTC_SHIELD_LR20XX_MIN_PWR_HF + 1 ] = {
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
        { .power     = 0,
         .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                         .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                         .pa_lf_duty_cycle = 6,
                         .pa_lf_slices     = 7,
                         .pa_hf_duty_cycle = 6 } },
};

const smtc_shield_lr20xx_dio_cfg_t transceiver_dio_cfg[ SMTC_SHIELD_LR2021_SUPPORTED_DIO_NUM ] = {
    { // LR20XX_SYSTEM_DIO_5  // 2.4GHz TXEN: TX_HF 高
      .dio_function    = LR20XX_SYSTEM_DIO_FUNC_RF_SWITCH,
      .dio_drive_sleep = LR20XX_SYSTEM_DIO_DRIVE_PULL_UP,
      .rf_switch_cfg   = LR20XX_SYSTEM_DIO_RF_SWITCH_WHEN_TX_HF,
      .irq_mask        = LR20XX_SYSTEM_IRQ_NONE     },
    { // LR20XX_SYSTEM_DIO_6  // 2.4GHz RXEN: RX_HF 高
      .dio_function    = LR20XX_SYSTEM_DIO_FUNC_RF_SWITCH,
      .dio_drive_sleep = LR20XX_SYSTEM_DIO_DRIVE_PULL_UP,
      .rf_switch_cfg   = LR20XX_SYSTEM_DIO_RF_SWITCH_WHEN_RX_HF,
      .irq_mask        = LR20XX_SYSTEM_IRQ_NONE     },
    { // LR20XX_SYSTEM_DIO_7  // W20 未引出到 MCU，保持 NONE
      .dio_function    = LR20XX_SYSTEM_DIO_FUNC_NONE,
      .dio_drive_sleep = LR20XX_SYSTEM_DIO_DRIVE_NONE,
      .rf_switch_cfg   = 0,
      .irq_mask        = LR20XX_SYSTEM_IRQ_NONE     },
    { // LR20XX_SYSTEM_DIO_8  // W20 未引出到 MCU，保持 NONE
      .dio_function    = LR20XX_SYSTEM_DIO_FUNC_NONE,
      .dio_drive_sleep = LR20XX_SYSTEM_DIO_DRIVE_NONE,
      .rf_switch_cfg   = 0,
      .irq_mask        = LR20XX_SYSTEM_IRQ_NONE     },
    { // LR20XX_SYSTEM_DIO_9  // 唯一 IRQ：模块 DIO9 → MCU GPIO5
      .dio_function    = LR20XX_SYSTEM_DIO_FUNC_IRQ,
      .dio_drive_sleep = LR20XX_SYSTEM_DIO_DRIVE_NONE,
      .rf_switch_cfg   = 0,
      .irq_mask        = LR20XX_SYSTEM_IRQ_ALL_MASK },
    { // LR20XX_SYSTEM_DIO_10  // SubGHz CPS: TX_LF 高 (高功率)
      .dio_function    = LR20XX_SYSTEM_DIO_FUNC_RF_SWITCH,
      .dio_drive_sleep = LR20XX_SYSTEM_DIO_DRIVE_NONE,
      .rf_switch_cfg   = LR20XX_SYSTEM_DIO_RF_SWITCH_WHEN_TX_LF,
      .irq_mask        = LR20XX_SYSTEM_IRQ_NONE     },
    { // LR20XX_SYSTEM_DIO_11  // SubGHz CSD: TX_LF 或 RX_LF 高
      .dio_function    = LR20XX_SYSTEM_DIO_FUNC_RF_SWITCH,
      .dio_drive_sleep = LR20XX_SYSTEM_DIO_DRIVE_NONE,
      .rf_switch_cfg   = (LR20XX_SYSTEM_DIO_RF_SWITCH_WHEN_TX_LF | LR20XX_SYSTEM_DIO_RF_SWITCH_WHEN_RX_LF),
      .irq_mask        = LR20XX_SYSTEM_IRQ_NONE     }
};

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC VARIABLES --------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

const smtc_shield_lr20xx_pa_pwr_cfg_t* smtc_shield_lr20xx_get_pa_pwr_cfg( const uint32_t rf_freq_in_hz,
                                                                          int8_t         expected_output_pwr_in_dbm )
{
    if( rf_freq_in_hz < SMTC_SHIELD_LR20XX_LF_HF_FREQ_THRESHOLD )
    {
        // SubGHz frequencies
        if( ( SMTC_SHIELD_LR20XX_SUBGHZ_FREQ_MIN <= rf_freq_in_hz ) &&
            ( rf_freq_in_hz <= SMTC_SHIELD_LR20XX_SUBGHZ_FREQ_MAX ) )
        {
            return smtc_shield_lr20xx_common_get_default_pa_lf_pwr_cfg();
        }
    }
    else
    {
        if( ( ( SMTC_SHIELD_LR20XX_2GHZ_FREQ_MIN <= rf_freq_in_hz ) &&
              ( rf_freq_in_hz <= SMTC_SHIELD_LR20XX_2GHZ_FREQ_MAX ) ) ||
            ( ( SMTC_SHIELD_LR20XX_2_4GHZ_FREQ_MIN <= rf_freq_in_hz ) &&
              ( rf_freq_in_hz <= SMTC_SHIELD_LR20XX_2_4GHZ_FREQ_MAX ) ) )

        {
            return smtc_shield_lr20xx_common_get_default_pa_hf_pwr_cfg();
        }
    }
    return NULL;
}

const lr20xx_system_dio_rf_switch_cfg_t* smtc_shield_lr20xx_get_rf_switch_cfg( lr20xx_system_dio_t dio )
{
    uint8_t dio_num = (uint8_t) dio - (uint8_t) LR20XX_SYSTEM_DIO_5;

    if( dio_num < SMTC_SHIELD_LR2021_SUPPORTED_DIO_NUM )
    {
        smtc_shield_lr20xx_dio_cfg_t dio_cfg = transceiver_dio_cfg[ dio_num ];
        if( dio_cfg.dio_function == LR20XX_SYSTEM_DIO_FUNC_RF_SWITCH )
        {
            return &transceiver_dio_cfg[ dio_num ].rf_switch_cfg;
        }
    }

    return NULL;
}

const lr20xx_system_dio_drive_t* smtc_shield_lr20xx_get_dio_sleep_drive( lr20xx_system_dio_t dio )
{
    uint8_t dio_num = (uint8_t) dio - (uint8_t) LR20XX_SYSTEM_DIO_5;

    if( dio_num < SMTC_SHIELD_LR2021_SUPPORTED_DIO_NUM )
    {
        return &transceiver_dio_cfg[ dio_num ].dio_drive_sleep;
    }

    return NULL;
}

const lr20xx_system_dio_func_t* smtc_shield_lr20xx_get_dio_function( lr20xx_system_dio_t dio )
{
    uint8_t dio_num = (uint8_t) dio - (uint8_t) LR20XX_SYSTEM_DIO_5;

    if( dio_num < SMTC_SHIELD_LR2021_SUPPORTED_DIO_NUM )
    {
        return &transceiver_dio_cfg[ dio_num ].dio_function;
    }

    return NULL;
}

const lr20xx_system_irq_mask_t* smtc_shield_lr20xx_get_dio_irq_mask( lr20xx_system_dio_t dio )
{
    uint8_t dio_num = (uint8_t) dio - (uint8_t) LR20XX_SYSTEM_DIO_5;

    if( dio_num < SMTC_SHIELD_LR2021_SUPPORTED_DIO_NUM )
    {
        smtc_shield_lr20xx_dio_cfg_t dio_cfg = transceiver_dio_cfg[ dio_num ];
        if( dio_cfg.dio_function == LR20XX_SYSTEM_DIO_FUNC_IRQ )
        {
            return &transceiver_dio_cfg[ dio_num ].irq_mask;
        }
    }

    return NULL;
}

const lr20xx_system_reg_mode_t* smtc_shield_lr20xx_get_reg_mode( void )
{
    return smtc_shield_lr20xx_common_get_reg_mode();
}

const smtc_shield_lr20xx_xosc_cfg_t* smtc_shield_lr20xx_get_xosc_cfg( void )
{
#if W20_LORA_HAS_TCXO
    return &smtc_shield_lr2021_xosc_cfg;
#else
    /* 无源：与 demo 一样回落 common（has_tcxo=false, startup=96000） */
    return smtc_shield_lr20xx_common_get_xosc_cfg();
#endif
}

const smtc_shield_lr20xx_pinout_t* smtc_shield_lr20xx_get_pinout( void )
{
    return smtc_shield_lr20xx_common_get_pinout();
}

const lr20xx_system_lfclk_cfg_t* smtc_shield_lr20xx_get_lfclk_cfg( void )
{
    return smtc_shield_lr20xx_common_get_lfclk_cfg();
}

const lr20xx_radio_common_rssi_calibration_gain_table_t* smtc_shield_lr20xx_get_rssi_calibration_table(
    const uint32_t rf_freq_in_hz )
{
    return smtc_shield_lr20xx_common_get_rssi_calibration_table( rf_freq_in_hz );
}

bool smtc_shield_lr20xx_rttof_recommended_rx_tx_delay_indicator( uint32_t               rf_freq_in_hz,
                                                                 lr20xx_radio_lora_bw_t bw,
                                                                 lr20xx_radio_lora_sf_t sf,
                                                                 uint32_t*              delay_indicator )
{
    return smtc_shield_lr20xx_common_rttof_recommended_rx_tx_delay_indicator( rf_freq_in_hz, bw, sf, delay_indicator );
}

const lr20xx_system_dio_t* smtc_shield_lr20xx_get_irq_dio( void )
{
    return smtc_shield_lr20xx_common_get_irq_dio();
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

/* --- EOF ------------------------------------------------------------------ */
