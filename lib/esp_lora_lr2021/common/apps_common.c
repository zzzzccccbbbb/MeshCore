/*!
 * @file      apps_common.c
 *
 * @brief     Common functions shared by the examples
 *
 * @copyright
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

// #include "main.h"

// #include "common_version.h"
#include "apps_common.h"
#include "apps_utilities.h"
// #include "smtc_hal_dbg_trace.h"
// #include "smtc_shield_pinout_mapping.h"
#include "smtc_shield_lr20xx_interface.h"

#include "lr20xx_driver_version.h"
#include "lr20xx_radio_lora.h"
#include "lr20xx_radio_fsk.h"
#include "lr20xx_radio_fifo.h"
#include "lr20xx_radio_flrc.h"
#include "lr20xx_radio_bpsk.h"
#include "lr20xx_radio_common.h"
#include "lr20xx_radio_ook.h"
#include "lr20xx_regmem.h"
#include "lr20xx_rttof.h"
#include "lr20xx_workarounds.h"

/*
 * Printers
 */
#include "lr20xx_radio_common_types_str.h"
#include "lr20xx_radio_lora_types_str.h"
#include "lr20xx_radio_fsk_types_str.h"
#include "lr20xx_radio_flrc_types_str.h"
#include "lr20xx_radio_bpsk_types_str.h"
#include "lr20xx_radio_lr_fhss_types_str.h"
#include "lr20xx_radio_ook_types_str.h"
#include "lr20xx_system.h"
#include "lr20xx_system_types.h"

#include "lr20xx_radio_common_types.h"  // 确保 include 这个头

#include "driver/gpio.h"
#include "esp_log.h"
static const char* TAG = "APP_COMMON";

#define HAL_DBG_TRACE_INFO(...)    ESP_LOGI(TAG, __VA_ARGS__)
#define HAL_DBG_TRACE_ERROR(...)   ESP_LOGE(TAG, __VA_ARGS__)
#define HAL_DBG_TRACE_WARNING(...) ESP_LOGW(TAG, __VA_ARGS__)
#define HAL_DBG_TRACE_PRINTF(...)  ESP_LOGI(TAG, __VA_ARGS__)  // 或用 vprintf 如果需要
#define HAL_DBG_TRACE_ARRAY(tag, buf, len) ESP_LOG_BUFFER_HEX(TAG, buf, len)

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */
static lr20xx_radio_common_pa_cfg_t esp_get_pa_cfg(uint32_t freq) {
    lr20xx_radio_common_pa_cfg_t pa_cfg = {0};

    if (freq > 2400000000) {  // HF (2.4GHz)
        pa_cfg.pa_sel = LR20XX_RADIO_COMMON_PA_SEL_HF;
        pa_cfg.pa_reg_supply = LR20XX_RADIO_COMMON_PA_REG_SUPPLY_VREG;  // 使用正确枚举，如果未定义，用 1 (VREG)
        pa_cfg.pa_hp_sel = 0x07;  // HF 专用参数
    } else {  // LF (e.g., 868/915MHz)
        pa_cfg.pa_sel = LR20XX_RADIO_COMMON_PA_SEL_LF;
        pa_cfg.pa_reg_supply = LR20XX_RADIO_COMMON_PA_REG_SUPPLY_VBAT;  // 使用正确枚举，如果未定义，用 0 (VBAT)
        pa_cfg.pa_lf_duty_cycle = 0x04;  // 修正为 LF 参数
        pa_cfg.pa_lf_bw = 0x00;  // 默认带宽参数，如果需要
    }
    return pa_cfg;
}
/**
 * @brief Number of symbols sent in the RTToF response - range within [0:63]
 */
#define RESPONSE_SYMBOLS_COUNT UINT8_C( 15 )

#define DBM_STRING_LEN         ( 15 )

/* W20：MCU GPIO5 只接模块 DIO9；DIO7/DIO8 未引出，勿作 IRQ */
static const lr20xx_system_dio_t esp_irq_dio = LR20XX_SYSTEM_DIO_9;

const lr20xx_system_dio_t* esp_lora_get_irq_dio( void )
{
    ESP_LOGD(TAG, "Returning IRQ DIO: DIO%d", (int)esp_irq_dio);
    return &esp_irq_dio;
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

static lr20xx_radio_lora_mod_params_t       lora_mod_params = { .sf  = LORA_SPREADING_FACTOR,
                                                                .bw  = LORA_BANDWIDTH,
                                                                .cr  = LORA_CODING_RATE,
                                                                .ppm = LR20XX_RADIO_LORA_NO_PPM };

static const lr20xx_radio_lora_pkt_params_t lora_pkt_params = {
    .preamble_len_in_symb = LORA_PREAMBLE_LENGTH,
    .pkt_mode             = LORA_PKT_LEN_MODE,
    .pld_len_in_bytes     = (uint8_t) PAYLOAD_LENGTH,
    .crc                  = LORA_CRC,
    .iq                   = LORA_IQ,
};

static lr20xx_radio_fsk_mod_params_t fsk_mod_params = {
    .br          = FSK_BITRATE,
    .pulse_shape = FSK_PULSE_SHAPE,
    .bw          = FSK_BANDWIDTH,
    .fdev_in_hz  = FSK_FDEV,
};

static lr20xx_radio_fsk_pkt_params_t fsk_pkt_params = {
    .pbl_length_in_bit = FSK_PREAMBLE_LENGTH,
    .preamble_detector = FSK_PREAMBLE_DETECTOR,
    .address_filtering = FSK_ADDRESS_FILTERING,
    .header_mode       = FSK_HEADER_TYPE,
    .payload_length    = PAYLOAD_LENGTH,
    .crc               = FSK_CRC_TYPE,
    .whitening         = FSK_DC_FREE,
};

static const lr20xx_radio_flrc_mod_params_t flrc_mod_params = { .br_bw = FLRC_BANDWIDTH,
                                                                .cr    = FLRC_CODING_RATE,
                                                                .shape = FLRC_PULSE_SHAPE };

static const lr20xx_radio_flrc_pkt_params_t flrc_pkt_params = { .preamble_len     = FLRC_PREAMBLE_LEN,
                                                                .sync_word_len    = FLRC_SYNCWORD_LEN,
                                                                .match_sync_word  = FLRC_RX_SYNCWORD_MATCH,
                                                                .header_type      = FLRC_PKT_LEN_MODE,
                                                                .pld_len_in_bytes = PAYLOAD_LENGTH,
                                                                .crc_type         = FLRC_CRC_TYPE,
                                                                .tx_syncword      = FLRC_TX_SYNCWORD };

static const lr20xx_radio_bpsk_mod_params_t bpsk_mod_params = {
    .differential_encoding_enable        = LR20XX_RADIO_BPSK_DIFFERENTIAL_ENCODING_ENABLE,
    .bitrate_unit                        = LR20XX_RADIO_BPSK_MOD_PARAMS_BR_IN_BPS,
    .bitrate                             = BPSK_BITRATE_IN_BPS,
    .pulse_shape                         = LR20XX_RADIO_BPSK_MOD_PARAMS_SHAPE_FILTER_GAUSSIAN_BT_0_5,
    .differential_encoding_init_parity   = true,
    .differential_encoding_init_previous = false
};

static lr20xx_radio_bpsk_pkt_params_t bpsk_pkt_params = {
    .payload_length      = 0,    // Will be initialized in radio init
    .phy_mode            = LR20XX_RADIO_BPSK_PHY_MODE_SIGFOX,
    .sigfox_message_type = LR20XX_RADIO_BPSK_SIGFOX_MESSAGE_TYPE_APPLICATION,
    .sigfox_frame_rank   = LR20XX_RADIO_BPSK_SIGFOX_FRAME_RANK_FIRST_LAST,
};

static const lr20xx_radio_ook_mod_params_t ook_mod_params = {
    .br          = OOK_BITRATE,
    .bw          = OOK_BANDWIDTH,
    .mag_depth   = OOK_MAG_DEPTH,
    .pulse_shape = { .bt = OOK_PULSE_SHAPE, .filter = OOK_PULSE_SHAPE_FILTER }
};

static lr20xx_radio_ook_pkt_params_t        ook_pck_params         = { .pbl_length_in_bit = OOK_PREAMBLE_LENGTH_BITS,
                                                                       .address_filtering = OOK_ADDRESS_FILTERING,
                                                                       .header_mode       = OOK_HEADER_MODE,
                                                                       .payload_length    = PAYLOAD_LENGTH,
                                                                       .crc               = OOK_CRC,
                                                                       .encoding          = OOK_ENCODING };

static const lr20xx_radio_ook_rx_detector_t ook_rx_detector_params = {
    .pattern               = 0x5,
    .pattern_length_in_bit = 3,
    .pattern_repeat_nb     = 8,
    .sfd_type              = LR20XX_RADIO_OOK_RX_DETECTOR_SFD_TYPE_FALLING_EDGE,
    .sfd_length_in_bit     = 0,
    .is_syncword_encoded   = false
};

static const lr20xx_rttof_params_t rttof_mod_params = {
    .mode      = LR20XX_RTTOF_MODE_NORMAL,
    .spy_mode  = LR20XX_RTTOF_SPY_MODE_DISABLED,
    .nb_symbol = RESPONSE_SYMBOLS_COUNT,
};

static uint32_t                       fsk_syncword_length = FSK_SYNCWORD_LENGTH;
static lr20xx_radio_common_pkt_type_t packet_type         = PACKET_TYPE;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

static lr20xx_hal_context_t               context;

static volatile bool                      irq_fired     = false;
static volatile lr20xx_mesh_dio_cb_t      s_mesh_dio_cb = 0;

// static const smtc_shield_lr20xx_pinout_t* shield_pinout = 0;

// static struct
// {
//     smtc_hal_mcu_gpio_cfg_t        cfg;
//     smtc_hal_mcu_gpio_output_cfg_t cfg_output;
//     smtc_hal_mcu_gpio_inst_t       inst;
// } led_tx;

// static struct
// {
//     smtc_hal_mcu_gpio_cfg_t        cfg;
//     smtc_hal_mcu_gpio_output_cfg_t cfg_output;
//     smtc_hal_mcu_gpio_inst_t       inst;
// } led_rx;

// static struct
// {
//     smtc_hal_mcu_gpio_cfg_t        cfg;
//     smtc_hal_mcu_gpio_output_cfg_t cfg_output;
//     smtc_hal_mcu_gpio_inst_t       inst;
// } led_scan;

// static struct
// {
//     smtc_hal_mcu_gpio_cfg_t        cfg;
//     smtc_hal_mcu_gpio_output_cfg_t cfg_output;
//     smtc_hal_mcu_gpio_inst_t       inst;
// } lna_ctrl;

static char dbm_string_buf[ DBM_STRING_LEN ] = { 0 };

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/*!
 * @brief Print the common configuration on the debug interface
 */
static void print_common_configuration( void );

/*!
 * @brief Print the LoRa configuration on the debug interface
 */
static void print_lora_configuration( void );

/*!
 * @brief Print the FSK configuration on the debug interface
 */
static void print_fsk_configuration( void );

/*!
 * @brief Print the OOK configuration on the debug interface
 */
static void print_ook_configuration( void );

/*!
 * @brief Print the FLRC configuration on the debug interface
 */
static void print_flrc_configuration( void );

/*!
 * @brief Print the LR11XX driver version
 */
static void print_driver_version( void );

/*!
 * @brief Evaluates if the currently used header is implicit or not
 *
 * @return True if header is implicit, False otherwise
 */
static bool is_header_implicit( void );

/*!
 * @brief Callback function called upon detection of GPIO lr20xx irq
 *
 * @param [in] context  radio context
 */
void radio_on_dio_irq( void* context );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC VARIABLES --------------------------------------------------------
 */
void smtc_hal_mcu_wait_ms( const uint32_t milliseconds )
{
    vTaskDelay( pdMS_TO_TICKS( milliseconds ) );
}
/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

// lr20xx_hal_context_t* apps_common_lr20xx_get_context( void )
// {
//     context.busy.cfg = (smtc_hal_mcu_gpio_cfg_t) smtc_shield_pinout_mapping_get_gpio_cfg( shield_pinout->busy );
//     context.busy.cfg_input.pull_mode = SMTC_HAL_MCU_GPIO_PULL_MODE_NONE;
//     context.busy.cfg_input.irq_mode  = SMTC_HAL_MCU_GPIO_IRQ_MODE_OFF;
//     context.busy.cfg_input.callback  = NULL;

//     context.irq.cfg = (smtc_hal_mcu_gpio_cfg_t) smtc_shield_pinout_mapping_get_gpio_cfg( shield_pinout->irq );
//     context.irq.cfg_input.pull_mode = SMTC_HAL_MCU_GPIO_PULL_MODE_NONE;
//     context.irq.cfg_input.irq_mode  = SMTC_HAL_MCU_GPIO_IRQ_MODE_RISING;
//     context.irq.cfg_input.callback  = radio_on_dio_irq;
//     context.irq.cfg_input.context   = NULL;

//     context.nss.cfg = (smtc_hal_mcu_gpio_cfg_t) smtc_shield_pinout_mapping_get_gpio_cfg( shield_pinout->nss );
//     context.nss.cfg_output.initial_state = SMTC_HAL_MCU_GPIO_STATE_HIGH;
//     context.nss.cfg_output.mode          = SMTC_HAL_MCU_GPIO_OUTPUT_MODE_PUSH_PULL;

//     context.reset.cfg = (smtc_hal_mcu_gpio_cfg_t) smtc_shield_pinout_mapping_get_gpio_cfg( shield_pinout->reset );
//     context.reset.cfg_output.initial_state = SMTC_HAL_MCU_GPIO_STATE_HIGH;
//     context.reset.cfg_output.mode          = SMTC_HAL_MCU_GPIO_OUTPUT_MODE_PUSH_PULL;

//     if( g_boardType == DEV_BOARD_DK01 )
//         context.spi.cfg.spi = SPI2;
//     else
//         context.spi.cfg.spi = SPI3;

//     smtc_hal_mcu_gpio_init_input( context.busy.cfg, &( context.busy.cfg_input ), &( context.busy.inst ) );
//     smtc_hal_mcu_gpio_init_input( context.irq.cfg, &( context.irq.cfg_input ), &( context.irq.inst ) );
//     smtc_hal_mcu_gpio_init_output( context.nss.cfg, &( context.nss.cfg_output ), &( context.nss.inst ) );
//     smtc_hal_mcu_gpio_init_output( context.reset.cfg, &( context.reset.cfg_output ), &( context.reset.inst ) );

//     smtc_hal_mcu_gpio_enable_irq( context.irq.inst );

//     const smtc_hal_mcu_spi_cfg_app_t cfg = { .is_master = true };

//     smtc_hal_mcu_spi_init( &( context.spi.cfg ), &cfg, &( context.spi.inst ) );

//     return &context;
// }

void* apps_common_lr20xx_get_radio_context( void )
{
    return (void*)&context;
}

// void apps_common_shield_init( void )
// {
//     shield_pinout = smtc_shield_lr20xx_get_pinout();

//     if( shield_pinout->led_tx != SMTC_SHIELD_PINOUT_NONE )
//     {
//         led_tx.cfg = (smtc_hal_mcu_gpio_cfg_t) smtc_shield_pinout_mapping_get_gpio_cfg( shield_pinout->led_tx );
//         led_tx.cfg_output.initial_state = SMTC_HAL_MCU_GPIO_STATE_LOW;
//         led_tx.cfg_output.mode          = SMTC_HAL_MCU_GPIO_OUTPUT_MODE_PUSH_PULL;

//         smtc_hal_mcu_gpio_init_output( led_tx.cfg, &( led_tx.cfg_output ), &( led_tx.inst ) );
//     }

//     if( shield_pinout->led_rx != SMTC_SHIELD_PINOUT_NONE )
//     {
//         led_rx.cfg = (smtc_hal_mcu_gpio_cfg_t) smtc_shield_pinout_mapping_get_gpio_cfg( shield_pinout->led_rx );
//         led_rx.cfg_output.initial_state = SMTC_HAL_MCU_GPIO_STATE_LOW;
//         led_rx.cfg_output.mode          = SMTC_HAL_MCU_GPIO_OUTPUT_MODE_PUSH_PULL;

//         smtc_hal_mcu_gpio_init_output( led_rx.cfg, &( led_rx.cfg_output ), &( led_rx.inst ) );
//     }

//     if( shield_pinout->led_scan != SMTC_SHIELD_PINOUT_NONE )
//     {
//         led_scan.cfg = (smtc_hal_mcu_gpio_cfg_t) smtc_shield_pinout_mapping_get_gpio_cfg( shield_pinout->led_scan );
//         led_scan.cfg_output.initial_state = SMTC_HAL_MCU_GPIO_STATE_LOW;
//         led_scan.cfg_output.mode          = SMTC_HAL_MCU_GPIO_OUTPUT_MODE_PUSH_PULL;

//         smtc_hal_mcu_gpio_init_output( led_scan.cfg, &( led_scan.cfg_output ), &( led_scan.inst ) );
//     }

//     if( shield_pinout->lna != SMTC_SHIELD_PINOUT_NONE )
//     {
//         lna_ctrl.cfg = (smtc_hal_mcu_gpio_cfg_t) smtc_shield_pinout_mapping_get_gpio_cfg( shield_pinout->lna );
//         lna_ctrl.cfg_output.initial_state = SMTC_HAL_MCU_GPIO_STATE_LOW;
//         lna_ctrl.cfg_output.mode          = SMTC_HAL_MCU_GPIO_OUTPUT_MODE_PUSH_PULL;

//         smtc_hal_mcu_gpio_init_output( lna_ctrl.cfg, &( lna_ctrl.cfg_output ), &( lna_ctrl.inst ) );
//     }
// }

void apps_common_lr20xx_set_dio_irq_params( const void*                         context,
                                            const apps_common_lr20xx_irq_cfg_t* irq_cfg,
                                            const uint8_t                       irq_cfg_len )
{
    lr20xx_system_clear_irq_status( context, LR20XX_SYSTEM_IRQ_ALL_MASK );

    for( uint8_t i = 0; i < irq_cfg_len; i++ )
    {
        lr20xx_system_dio_t             dio          = irq_cfg->dio;
        const lr20xx_system_dio_func_t* dio_function = smtc_shield_lr20xx_get_dio_function( dio );

        if( *dio_function != LR20XX_SYSTEM_DIO_FUNC_IRQ )
        {
            continue;
        }

        lr20xx_system_set_dio_irq_cfg( context, dio, irq_cfg->irq_mask );

        irq_cfg += sizeof( apps_common_lr20xx_irq_cfg_t );
    }
}

void apps_common_lr20xx_system_init( const lr20xx_hal_context_t* context )
{
    ASSERT_LR20XX_RC( lr20xx_system_reset( (void*) context ) );

    // Workaround SIMO
    const uint32_t freq_val = 2.8e6 * 1.048576;
    lr20xx_regmem_write_regmem32( context, 0x80004c, &freq_val, 1 );

    // Configure the regulator
    const lr20xx_system_reg_mode_t* regulator = smtc_shield_lr20xx_get_reg_mode();
    ASSERT_LR20XX_RC( lr20xx_system_set_reg_mode( (void*) context, *regulator ) );

    /* 有源/无源只认编译宏 W20_LORA_HAS_TCXO（platformio -D）：
     * 1 = 有源 TCXO → set_tcxo_mode；0 = 无源 XTAL → 跳过。
     * supply/startup 仍取自 shield；宏为唯一开关，避免与旧 has_tcxo 缓存打架。 */
#ifndef W20_LORA_HAS_TCXO
#define W20_LORA_HAS_TCXO 1
#endif
    const smtc_shield_lr20xx_xosc_cfg_t* tcxo_cfg = smtc_shield_lr20xx_get_xosc_cfg();
#if W20_LORA_HAS_TCXO
    {
        lr20xx_system_tcxo_supply_voltage_t supply = tcxo_cfg->supply;
        uint32_t startup = tcxo_cfg->startup_time_in_tick;
        if( startup == 0 ) {
            startup = 160000; /* ~5ms @ 32MHz */
        }
        ASSERT_LR20XX_RC( lr20xx_system_set_tcxo_mode( context, supply, startup ) );
        HAL_DBG_TRACE_INFO( "XOSC: active TCXO (W20_LORA_HAS_TCXO=1) supply=%u startup_ticks=%lu\n",
                            (unsigned) supply, (unsigned long) startup );
    }
#else
    (void) tcxo_cfg;
    /* 无源 XTAL：不调用 set_tcxo_mode */
    HAL_DBG_TRACE_INFO( "XOSC: passive XTAL (W20_LORA_HAS_TCXO=0)\n" );
#endif

    const lr20xx_system_lfclk_cfg_t* lfclk_cfg = smtc_shield_lr20xx_get_lfclk_cfg();
    ASSERT_LR20XX_RC( lr20xx_system_cfg_lfclk( context, *lfclk_cfg ) );

    uint16_t errors;
    lr20xx_system_get_errors( context, &errors );
    if( errors != 0 )
    {
        lr20xx_system_clear_errors( context );
    }

    ASSERT_LR20XX_RC( lr20xx_system_clear_irq_status( context, LR20XX_SYSTEM_IRQ_ALL_MASK ) );

    uint8_t dio_count = lr20xx_system_dio_get_count();
    for( uint8_t dio_nth = 0; dio_nth < dio_count; dio_nth++ )
    {
        lr20xx_system_dio_t dio;
        if( ! lr20xx_system_dio_get_nth( dio_nth, &dio ) )
        {
            return;
        }

        const lr20xx_system_dio_func_t*  dio_function = smtc_shield_lr20xx_get_dio_function( dio );
        const lr20xx_system_dio_drive_t* dio_drive    = smtc_shield_lr20xx_get_dio_sleep_drive( dio );

        if( *dio_function == LR20XX_SYSTEM_DIO_FUNC_HF_CLK_OUT )
        {
            lr20xx_system_cfg_clk_output( context, LR20XX_SYSTEM_HF_CLK_SCALING_31250_HZ );
        }

        lr20xx_system_set_dio_function( context, dio, *dio_function, *dio_drive );

        if( *dio_function == LR20XX_SYSTEM_DIO_FUNC_RF_SWITCH )
        {
            const lr20xx_system_dio_rf_switch_cfg_t* rf_switch_cfg = smtc_shield_lr20xx_get_rf_switch_cfg( dio );
            lr20xx_system_set_dio_rf_switch_cfg( context, dio, *rf_switch_cfg );
        }
    }
}

void apps_common_lr20xx_fetch_and_print_version( const lr20xx_hal_context_t* context )
{
    lr20xx_system_version_t version;

    apps_common_lr20xx_fetch_version( (void*) context, &version );
    apps_common_lr20xx_print_version( &version );
}

void apps_common_lr20xx_fetch_version( const lr20xx_hal_context_t* context, lr20xx_system_version_t* version )
{
    ASSERT_LR20XX_RC( lr20xx_system_get_version( (void*) context, version ) );
}

void apps_common_lr20xx_configure_and_get_inst_rssi( const void* context, int16_t* threshold_level_db )
{
    ASSERT_LR20XX_RC( lr20xx_system_set_standby_mode( context, LR20XX_SYSTEM_STANDBY_MODE_XOSC ) );
    apps_common_lr20xx_configure_rssi_instantaneous_rx_mode( context, RF_FREQ_IN_HZ, TX_OUTPUT_POWER_HALF_DBM_STEPS );
    ASSERT_LR20XX_RC( lr20xx_radio_common_set_rx( context, 0 ) );

    smtc_hal_mcu_wait_ms( 1u );
    int16_t result = 0;
    apps_common_lr20xx_radio_get_rssi_inst( context, &result );
    ASSERT_LR20XX_RC( lr20xx_system_set_standby_mode( context, LR20XX_SYSTEM_STANDBY_MODE_XOSC ) );
    *threshold_level_db = result;
}

void apps_common_lr20xx_configure_rssi_instantaneous_rx_mode( const void* context, uint32_t freq, int8_t pwr )
{
    ASSERT_LR20XX_RC( lr20xx_radio_common_set_pkt_type( context, LR20XX_RADIO_COMMON_PKT_TYPE_FSK ) );

    const smtc_shield_lr20xx_pa_pwr_cfg_t* pa_pwr_cfg = smtc_shield_lr20xx_get_pa_pwr_cfg( freq, pwr );

    if( pa_pwr_cfg == NULL )
    {
        HAL_DBG_TRACE_ERROR( "Invalid target frequency or power level\n" );
        while( true )
        {
        }
    }

    lr20xx_radio_common_set_rf_freq( context, freq );

    lr20xx_radio_common_rx_path_t rx_path = pa_pwr_cfg->pa_config.pa_sel == LR20XX_RADIO_COMMON_PA_SEL_HF
                                                ? LR20XX_RADIO_COMMON_RX_PATH_HF
                                                : LR20XX_RADIO_COMMON_RX_PATH_LF;
    lr20xx_radio_common_front_end_calibration_value_t calibration = { .frequency_in_hertz = freq, .rx_path = rx_path };
    lr20xx_radio_common_calibrate_front_end_helper( context, &calibration, 1 );

    lr20xx_radio_common_set_rx_path( context, rx_path, ENABLE_RX_BOOST_MODE );

    fsk_mod_params.bw = FSK_RSSI_INST_BW;

    ASSERT_LR20XX_RC( lr20xx_radio_fsk_set_modulation_params( context, &fsk_mod_params ) );
    ASSERT_LR20XX_RC( lr20xx_radio_fsk_set_packet_params( context, &fsk_pkt_params ) );
}

void apps_common_lr20xx_print_version( const lr20xx_system_version_t* version )
{
    HAL_DBG_TRACE_INFO( "LR20xx information:\n" );
    HAL_DBG_TRACE_INFO( "  - Firmware mayor = 0x%04X\n", version->major );
    HAL_DBG_TRACE_INFO( "  - Firmware minor = 0x%02X\n", version->minor );
    HAL_DBG_TRACE_PRINTF( "\n" );
}

void apps_common_print_sdk_driver_version( void )
{
    // common_version_print();
    print_driver_version();
    HAL_DBG_TRACE_INFO( "\n" );
}

void apps_common_lr20xx_radio_get_rssi_inst( const void* context, int16_t* result )
{
    int16_t rssi_in_dbm    = 0;
    uint8_t half_dbm_count = 0;
    lr20xx_radio_common_get_rssi_inst( (void*) context, &rssi_in_dbm, &half_dbm_count );
    *result = rssi_in_dbm - ( half_dbm_count * 0.5 );
}

void apps_common_lr20xx_radio_init( const void* context )
{
    print_common_configuration();

    ASSERT_LR20XX_RC( lr20xx_radio_common_set_pkt_type( context, packet_type ) );

    uint32_t                               freq       = RF_FREQ_IN_HZ;
    int8_t                                 tx_pwr     = TX_OUTPUT_POWER_HALF_DBM_STEPS;

    const smtc_shield_lr20xx_pa_pwr_cfg_t* pa_pwr_cfg = smtc_shield_lr20xx_get_pa_pwr_cfg( freq, tx_pwr );

    if( pa_pwr_cfg == NULL )
    {
        HAL_DBG_TRACE_ERROR( "Invalid target frequency or power level\n" );
        while( true )
        {
        }
    }

    ASSERT_LR20XX_RC( lr20xx_radio_common_set_rf_freq( context, freq ) );

    lr20xx_radio_common_rx_path_t rx_path = pa_pwr_cfg->pa_config.pa_sel == LR20XX_RADIO_COMMON_PA_SEL_HF
                                                ? LR20XX_RADIO_COMMON_RX_PATH_HF
                                                : LR20XX_RADIO_COMMON_RX_PATH_LF;
    lr20xx_radio_common_front_end_calibration_value_t calibration = { .frequency_in_hertz = freq, .rx_path = rx_path };
    ASSERT_LR20XX_RC( lr20xx_radio_common_calibrate_front_end_helper( context, &calibration, 1 ) );

    int8_t tx_pwr_params_cfg = pa_pwr_cfg->power;

    if( tx_pwr_params_cfg == 0 )
    {
        tx_pwr_params_cfg = tx_pwr;
    }

    ASSERT_LR20XX_RC( lr20xx_radio_common_set_pa_cfg( context, &( pa_pwr_cfg->pa_config ) ) );
    ASSERT_LR20XX_RC( lr20xx_radio_common_set_tx_params( context, tx_pwr_params_cfg, PA_RAMP_TIME ) );
    ASSERT_LR20XX_RC( lr20xx_radio_common_set_rx_path( context, rx_path, ENABLE_RX_BOOST_MODE ) );
    ASSERT_LR20XX_RC( lr20xx_radio_common_set_rx_tx_fallback_mode( context, FALLBACK_MODE ) );

    switch( packet_type )
    {
        case LR20XX_RADIO_COMMON_PKT_TYPE_LORA:
            print_lora_configuration();

            lora_mod_params.ppm = apps_common_compute_lora_ppm( LORA_SPREADING_FACTOR, LORA_BANDWIDTH );
            ASSERT_LR20XX_RC( lr20xx_radio_lora_set_modulation_params( context, &lora_mod_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_lora_set_packet_params( context, &lora_pkt_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_lora_set_syncword( context, LORA_SYNCWORD ) );
            break;
        case LR20XX_RADIO_COMMON_PKT_TYPE_FSK:
            print_fsk_configuration();

            ASSERT_LR20XX_RC( lr20xx_radio_fsk_set_modulation_params( context, &fsk_mod_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_fsk_set_packet_params( context, &fsk_pkt_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_fsk_set_syncword( context,
                                                             gfsk_sync_word,
                                                             fsk_syncword_length,
                                                             LR20XX_RADIO_FSK_SYNCWORD_MSBF ) );

            if( FSK_DC_FREE != LR20XX_RADIO_FSK_WHITENING_OFF )
            {
                ASSERT_LR20XX_RC(
                    lr20xx_radio_fsk_set_whitening_params( context,
                                                           LR20XX_RADIO_FSK_WHITENING_COMPATIBILITY_SX126X_LR11XX,
                                                           FSK_WHITENING_SEED ) );
            }

            if( FSK_CRC_TYPE != LR20XX_RADIO_FSK_CRC_OFF )
            {
                ASSERT_LR20XX_RC( lr20xx_radio_fsk_set_crc_params( context, FSK_CRC_POLYNOMIAL, FSK_CRC_SEED ) );
            }

            if( FSK_ADDRESS_FILTERING != LR20XX_RADIO_FSK_ADDRESS_FILTERING_DISABLED )
            {
                ASSERT_LR20XX_RC( lr20xx_radio_fsk_set_addresses( context, FSK_NODE_ADDRESS, FSK_BROADCAST_ADDRESS ) );
            }
            break;
        case LR20XX_RADIO_COMMON_PKT_TYPE_FLRC:
            print_flrc_configuration();

            ASSERT_LR20XX_RC( lr20xx_radio_flrc_set_modulation_params( context, &flrc_mod_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_flrc_set_pkt_params( context, &flrc_pkt_params ) );

            if( FLRC_USE_DEFAULT_SYNC_WORD == false )
            {
                ASSERT_LR20XX_RC( lr20xx_radio_flrc_set_syncword( context, 1, flrc_sync_word ) );
            }
            break;
        case LR20XX_RADIO_COMMON_PKT_TYPE_OOK:
            print_ook_configuration();

            ASSERT_LR20XX_RC( lr20xx_radio_ook_set_packet_params( context, &ook_pck_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_ook_set_modulation_params( context, &ook_mod_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_ook_set_rx_detector( context, &ook_rx_detector_params ) );

            ASSERT_LR20XX_RC(
                lr20xx_radio_ook_set_syncword( context, ook_sync_word, 32U, LR20XX_RADIO_OOK_SYNCWORD_MSBF ) );

            if( OOK_CRC != LR20XX_RADIO_OOK_CRC_OFF )
            {
                ASSERT_LR20XX_RC( lr20xx_radio_ook_set_crc_params( context,
                                                                   (uint32_t) OOK_CRC_POLYNOMIAL,
                                                                   (uint32_t) OOK_CRC_SEED ) );
            }
            if( OOK_ADDRESS_FILTERING != LR20XX_RADIO_OOK_ADDRESS_FILTERING_DISABLED )
            {
                lr20xx_radio_ook_set_addresses( context, (uint8_t) OOK_NODE_ADDRESS, (uint8_t) OOK_BROADCAST_ADDRESS );
            }

            const lr20xx_radio_ook_whitening_params_t ook_whitening_params = { .bit_index  = 1,
                                                                               .polynomial = OOK_WHITENING_POLYNOMIAL,
                                                                               .seed       = OOK_WHITENING_SEED };
            ASSERT_LR20XX_RC( lr20xx_radio_ook_set_whitening_params( context, &ook_whitening_params ) );
            break;
        case LR20XX_RADIO_COMMON_PKT_TYPE_RTTOF:
            print_lora_configuration();

            lora_mod_params.ppm = apps_common_compute_lora_ppm( LORA_SPREADING_FACTOR, LORA_BANDWIDTH );
            ASSERT_LR20XX_RC( lr20xx_radio_lora_set_modulation_params( context, &lora_mod_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_lora_set_packet_params( context, &lora_pkt_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_lora_set_syncword( context, LORA_SYNCWORD ) );

            ASSERT_LR20XX_RC( lr20xx_rttof_set_params( context, &rttof_mod_params ) );
            uint32_t rttof_rx_tx_delay = 0u;
            if( smtc_shield_lr20xx_rttof_recommended_rx_tx_delay_indicator( RF_FREQ_IN_HZ,
                                                                            lora_mod_params.bw,
                                                                            lora_mod_params.sf,
                                                                            &rttof_rx_tx_delay ) )
            {
                ASSERT_LR20XX_RC( lr20xx_rttof_set_tx_rx_delay( context, rttof_rx_tx_delay ) );
                HAL_DBG_TRACE_INFO( "RTToF delay : %d\n\n", rttof_rx_tx_delay );
            }
            else
            {
                HAL_DBG_TRACE_ERROR( "Failed to get RTToF delay indicator\n" );
            }
            break;
        default:
            break;
    }
}

void my_apps_common_lr20xx_radio_init_first( const void* context,
                                             uint8_t     packet_set,
                                             uint32_t    freq_set,
                                             uint8_t     pwr_set )
{
    ASSERT_LR20XX_RC( lr20xx_radio_common_set_pkt_type( context, packet_set ) );
    packet_type                                       = packet_set;

    uint32_t                               freq       = freq_set;
    int8_t                                 tx_pwr     = pwr_set;

    const smtc_shield_lr20xx_pa_pwr_cfg_t* pa_pwr_cfg = smtc_shield_lr20xx_get_pa_pwr_cfg( freq, tx_pwr );

    if( pa_pwr_cfg == NULL )
    {
        HAL_DBG_TRACE_ERROR( "Invalid target frequency or power level\n" );
        while( true )
        {
        }
    }

    ASSERT_LR20XX_RC( lr20xx_radio_common_set_rf_freq( context, freq ) );

    lr20xx_radio_common_rx_path_t rx_path = pa_pwr_cfg->pa_config.pa_sel == LR20XX_RADIO_COMMON_PA_SEL_HF
                                                ? LR20XX_RADIO_COMMON_RX_PATH_HF
                                                : LR20XX_RADIO_COMMON_RX_PATH_LF;
    lr20xx_radio_common_front_end_calibration_value_t calibration = { .frequency_in_hertz = freq, .rx_path = rx_path };
    ASSERT_LR20XX_RC( lr20xx_radio_common_calibrate_front_end_helper( context, &calibration, 1 ) );

    int8_t tx_pwr_params_cfg = pa_pwr_cfg->power;

    if( tx_pwr_params_cfg == 0 )
    {
        tx_pwr_params_cfg = tx_pwr;
    }

    ASSERT_LR20XX_RC( lr20xx_radio_common_set_pa_cfg( context, &( pa_pwr_cfg->pa_config ) ) );
    ASSERT_LR20XX_RC( lr20xx_radio_common_set_tx_params( context, tx_pwr_params_cfg, PA_RAMP_TIME ) );
    ASSERT_LR20XX_RC( lr20xx_radio_common_set_rx_path( context, rx_path, ENABLE_RX_BOOST_MODE ) );
    ASSERT_LR20XX_RC( lr20xx_radio_common_set_rx_tx_fallback_mode( context, FALLBACK_MODE ) );
}
void my_apps_common_lr20xx_radio_init_second( const void* context, uint8_t packet_set )
{
    switch( packet_set )
    {
        case LR20XX_RADIO_COMMON_PKT_TYPE_LORA:
            print_lora_configuration();

            lora_mod_params.ppm = apps_common_compute_lora_ppm( LORA_SPREADING_FACTOR, LORA_BANDWIDTH );
            ASSERT_LR20XX_RC( lr20xx_radio_lora_set_modulation_params( context, &lora_mod_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_lora_set_packet_params( context, &lora_pkt_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_lora_set_syncword( context, LORA_SYNCWORD ) );
            break;
        case LR20XX_RADIO_COMMON_PKT_TYPE_FSK:
            print_fsk_configuration();

            ASSERT_LR20XX_RC( lr20xx_radio_fsk_set_modulation_params( context, &fsk_mod_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_fsk_set_packet_params( context, &fsk_pkt_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_fsk_set_syncword( context,
                                                             gfsk_sync_word,
                                                             fsk_syncword_length,
                                                             LR20XX_RADIO_FSK_SYNCWORD_MSBF ) );

            if( FSK_DC_FREE != LR20XX_RADIO_FSK_WHITENING_OFF )
            {
                ASSERT_LR20XX_RC(
                    lr20xx_radio_fsk_set_whitening_params( context,
                                                           LR20XX_RADIO_FSK_WHITENING_COMPATIBILITY_SX126X_LR11XX,
                                                           FSK_WHITENING_SEED ) );
            }

            if( FSK_CRC_TYPE != LR20XX_RADIO_FSK_CRC_OFF )
            {
                ASSERT_LR20XX_RC( lr20xx_radio_fsk_set_crc_params( context, FSK_CRC_POLYNOMIAL, FSK_CRC_SEED ) );
            }

            if( FSK_ADDRESS_FILTERING != LR20XX_RADIO_FSK_ADDRESS_FILTERING_DISABLED )
            {
                ASSERT_LR20XX_RC( lr20xx_radio_fsk_set_addresses( context, FSK_NODE_ADDRESS, FSK_BROADCAST_ADDRESS ) );
            }
            break;
        case LR20XX_RADIO_COMMON_PKT_TYPE_FLRC:
            print_flrc_configuration();

            ASSERT_LR20XX_RC( lr20xx_radio_flrc_set_modulation_params( context, &flrc_mod_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_flrc_set_pkt_params( context, &flrc_pkt_params ) );

            if( FLRC_USE_DEFAULT_SYNC_WORD == false )
            {
                ASSERT_LR20XX_RC( lr20xx_radio_flrc_set_syncword( context, 1, flrc_sync_word ) );
            }
            break;
        case LR20XX_RADIO_COMMON_PKT_TYPE_OOK:
            print_ook_configuration();

            ASSERT_LR20XX_RC( lr20xx_radio_ook_set_packet_params( context, &ook_pck_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_ook_set_modulation_params( context, &ook_mod_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_ook_set_rx_detector( context, &ook_rx_detector_params ) );

            ASSERT_LR20XX_RC(
                lr20xx_radio_ook_set_syncword( context, ook_sync_word, 32U, LR20XX_RADIO_OOK_SYNCWORD_MSBF ) );

            if( OOK_CRC != LR20XX_RADIO_OOK_CRC_OFF )
            {
                ASSERT_LR20XX_RC( lr20xx_radio_ook_set_crc_params( context,
                                                                   (uint32_t) OOK_CRC_POLYNOMIAL,
                                                                   (uint32_t) OOK_CRC_SEED ) );
            }
            if( OOK_ADDRESS_FILTERING != LR20XX_RADIO_OOK_ADDRESS_FILTERING_DISABLED )
            {
                lr20xx_radio_ook_set_addresses( context, (uint8_t) OOK_NODE_ADDRESS, (uint8_t) OOK_BROADCAST_ADDRESS );
            }

            const lr20xx_radio_ook_whitening_params_t ook_whitening_params = { .bit_index  = 1,
                                                                               .polynomial = OOK_WHITENING_POLYNOMIAL,
                                                                               .seed       = OOK_WHITENING_SEED };
            ASSERT_LR20XX_RC( lr20xx_radio_ook_set_whitening_params( context, &ook_whitening_params ) );
            break;
        case LR20XX_RADIO_COMMON_PKT_TYPE_RTTOF:
            print_lora_configuration();

            lora_mod_params.ppm = apps_common_compute_lora_ppm( LORA_SPREADING_FACTOR, LORA_BANDWIDTH );
            ASSERT_LR20XX_RC( lr20xx_radio_lora_set_modulation_params( context, &lora_mod_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_lora_set_packet_params( context, &lora_pkt_params ) );
            ASSERT_LR20XX_RC( lr20xx_radio_lora_set_syncword( context, LORA_SYNCWORD ) );

            ASSERT_LR20XX_RC( lr20xx_rttof_set_params( context, &rttof_mod_params ) );
            uint32_t rttof_rx_tx_delay = 0u;
            if( smtc_shield_lr20xx_rttof_recommended_rx_tx_delay_indicator( RF_FREQ_IN_HZ,
                                                                            lora_mod_params.bw,
                                                                            lora_mod_params.sf,
                                                                            &rttof_rx_tx_delay ) )
            {
                ASSERT_LR20XX_RC( lr20xx_rttof_set_tx_rx_delay( context, rttof_rx_tx_delay ) );
                HAL_DBG_TRACE_INFO( "RTToF delay : %d\n\n", rttof_rx_tx_delay );
            }
            else
            {
                HAL_DBG_TRACE_ERROR( "Failed to get RTToF delay indicator\n" );
            }
            break;
        default:
            break;
    }
}

void apps_common_lr20xx_radio_lrfhss_init( const void* context )
{
    HAL_DBG_TRACE_INFO( "LR-FHSS parameters:\n" );
    HAL_DBG_TRACE_INFO( "   RF frequency  = %u Hz\n", RF_FREQ_IN_HZ );
    HAL_DBG_TRACE_INFO( "   Output power  = %i dBm\n", TX_OUTPUT_POWER_HALF_DBM_STEPS / 2 );
    HAL_DBG_TRACE_INFO( "   Bandwidth  = %s\n", lr_fhss_v1_bw_to_str( LR_FHSS_BANDWIDTH ) );
    HAL_DBG_TRACE_INFO( "   Coding rate  = %s\n", lr_fhss_v1_cr_to_str( LR_FHSS_CODING_RATE ) );
    HAL_DBG_TRACE_INFO( "   Grid = %s\n", lr_fhss_v1_grid_to_str( LR_FHSS_GRID ) );
    HAL_DBG_TRACE_INFO( "   Header count = %u\n", (uint8_t) LR_FHSS_HEADER_COUNT );
    HAL_DBG_TRACE_INFO( "   Modulation type = %s\n", lr_fhss_v1_modulation_type_to_str( LR_FHSS_MODULATION_TYPE ) );
    HAL_DBG_TRACE_INFO( "   Hopping enabled  = %s\n\n", LR_FHSS_ENABLE_HOPPING == true ? "true" : "false" );

    ASSERT_LR20XX_RC( lr20xx_radio_common_set_pkt_type( context, LR20XX_RADIO_COMMON_PKT_TYPE_LRFHSS ) );

    uint32_t                               freq       = RF_FREQ_IN_HZ;
    int8_t                                 tx_pwr     = TX_OUTPUT_POWER_HALF_DBM_STEPS;

    const smtc_shield_lr20xx_pa_pwr_cfg_t* pa_pwr_cfg = smtc_shield_lr20xx_get_pa_pwr_cfg( freq, tx_pwr );

    if( pa_pwr_cfg == NULL )
    {
        HAL_DBG_TRACE_ERROR( "Invalid target frequency or power level\n" );
        while( true )
        {
        }
    }

    if( pa_pwr_cfg->pa_config.pa_sel == LR20XX_RADIO_COMMON_PA_SEL_HF )
    {
        if( tx_pwr > 24 )
        {
            tx_pwr = 24;
        }
    }
    else
    {
        if( tx_pwr > 44 )
        {
            tx_pwr = 44;
        }
    }

    if( pa_pwr_cfg == NULL )
    {
        while( true )
        {
        }
    }

    lr20xx_radio_common_set_rf_freq( context, freq );

    lr20xx_radio_common_rx_path_t rx_path = pa_pwr_cfg->pa_config.pa_sel == LR20XX_RADIO_COMMON_PA_SEL_HF
                                                ? LR20XX_RADIO_COMMON_RX_PATH_HF
                                                : LR20XX_RADIO_COMMON_RX_PATH_LF;
    lr20xx_radio_common_front_end_calibration_value_t calibration = { .frequency_in_hertz = freq, .rx_path = rx_path };
    lr20xx_radio_common_calibrate_front_end_helper( context, &calibration, 1 );

    lr20xx_radio_common_set_pa_cfg( context, &( pa_pwr_cfg->pa_config ) );
    lr20xx_radio_common_set_tx_params( context, tx_pwr, PA_RAMP_TIME );
    lr20xx_radio_common_set_rx_path( context, rx_path, ENABLE_RX_BOOST_MODE );
    lr20xx_radio_common_set_rx_tx_fallback_mode( context, FALLBACK_MODE );
}

lr20xx_status_t apps_common_lr20xx_radio_get_last_cmd_status( const void* context )
{
    lr20xx_system_stat1_t    stat1;
    lr20xx_system_stat2_t    stat2;
    lr20xx_system_irq_mask_t irq_status;
    lr20xx_system_get_status( context, &stat1, &stat2, &irq_status );

    if( stat1.command_status >= LR20XX_SYSTEM_CMD_STATUS_OK )
    {
        return LR20XX_STATUS_OK;
    }

    return LR20XX_STATUS_ERROR;
}

void apps_common_lr20xx_read_length_from_header( void* context, uint16_t* length )
{
    if( is_header_implicit() == true )
    {
        *length = PAYLOAD_LENGTH;
        return;
    }

    uint32_t reg_32 = 0;
    lr20xx_regmem_read_regmem32( context, 0xF30800 + 0x74, &reg_32, 1 );
    apps_utilities_reverse_bits( &reg_32 );
    reg_32 >>= 16;
    *length = (uint16_t) reg_32;
}

void apps_common_lr20xx_radio_dbpsk_init( const void* context, const uint8_t payload_len )
{
    HAL_DBG_TRACE_INFO( "Sigfox parameters:\n" );
    HAL_DBG_TRACE_INFO( "   RF frequency  = %u Hz\n", SIGFOX_UPLINK_RF_FREQ_IN_HZ );
    HAL_DBG_TRACE_INFO( "   Output power  = %i dBm\n", SIGFOX_TX_OUTPUT_POWER_DBM_HALF_STEPS / 2 );
    packet_type = LR20XX_RADIO_COMMON_PKT_TYPE_BPSK;

    ASSERT_LR20XX_RC( lr20xx_radio_common_set_pkt_type( context, packet_type ) );

    uint32_t                               freq       = SIGFOX_UPLINK_RF_FREQ_IN_HZ;
    int8_t                                 tx_pwr     = SIGFOX_TX_OUTPUT_POWER_DBM_HALF_STEPS;

    const smtc_shield_lr20xx_pa_pwr_cfg_t* pa_pwr_cfg = smtc_shield_lr20xx_get_pa_pwr_cfg( freq, tx_pwr );

    if( pa_pwr_cfg == NULL )
    {
        HAL_DBG_TRACE_ERROR( "Invalid target frequency or power level\n" );
        while( true )
        {
        }
    }

    if( pa_pwr_cfg->pa_config.pa_sel == LR20XX_RADIO_COMMON_PA_SEL_HF )
    {
        if( tx_pwr > 24 )
        {
            tx_pwr = 24;
        }
    }
    else
    {
        if( tx_pwr > 44 )
        {
            tx_pwr = 44;
        }
    }

    if( pa_pwr_cfg == NULL )
    {
        while( true )
        {
        }
    }

    lr20xx_radio_common_set_rf_freq( context, freq );

    lr20xx_radio_common_rx_path_t rx_path = pa_pwr_cfg->pa_config.pa_sel == LR20XX_RADIO_COMMON_PA_SEL_HF
                                                ? LR20XX_RADIO_COMMON_RX_PATH_HF
                                                : LR20XX_RADIO_COMMON_RX_PATH_LF;
    lr20xx_radio_common_front_end_calibration_value_t calibration = { .frequency_in_hertz = freq, .rx_path = rx_path };
    lr20xx_radio_common_calibrate_front_end_helper( context, &calibration, 1 );

    lr20xx_radio_common_set_pa_cfg( context, &( pa_pwr_cfg->pa_config ) );
    lr20xx_radio_common_set_tx_params( context, tx_pwr, PA_RAMP_TIME );
    lr20xx_radio_common_set_rx_path( context, rx_path, ENABLE_RX_BOOST_MODE );

    ASSERT_LR20XX_RC( lr20xx_radio_common_set_rx_tx_fallback_mode( context, FALLBACK_MODE ) );
    ASSERT_LR20XX_RC( lr20xx_radio_bpsk_set_mod_params( context, &bpsk_mod_params ) );

    bpsk_pkt_params.payload_length = payload_len;

    ASSERT_LR20XX_RC( lr20xx_radio_bpsk_set_pkt_params( context, &bpsk_pkt_params ) );
}

extern int16_t g_rfRssi;
extern float   g_rfSnr;

void apps_common_lr20xx_receive( const void* context, uint8_t* buffer, uint16_t buffer_length, uint16_t* size )
{
    lr20xx_radio_common_get_rx_packet_length( context, size );

    if( *size > buffer_length )
    {
        HAL_DBG_TRACE_ERROR( "Received payload (size: %d) is bigger than the buffer (size: %d)!\n",
                             *size,
                             buffer_length );
        return;
    }
    lr20xx_radio_fifo_read_rx( context, buffer, *size );

#ifdef APPS_COMMON_DISPLAY_READ_PCK_AND_STATUS

    HAL_DBG_TRACE_ARRAY( "Packet content", buffer, *size );
    HAL_DBG_TRACE_INFO( "Packet status:\n" );

    switch( packet_type )
    {
        case LR20XX_RADIO_COMMON_PKT_TYPE_LORA:
        {
            lr20xx_radio_lora_packet_status_t pkt_status_lora = { 0 };
            lr20xx_radio_lora_get_packet_status( context, &pkt_status_lora );
            int16_t rssi_result = pkt_status_lora.rssi_pkt_in_dbm - ( pkt_status_lora.rssi_pkt_half_dbm_count * 0.5 );
            float   snr_db      = pkt_status_lora.snr_pkt_raw / 4.0f;
            HAL_DBG_TRACE_INFO( "  - SNR = %.2f dB \n", snr_db );
            HAL_DBG_TRACE_INFO( "  - RSSI packet = %d dBm\n", rssi_result );
            HAL_DBG_TRACE_INFO( "  - Signal RSSI packet = %d dBm\n", pkt_status_lora.rssi_signal_pkt_in_dbm );
            g_rfRssi = pkt_status_lora.rssi_signal_pkt_in_dbm;
            g_rfSnr  = snr_db;
            break;
        }
        case LR20XX_RADIO_COMMON_PKT_TYPE_FSK:
        {
            lr20xx_radio_fsk_packet_status_t pkt_status_fsk = { 0 };
            lr20xx_radio_fsk_get_packet_status( context, &pkt_status_fsk );
            int16_t rssi_result = pkt_status_fsk.rssi_avg_in_dbm - ( pkt_status_fsk.rssi_avg_half_dbm_count * 0.5 );
            HAL_DBG_TRACE_INFO( "  - RSSI average = %d dBm\n", rssi_result );
            HAL_DBG_TRACE_INFO( "  - RSSI sync = %d dBm\n", pkt_status_fsk.rssi_sync_in_dbm );
            break;
        }
        case LR20XX_RADIO_COMMON_PKT_TYPE_FLRC:
        {
            lr20xx_radio_flrc_pkt_status_t pkt_status_flrc = { 0 };
            lr20xx_radio_flrc_get_pkt_status( context, &pkt_status_flrc );
            int16_t rssi_result = pkt_status_flrc.rssi_avg_in_dbm - ( pkt_status_flrc.rssi_avg_half_dbm_count * 0.5 );
            HAL_DBG_TRACE_INFO( "  - RSSI average = %d dBm\n", rssi_result );
            g_rfRssi = rssi_result;
            break;
        }
        case LR20XX_RADIO_COMMON_PKT_TYPE_OOK:
        {
            lr20xx_radio_ook_packet_status_t pkt_status_ook = { 0 };
            lr20xx_radio_ook_get_packet_status( context, &pkt_status_ook );
            int16_t rssi_result = pkt_status_ook.rssi_avg_in_dbm - ( pkt_status_ook.rssi_avg_half_dbm_count * 0.5 );
            HAL_DBG_TRACE_INFO( "  - RSSI average = %d dBm\n", rssi_result );
            break;
        }
        default:
            break;
    }

#endif
}

void apps_common_lr20xx_irq_process( const void* context, lr20xx_system_irq_mask_t irq_filter_mask, bool log_enable )
{
    if( irq_fired == true )
    {
        irq_fired = false;

        lr20xx_system_irq_mask_t irq_regs;
        lr20xx_system_get_and_clear_irq_status( context, &irq_regs );

        if( log_enable )
        {
            HAL_DBG_TRACE_INFO( "Interrupt flags = 0x%08X\n", irq_regs );
        }

        irq_regs &= irq_filter_mask;

        if( log_enable )
        {
            HAL_DBG_TRACE_INFO( "Interrupt flags (after filtering) = 0x%08X\n", irq_regs );
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_TX_DONE ) == LR20XX_SYSTEM_IRQ_TX_DONE )
        {
            HAL_DBG_TRACE_INFO( "Tx done\n\n" );
            lr20xx_tx_done_irq_handler();
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_PREAMBLE_DETECTED ) == LR20XX_SYSTEM_IRQ_PREAMBLE_DETECTED )
        {
            HAL_DBG_TRACE_INFO( "Preamble detected\n\n" );
            lr20xx_preamble_detected_irq_handler();
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_LORA_HEADER_ERROR ) == LR20XX_SYSTEM_IRQ_LORA_HEADER_ERROR )
        {
            HAL_DBG_TRACE_ERROR( "LoRa header error\n\n" );
            lr20xx_lora_header_error_irq_handler();
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_FIFO_TX ) == LR20XX_SYSTEM_IRQ_FIFO_TX )
        {
            lr20xx_tx_fifo_threshold_irq_handler();
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_FIFO_RX ) == LR20XX_SYSTEM_IRQ_FIFO_RX )
        {
            lr20xx_rx_fifo_threshold_irq_handler();
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_SYNC_WORD_HEADER_VALID ) == LR20XX_SYSTEM_IRQ_SYNC_WORD_HEADER_VALID )
        {
            HAL_DBG_TRACE_INFO( "Syncword or header valid\n\n" );
            lr20xx_syncword_header_valid_irq_handler();
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_CMD_ERROR ) == LR20XX_SYSTEM_IRQ_CMD_ERROR )
        {
            HAL_DBG_TRACE_INFO( "Invalid command received by host\n\n" );
            lr20xx_host_cmd_error_irq_handler();
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_RX_DONE ) == LR20XX_SYSTEM_IRQ_RX_DONE )
        {
            if( ( irq_regs & LR20XX_SYSTEM_IRQ_CRC_ERROR ) == LR20XX_SYSTEM_IRQ_CRC_ERROR )
            {
                HAL_DBG_TRACE_ERROR( "CRC error\n\n" );
                lr20xx_crc_error_irq_handler();
            }
            else if( ( irq_regs & LR20XX_SYSTEM_IRQ_LEN_ERROR ) == LR20XX_SYSTEM_IRQ_LEN_ERROR )
            {
                HAL_DBG_TRACE_ERROR( "RX length error\n\n" );
                lr20xx_rx_len_error_irq_handler();
            }
            else
            {
                HAL_DBG_TRACE_INFO( "Rx done\n\n" );
                lr20xx_rx_done_irq_handler();
            }
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_CAD_DONE ) == LR20XX_SYSTEM_IRQ_CAD_DONE )
        {
            HAL_DBG_TRACE_INFO( "CAD done\n\n" );
            if( ( irq_regs & LR20XX_SYSTEM_IRQ_CAD_DETECTED ) == LR20XX_SYSTEM_IRQ_CAD_DETECTED )
            {
                HAL_DBG_TRACE_INFO( "Channel activity detected\n\n" );
                lr20xx_cad_done_detected_irq_handler();
            }
            else
            {
                HAL_DBG_TRACE_INFO( "No channel activity detected\n\n" );
                lr20xx_cad_done_undetected_irq_handler();
            }
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_TIMEOUT ) == LR20XX_SYSTEM_IRQ_TIMEOUT )
        {
            HAL_DBG_TRACE_INFO( "RX/TX timeout\n\n" );
            lr20xx_timeout_irq_handler();
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_RX_TIMESTAMP ) == LR20XX_SYSTEM_IRQ_RX_TIMESTAMP )
        {
            HAL_DBG_TRACE_INFO( "RX timestamp IRQ\n\n" );
            lr20xx_rx_timestamp_irq_handler();
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_RTTOF_RESPONDER_REQUEST_VALID ) ==
            LR20XX_SYSTEM_IRQ_RTTOF_RESPONDER_REQUEST_VALID )
        {
            HAL_DBG_TRACE_INFO( "RTToF request valid\n\n" );
            lr20xx_rttof_request_valid_irq_handler();
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_RTTOF_RESPONDER_REQUEST_DISCARDED ) ==
            LR20XX_SYSTEM_IRQ_RTTOF_RESPONDER_REQUEST_DISCARDED )
        {
            HAL_DBG_TRACE_WARNING( "RTToF request discarded\n\n" );
            lr20xx_rttof_request_discarded_irq_handler();
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_RTTOF_RESPONDER_RESPONSE_DONE ) ==
            LR20XX_SYSTEM_IRQ_RTTOF_RESPONDER_RESPONSE_DONE )
        {
            HAL_DBG_TRACE_INFO( "RTToF response done\n\n" );
            lr20xx_rttof_response_done_irq_handler();
        }

        if( ( irq_regs & LR20XX_SYSTEM_IRQ_RTTOF_INITIATOR_EXCHANGE_VALID ) ==
            LR20XX_SYSTEM_IRQ_RTTOF_INITIATOR_EXCHANGE_VALID )
        {
            HAL_DBG_TRACE_INFO( "RTToF exchange valid\n\n" );
            lr20xx_rttof_exchange_valid_irq_handler();
        }

        if( ( irq_regs & (uint32_t) LR20XX_SYSTEM_IRQ_RTTOF_INITIATOR_TIMEOUT ) ==
            (uint32_t) LR20XX_SYSTEM_IRQ_RTTOF_INITIATOR_TIMEOUT )
        {
            HAL_DBG_TRACE_WARNING( "RTToF request timeout\n\n" );
            lr20xx_rttof_timeout_irq_handler();
        }
    }
}

// void apps_common_lr20xx_handle_pre_tx( void )
// {
//     if( shield_pinout->led_tx != SMTC_SHIELD_PINOUT_NONE )
//     {
//         smtc_hal_mcu_gpio_set_state( led_tx.inst, SMTC_HAL_MCU_GPIO_STATE_HIGH );
//     }
// }

char* apps_common_dbm_half_step_to_string( int16_t dbm_half_step )
{
    bool half_step = false;
    memset( dbm_string_buf, 0x00, DBM_STRING_LEN );

    if( dbm_half_step % 2 != 0 )
    {
        half_step = true;
    }

    sprintf( dbm_string_buf, "%i.%s", dbm_half_step / 2, ( half_step == true ) ? "5" : "0" );

    return &dbm_string_buf[ 0 ];
}

// void apps_common_lr20xx_handle_post_tx( void )
// {
//     if( shield_pinout->led_tx != SMTC_SHIELD_PINOUT_NONE )
//     {
//         smtc_hal_mcu_gpio_set_state( led_tx.inst, SMTC_HAL_MCU_GPIO_STATE_LOW );
//     }
// }

// void apps_common_lr20xx_handle_pre_rx( void )
// {
//     if( shield_pinout->led_rx != SMTC_SHIELD_PINOUT_NONE )
//     {
//         smtc_hal_mcu_gpio_set_state( led_rx.inst, SMTC_HAL_MCU_GPIO_STATE_HIGH );
//     }
// }

// void apps_common_lr20xx_handle_post_rx( void )
// {
//     if( shield_pinout->led_rx != SMTC_SHIELD_PINOUT_NONE )
//     {
//         smtc_hal_mcu_gpio_set_state( led_rx.inst, SMTC_HAL_MCU_GPIO_STATE_LOW );
//     }
// }

// void apps_common_lr20xx_handle_pre_gnss_scan( void )
// {
//     if( shield_pinout->led_scan != SMTC_SHIELD_PINOUT_NONE )
//     {
//         smtc_hal_mcu_gpio_set_state( led_scan.inst, SMTC_HAL_MCU_GPIO_STATE_HIGH );
//     }

//     if( shield_pinout->lna != SMTC_SHIELD_PINOUT_NONE )
//     {
//         smtc_hal_mcu_gpio_set_state( lna_ctrl.inst, SMTC_HAL_MCU_GPIO_STATE_HIGH );
//     }
// }

// void apps_common_lr20xx_handle_post_gnss_scan( void )
// {
//     if( shield_pinout->led_scan != SMTC_SHIELD_PINOUT_NONE )
//     {
//         smtc_hal_mcu_gpio_set_state( led_scan.inst, SMTC_HAL_MCU_GPIO_STATE_LOW );
//     }

//     if( shield_pinout->lna != SMTC_SHIELD_PINOUT_NONE )
//     {
//         smtc_hal_mcu_gpio_set_state( lna_ctrl.inst, SMTC_HAL_MCU_GPIO_STATE_LOW );
//     }
// }

// void apps_common_lr20xx_handle_pre_wifi_scan( void )
// {
//     if( shield_pinout->led_scan != SMTC_SHIELD_PINOUT_NONE )
//     {
//         smtc_hal_mcu_gpio_set_state( led_scan.inst, SMTC_HAL_MCU_GPIO_STATE_HIGH );
//     }
// }

// void apps_common_lr20xx_handle_post_wifi_scan( void )
// {
//     if( shield_pinout->led_scan != SMTC_SHIELD_PINOUT_NONE )
//     {
//         smtc_hal_mcu_gpio_set_state( led_scan.inst, SMTC_HAL_MCU_GPIO_STATE_LOW );
//     }
// }

uint32_t get_time_on_air_in_ms( void )
{
    switch( packet_type )
    {
        case LR20XX_RADIO_COMMON_PKT_TYPE_LORA:
        {
            return lr20xx_radio_lora_get_time_on_air_in_ms( &lora_pkt_params, &lora_mod_params );
        }
        case LR20XX_RADIO_COMMON_PKT_TYPE_FSK:
        {
            return lr20xx_radio_fsk_get_time_on_air_in_ms( &fsk_pkt_params, &fsk_mod_params, fsk_syncword_length );
        }
        default:
        {
            return 0;
        }
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

static void print_common_configuration( void )
{
    HAL_DBG_TRACE_INFO( "Common parameters:\n" );
    HAL_DBG_TRACE_INFO( "   Packet type   = %s\n", lr20xx_radio_common_pkt_type_to_str( packet_type ) );
    HAL_DBG_TRACE_INFO( "   RF frequency  = %u Hz\n", RF_FREQ_IN_HZ );
    HAL_DBG_TRACE_INFO( "   Output power  = %s dBm\n",
                        apps_common_dbm_half_step_to_string( TX_OUTPUT_POWER_HALF_DBM_STEPS ) );
    HAL_DBG_TRACE_INFO( "   Fallback mode = %s\n", lr20xx_radio_common_fallback_modes_to_str( FALLBACK_MODE ) );
    if (ENABLE_RX_BOOST_MODE) {
        HAL_DBG_TRACE_INFO(" Rx boost activated\n");
    } else {
        HAL_DBG_TRACE_INFO(" Rx boost deactivated\n");
    }    
    HAL_DBG_TRACE_PRINTF( "\n" );
}

static void print_lora_configuration( void )
{
    HAL_DBG_TRACE_INFO( "LoRa modulation parameters:\n" );
    HAL_DBG_TRACE_INFO( "   Spreading factor = %s\n", lr20xx_radio_lora_sf_to_str( LORA_SPREADING_FACTOR ) );
    HAL_DBG_TRACE_INFO( "   Bandwidth        = %s kHz\n", lr20xx_radio_lora_bw_to_str( LORA_BANDWIDTH ) );
    HAL_DBG_TRACE_INFO( "   Coding rate      = %s\n", lr20xx_radio_lora_cr_to_str( LORA_CODING_RATE ) );
    HAL_DBG_TRACE_PRINTF( "\n" );

    HAL_DBG_TRACE_INFO( "LoRa packet parameters:\n" );
    HAL_DBG_TRACE_INFO( "   Preamble length = %d symbol(s)\n", LORA_PREAMBLE_LENGTH );
    HAL_DBG_TRACE_INFO( "   Header mode     = %s\n", lr20xx_radio_lora_pkt_mode_to_str( LORA_PKT_LEN_MODE ) );
    HAL_DBG_TRACE_INFO( "   Payload length  = %d byte(s)\n", PAYLOAD_LENGTH );
    HAL_DBG_TRACE_INFO( "   CRC mode        = %s\n", lr20xx_radio_lora_crc_to_str( LORA_CRC ) );
    HAL_DBG_TRACE_INFO( "   IQ              = %s\n", lr20xx_radio_lora_iq_to_str( LORA_IQ ) );
    HAL_DBG_TRACE_PRINTF( "\n" );

    HAL_DBG_TRACE_INFO( "LoRa syncword = 0x%02X\n", LORA_SYNCWORD );
    HAL_DBG_TRACE_PRINTF( "\n" );
}

static bool is_header_implicit( void )
{
    return ( ( packet_type == LR20XX_RADIO_COMMON_PKT_TYPE_LORA ) &&
             ( LORA_PKT_LEN_MODE == LR20XX_RADIO_LORA_PKT_IMPLICIT ) ) ||
           ( ( packet_type == LR20XX_RADIO_COMMON_PKT_TYPE_FSK ) &&
             ( FSK_HEADER_TYPE == LR20XX_RADIO_FSK_HEADER_IMPLICIT ) ) ||
           ( ( packet_type == LR20XX_RADIO_COMMON_PKT_TYPE_FLRC ) &&
             ( FLRC_PKT_LEN_MODE == LR20XX_RADIO_FLRC_PKT_FIX_LEN ) ) ||
           ( ( packet_type == LR20XX_RADIO_COMMON_PKT_TYPE_OOK ) &&
             ( OOK_HEADER_MODE == LR20XX_RADIO_OOK_HEADER_IMPLICIT ) );
}

static void print_fsk_configuration( void )
{
    HAL_DBG_TRACE_INFO( "FSK modulation parameters:\n" );
    HAL_DBG_TRACE_INFO( "   Bitrate             = %u bps\n", fsk_mod_params.br );
    HAL_DBG_TRACE_INFO( "   Pulse shape         = %s\n",
                        lr20xx_radio_fsk_pulse_shape_to_str( fsk_mod_params.pulse_shape ) );
    HAL_DBG_TRACE_INFO( "   Bandwidth           = %s\n", lr20xx_radio_fsk_common_bw_to_str( fsk_mod_params.bw ) );
    HAL_DBG_TRACE_INFO( "   Frequency deviation = %u Hz\n", fsk_mod_params.fdev_in_hz );
    HAL_DBG_TRACE_PRINTF( "\n" );

    HAL_DBG_TRACE_INFO( "GFSK packet parameters:\n" );
    HAL_DBG_TRACE_INFO( "   Preamble length   = %u bit(s)\n", fsk_pkt_params.pbl_length_in_bit );
    HAL_DBG_TRACE_INFO( "   Preamble detector = %s\n",
                        lr20xx_radio_fsk_preamble_detector_to_str( fsk_pkt_params.preamble_detector ) );
    HAL_DBG_TRACE_INFO( "   Syncword length   = %u bit(s)\n", fsk_syncword_length );
    HAL_DBG_TRACE_INFO( "   Address filtering = %s\n",
                        lr20xx_radio_fsk_address_filtering_to_str( FSK_ADDRESS_FILTERING ) );
    if( FSK_ADDRESS_FILTERING != LR20XX_RADIO_FSK_ADDRESS_FILTERING_DISABLED )
    {
        HAL_DBG_TRACE_INFO( "     (Node address      = 0x%02X)\n", FSK_NODE_ADDRESS );
        if( FSK_ADDRESS_FILTERING == LR20XX_RADIO_FSK_ADDRESS_FILTERING_NODE_BROADCAST )
        {
            HAL_DBG_TRACE_INFO( "     (Broadcast address = 0x%02X)\n", FSK_BROADCAST_ADDRESS );
        }
    }
    HAL_DBG_TRACE_INFO( "   Header mode       = %s\n", lr20xx_radio_fsk_header_mode_to_str( FSK_HEADER_TYPE ) );
    HAL_DBG_TRACE_INFO( "   Payload length    = %d byte(s)\n", PAYLOAD_LENGTH );
    HAL_DBG_TRACE_INFO( "   CRC mode          = %s\n", lr20xx_radio_fsk_crc_to_str( FSK_CRC_TYPE ) );
    if( FSK_CRC_TYPE != LR20XX_RADIO_FSK_CRC_OFF )
    {
        HAL_DBG_TRACE_INFO( "     (CRC seed       = 0x%08X)\n", FSK_CRC_SEED );
        HAL_DBG_TRACE_INFO( "     (CRC polynomial = 0x%08X)\n", FSK_CRC_POLYNOMIAL );
    }
    HAL_DBG_TRACE_INFO( "   Whitening           = %s\n", lr20xx_radio_fsk_whitening_to_str( FSK_DC_FREE ) );
    if( FSK_DC_FREE != LR20XX_RADIO_FSK_WHITENING_OFF )
    {
        HAL_DBG_TRACE_INFO( "     (Whitening seed = 0x%04X)\n", FSK_WHITENING_SEED );
    }
    HAL_DBG_TRACE_PRINTF( "\n" );
}

static void print_ook_configuration( void )
{
    HAL_DBG_TRACE_INFO( "OOK modulation parameters:\n" );
    HAL_DBG_TRACE_INFO( "   Bitrate             = %u bps\n", ook_mod_params.br );
    HAL_DBG_TRACE_INFO( "   Pulse shape BT         = %s\n",
                        lr20xx_radio_ook_pulse_shape_bt_to_str( ook_mod_params.pulse_shape.bt ) );
    HAL_DBG_TRACE_INFO( "   Pulse shape filter type        = %s\n",
                        lr20xx_radio_ook_pulse_shape_filter_to_str( ook_mod_params.pulse_shape.filter ) );
    HAL_DBG_TRACE_INFO( "   Bandwidth           = %s\n", lr20xx_radio_fsk_common_bw_to_str( ook_mod_params.bw ) );

    HAL_DBG_TRACE_INFO( "OOK packet parameters:\n" );
    HAL_DBG_TRACE_INFO( "   Preamble length   = %u bit(s)\n", ook_pck_params.pbl_length_in_bit );
    HAL_DBG_TRACE_INFO( "   Address filtering = %s\n",
                        lr20xx_radio_ook_address_filtering_to_str( OOK_ADDRESS_FILTERING ) );
    if( OOK_ADDRESS_FILTERING != LR20XX_RADIO_OOK_ADDRESS_FILTERING_DISABLED )
    {
        HAL_DBG_TRACE_INFO( "     (Node address      = 0x%02X)\n", OOK_NODE_ADDRESS );
        if( OOK_ADDRESS_FILTERING == LR20XX_RADIO_OOK_ADDRESS_FILTERING_NODE_BROADCAST )
        {
            HAL_DBG_TRACE_INFO( "     (Broadcast address = 0x%02X)\n", OOK_BROADCAST_ADDRESS );
        }
    }
    HAL_DBG_TRACE_INFO( "   Header mode       = %s\n", lr20xx_radio_ook_header_mode_to_str( OOK_HEADER_MODE ) );
    HAL_DBG_TRACE_INFO( "   Payload length    = %d byte(s)\n", PAYLOAD_LENGTH );
    HAL_DBG_TRACE_INFO( "   CRC mode          = %s\n", lr20xx_radio_ook_crc_to_str( OOK_CRC ) );
    HAL_DBG_TRACE_INFO( "   Encoding          = %s\n", lr20xx_radio_ook_encoding_to_str( ook_pck_params.encoding ) );
    if( OOK_WHITENING_POLYNOMIAL != 0x00 )
    {
        HAL_DBG_TRACE_INFO( "   Whitening polynomial       = %u\n", (uint16_t) OOK_WHITENING_POLYNOMIAL );
        HAL_DBG_TRACE_INFO( "   Whitening seed             = %u\n", (uint16_t) OOK_WHITENING_SEED );
    }
    if( OOK_CRC != LR20XX_RADIO_OOK_CRC_OFF )
    {
        HAL_DBG_TRACE_INFO( "     (CRC seed       = 0x%08X)\n", OOK_CRC_SEED );
        HAL_DBG_TRACE_INFO( "     (CRC polynomial = 0x%08X)\n", OOK_CRC_POLYNOMIAL );
    }

    HAL_DBG_TRACE_PRINTF( "\n" );
}

static void print_flrc_configuration( void )
{
    HAL_DBG_TRACE_INFO( "FLRC modulation parameters:\n" );
    HAL_DBG_TRACE_INFO( "   Bitrate and bandwidth  = %s\n", lr20xx_radio_flrc_br_bw_to_str( flrc_mod_params.br_bw ) );
    HAL_DBG_TRACE_INFO( "   Pulse shape            = %s\n",
                        lr20xx_radio_flrc_pulse_shape_to_str( flrc_mod_params.shape ) );
    HAL_DBG_TRACE_INFO( "   Coding rate            = %s\n", lr20xx_radio_flrc_cr_to_str( flrc_mod_params.cr ) );
    HAL_DBG_TRACE_PRINTF( "\n" );

    HAL_DBG_TRACE_INFO( "FLRC packet parameters:\n" );
    HAL_DBG_TRACE_INFO( "   Preamble length        = %s\n",
                        lr20xx_radio_flrc_preamble_len_to_str( flrc_pkt_params.preamble_len ) );
    HAL_DBG_TRACE_INFO( "   Syncword length        = %s\n",
                        lr20xx_radio_flrc_sync_word_len_to_str( flrc_pkt_params.sync_word_len ) );
    HAL_DBG_TRACE_INFO( "   Syncword RX match type = %s\n",
                        lr20xx_radio_flrc_rx_match_sync_word_to_str( flrc_pkt_params.match_sync_word ) );
    HAL_DBG_TRACE_INFO( "   Header type            = %s\n",
                        lr20xx_radio_flrc_pkt_len_modes_to_str( flrc_pkt_params.header_type ) );
    HAL_DBG_TRACE_INFO( "   Payload length         = %u\n", (uint16_t) flrc_pkt_params.pld_len_in_bytes );
    HAL_DBG_TRACE_INFO( "   CRC type               = %s\n\n",
                        lr20xx_radio_flrc_crc_types_to_str( flrc_pkt_params.crc_type ) );
}

void print_driver_version( void )
{
    HAL_DBG_TRACE_INFO( "LR20XX driver version: %s\n", lr20xx_driver_version_get_version_string() );
}

void lr20xx_set_mesh_dio_irq_cb(lr20xx_mesh_dio_cb_t cb)
{
    s_mesh_dio_cb = cb;
}

void radio_on_dio_irq(void* context) {
    irq_fired = true;
    if( s_mesh_dio_cb )
    {
        s_mesh_dio_cb();
    }
}

/* --- EOF ------------------------------------------------------------------ */
