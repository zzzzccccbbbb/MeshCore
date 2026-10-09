/*!
 * @file      smtc_shield_lr20xx_common.c
 *
 * @brief     Implementation common to LR20xx shields
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
#include <stdbool.h>
#include <stdint.h>
#include "smtc_shield_pinout.h"
#include "smtc_shield_lr20xx_types.h"
#include "smtc_shield_lr20xx_common.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

const smtc_shield_lr20xx_pa_pwr_cfg_t smtc_shield_lr20xx_common_pa_lf_default_pwr_cfg = {
    .power     = 0,
    .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF,
                  .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                  .pa_lf_duty_cycle = 7,
                  .pa_lf_slices     = 6,
                  .pa_hf_duty_cycle = 16 }
};

const smtc_shield_lr20xx_pa_pwr_cfg_t smtc_shield_lr20xx_common_pa_hf_default_pwr_cfg = {
    .power     = 0,
    .pa_config = { .pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF,
                  .pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
                  .pa_lf_duty_cycle = 7,
                  .pa_lf_slices     = 6,
                  .pa_hf_duty_cycle = 16 }
};

const lr20xx_system_dio_t smtc_shield_lr20xx_common_irq_dio = LR20XX_SYSTEM_DIO_9;

static const lr20xx_radio_common_rssi_calibration_gain_table_t
    smtc_shield_lr20xx_common_rssi_calibration_table_below_600mhz = {

        .g1         = { .gain_value = 12, .noise_figure = 2 },
        .g2         = { .gain_value = 12, .noise_figure = 2 },
        .g3         = { .gain_value = 14, .noise_figure = 2 },
        .g4         = { .gain_value = 0,  .noise_figure = 2 },
        .g5         = { .gain_value = 1,  .noise_figure = 2 },
        .g6         = { .gain_value = 3,  .noise_figure = 2 },
        .g7         = { .gain_value = 4,  .noise_figure = 2 },
        .g8         = { .gain_value = 4,  .noise_figure = 2 },
        .g9         = { .gain_value = 3,  .noise_figure = 2 },
        .g10        = { .gain_value = 6,  .noise_figure = 2 },
        .g11        = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost0 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost1 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost2 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost3 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost4 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost5 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost6 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost7 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost0 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost1 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost2 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost3 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost4 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost5 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost6 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost7 = { .gain_value = 6,  .noise_figure = 2 },
};

static const lr20xx_radio_common_rssi_calibration_gain_table_t
    smtc_shield_lr20xx_common_rssi_calibration_table_from_600mhz_to_2ghz = {

        .g1         = { .gain_value = 12, .noise_figure = 2 },
        .g2         = { .gain_value = 12, .noise_figure = 2 },
        .g3         = { .gain_value = 14, .noise_figure = 2 },
        .g4         = { .gain_value = 0,  .noise_figure = 2 },
        .g5         = { .gain_value = 1,  .noise_figure = 2 },
        .g6         = { .gain_value = 3,  .noise_figure = 2 },
        .g7         = { .gain_value = 4,  .noise_figure = 2 },
        .g8         = { .gain_value = 4,  .noise_figure = 2 },
        .g9         = { .gain_value = 3,  .noise_figure = 2 },
        .g10        = { .gain_value = 6,  .noise_figure = 2 },
        .g11        = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost0 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost1 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost2 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost3 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost4 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost5 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost6 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost7 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost0 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost1 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost2 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost3 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost4 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost5 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost6 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost7 = { .gain_value = 6,  .noise_figure = 2 },
};

static const lr20xx_radio_common_rssi_calibration_gain_table_t
    smtc_shield_lr20xx_common_rssi_calibration_table_above_2ghz = {
        .g1         = { .gain_value = 12, .noise_figure = 2 },
        .g2         = { .gain_value = 12, .noise_figure = 2 },
        .g3         = { .gain_value = 14, .noise_figure = 2 },
        .g4         = { .gain_value = 0,  .noise_figure = 2 },
        .g5         = { .gain_value = 1,  .noise_figure = 2 },
        .g6         = { .gain_value = 3,  .noise_figure = 2 },
        .g7         = { .gain_value = 4,  .noise_figure = 2 },
        .g8         = { .gain_value = 4,  .noise_figure = 2 },
        .g9         = { .gain_value = 3,  .noise_figure = 2 },
        .g10        = { .gain_value = 6,  .noise_figure = 2 },
        .g11        = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost0 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost1 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost2 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost3 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost4 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost5 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost6 = { .gain_value = 6,  .noise_figure = 2 },
        .g12_boost7 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost0 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost1 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost2 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost3 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost4 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost5 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost6 = { .gain_value = 6,  .noise_figure = 2 },
        .g13_boost7 = { .gain_value = 6,  .noise_figure = 2 },
};

const lr20xx_system_dio_rf_switch_cfg_t smtc_shield_lr20xx_common_rf_switch_cfg =
    ( LR20XX_SYSTEM_DIO_RF_SWITCH_WHEN_RX_LF | LR20XX_SYSTEM_DIO_RF_SWITCH_WHEN_TX_LF );

const smtc_shield_lr20xx_pinout_t smtc_shield_lr20xx_common_pinout = {
    .busy     = SMTC_SHIELD_PINOUT_D3,
    .irq      = SMTC_SHIELD_PINOUT_D5,
    .nss      = SMTC_SHIELD_PINOUT_D7,
    .reset    = SMTC_SHIELD_PINOUT_A0,
    .lna      = SMTC_SHIELD_PINOUT_NONE,
    .led_tx   = SMTC_SHIELD_PINOUT_NONE,
    .led_rx   = SMTC_SHIELD_PINOUT_NONE,
    .led_scan = SMTC_SHIELD_PINOUT_NONE,
};

static const lr20xx_system_reg_mode_t      smtc_shield_lr20xx_common_reg_mode         = LR20XX_SYSTEM_REG_MODE_DCDC;
static const lr20xx_system_lfclk_cfg_t     smtc_shield_lr20xx_common_lfclk_cfg        = LR20XX_SYSTEM_LFCLK_RC;

static const smtc_shield_lr20xx_xosc_cfg_t smtc_shield_lr20xx_common_xosc_no_tcxo_cfg = {
    .has_tcxo             = false, /* 无源晶振默认；有源板用盾牌自己的 get_xosc_cfg */
    .supply               = LR20XX_SYSTEM_TCXO_CTRL_3_0V,
    .startup_time_in_tick = 96000,
};

/*!
 * @brief Recommanded for frequency from 430MHz to 510MHz
 */
static const uint32_t rttof_delay_indicator_table_below_600mhz[ 7 ][ 8 ] = {
    /* SF5,  SF6,   SF7,   SF8,   SF9,   SF10,  SF11,  SF12 */
    { 19107, 19122, 19113, 19114, 19113, 19114, 19036, 19024 }, // BW125
    { 19107, 19122, 19113, 19114, 19113, 19114, 19036, 19024 }, // BW203
    { 20265, 20279, 20278, 20273, 20270, 20272, 20236, 20232 }, // BW250
    { 20166, 20249, 20288, 20309, 20318, 20312, 20295, 20298 }, // BW406
    { 20166, 20249, 20288, 20309, 20318, 20312, 20295, 20298 }, // BW500
    { 20166, 20249, 20288, 20309, 20318, 20312, 20295, 20298 }, // BW812
    { 20166, 20249, 20288, 21719, 20318, 20312, 20295, 20298 }, // BW1000
};

/*!
 * @brief Recommanded for frequency from 860MHz to 928MHz
 */
static const uint32_t rttof_delay_indicator_table_from_600mhz_to_2ghz[ 7 ][ 8 ] = {
    /* SF5,  SF6,   SF7,   SF8,   SF9,   SF10,  SF11,  SF12 */
    { 19115, 19113, 19121, 19127, 19141, 19178, 19036, 19024 }, // BW125
    { 19115, 19113, 19121, 19127, 19141, 19178, 19036, 19024 }, // BW203
    { 20265, 20266, 20279, 20292, 20236, 20305, 20236, 20232 }, // BW250
    { 20154, 20268, 20298, 20319, 20323, 20314, 20295, 20298 }, // BW406
    { 20154, 20268, 20298, 20319, 20323, 20314, 20295, 20298 }, // BW500
    { 20154, 20268, 20298, 20319, 20323, 20314, 20295, 20298 }, // BW812
    { 20154, 20268, 20298, 21719, 20323, 20314, 20295, 20298 }, // BW1000
};

/*!
 * @brief Recommanded for 2.4G frequency
 */
static const uint32_t rttof_delay_indicator_table_above_2ghz[ 7 ][ 8 ] = {
    /* SF5,  SF6,   SF7,   SF8,   SF9,   SF10,  SF11,  SF12 */
    { 19118, 19123, 19120, 19124, 19121, 19119, 19036, 19024 }, // BW125
    { 19118, 19123, 19120, 19124, 19121, 19119, 19036, 19024 }, // BW203
    { 20221, 20230, 20226, 20231, 20236, 20223, 20236, 20232 }, // BW250
    { 20143, 20230, 20252, 20284, 20305, 20288, 20295, 20298 }, // BW406
    { 20143, 20230, 20252, 20284, 20305, 20288, 20295, 20298 }, // BW500
    { 20143, 20230, 20252, 20284, 20305, 20288, 20295, 20298 }, // BW812
    { 20143, 20230, 20252, 21719, 20305, 20288, 20295, 20298 }, // BW1000
};

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

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

const smtc_shield_lr20xx_pa_pwr_cfg_t* smtc_shield_lr20xx_common_get_default_pa_lf_pwr_cfg( void )
{
    return &smtc_shield_lr20xx_common_pa_lf_default_pwr_cfg;
}

const smtc_shield_lr20xx_pa_pwr_cfg_t* smtc_shield_lr20xx_common_get_default_pa_hf_pwr_cfg( void )
{
    return &smtc_shield_lr20xx_common_pa_hf_default_pwr_cfg;
}

const lr20xx_radio_common_rssi_calibration_gain_table_t* smtc_shield_lr20xx_common_get_rssi_calibration_table(
    const uint32_t rf_freq_in_hz )
{
    if( rf_freq_in_hz < 600000000 )
    {
        return &smtc_shield_lr20xx_common_rssi_calibration_table_below_600mhz;
    }
    else if( ( 600000000 <= rf_freq_in_hz ) && ( rf_freq_in_hz <= 2000000000 ) )
    {
        return &smtc_shield_lr20xx_common_rssi_calibration_table_from_600mhz_to_2ghz;
    }
    else
    {
        return &smtc_shield_lr20xx_common_rssi_calibration_table_above_2ghz;
    }
}

const smtc_shield_lr20xx_pinout_t* smtc_shield_lr20xx_common_get_pinout( void )
{
    return &smtc_shield_lr20xx_common_pinout;
}

const lr20xx_system_dio_t* smtc_shield_lr20xx_common_get_irq_dio( void )
{
    return &smtc_shield_lr20xx_common_irq_dio;
}

const lr20xx_system_reg_mode_t* smtc_shield_lr20xx_common_get_reg_mode( void )
{
    return &smtc_shield_lr20xx_common_reg_mode;
}

const lr20xx_system_lfclk_cfg_t* smtc_shield_lr20xx_common_get_lfclk_cfg( void )
{
    return &smtc_shield_lr20xx_common_lfclk_cfg;
}

const smtc_shield_lr20xx_xosc_cfg_t* smtc_shield_lr20xx_common_get_xosc_cfg( void )
{
    return &smtc_shield_lr20xx_common_xosc_no_tcxo_cfg;
}

bool smtc_shield_lr20xx_common_rttof_recommended_rx_tx_delay_indicator( uint32_t               rf_freq_in_hz,
                                                                        lr20xx_radio_lora_bw_t bw,
                                                                        lr20xx_radio_lora_sf_t sf,
                                                                        uint32_t*              delay_indicator )
{
    uint8_t row_index;
    uint8_t column_index;

    *delay_indicator = 0u;

    switch( bw )
    {
        case LR20XX_RADIO_LORA_BW_125:
            row_index = 0;
            break;
        case LR20XX_RADIO_LORA_BW_203:
            row_index = 1;
            break;
        case LR20XX_RADIO_LORA_BW_250:
            row_index = 2;
            break;
        case LR20XX_RADIO_LORA_BW_406:
            row_index = 3;
            break;
        case LR20XX_RADIO_LORA_BW_500:
            row_index = 4;
            break;
        case LR20XX_RADIO_LORA_BW_812:
            row_index = 5;
            break;
        case LR20XX_RADIO_LORA_BW_1000:
            row_index = 6;
            break;
        default:
            return false;
    }

    switch( sf )
    {
        case LR20XX_RADIO_LORA_SF5:
            column_index = 0;
            break;
        case LR20XX_RADIO_LORA_SF6:
            column_index = 1;
            break;
        case LR20XX_RADIO_LORA_SF7:
            column_index = 2;
            break;
        case LR20XX_RADIO_LORA_SF8:
            column_index = 3;
            break;
        case LR20XX_RADIO_LORA_SF9:
            column_index = 4;
            break;
        case LR20XX_RADIO_LORA_SF10:
            column_index = 5;
            break;
        case LR20XX_RADIO_LORA_SF11:
            column_index = 6;
            break;
        case LR20XX_RADIO_LORA_SF12:
            column_index = 7;
            break;
        default:
            return false;
    }

    if( rf_freq_in_hz < 600000000 )
    {
        *delay_indicator = rttof_delay_indicator_table_below_600mhz[ row_index ][ column_index ];
    }
    else if( ( 600000000 <= rf_freq_in_hz ) && ( rf_freq_in_hz < 2000000000 ) )
    {
        *delay_indicator = rttof_delay_indicator_table_from_600mhz_to_2ghz[ row_index ][ column_index ];
    }
    else
    {
        *delay_indicator = rttof_delay_indicator_table_above_2ghz[ row_index ][ column_index ];
    }

    return true;
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

/* --- EOF ------------------------------------------------------------------ */
