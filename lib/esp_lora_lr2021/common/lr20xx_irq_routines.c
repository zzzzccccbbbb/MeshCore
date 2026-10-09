/*!
 * @file      lr20xx_irq_routines.c
 *
 * @brief     Functions (weak) to handle lr20xx interrupts
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

#include "lr20xx_irq_routines.h"
// #include "smtc_hal_dbg_trace.h"
#include "esp_log.h"

static const char* IRQ_TAG = "LR20XX_IRQ";

// 定义缺失的宏
#define HAL_DBG_TRACE_INFO(...)    ESP_LOGI(IRQ_TAG, __VA_ARGS__)
/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

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

/*!
 * @brief Print a message when a irq routine is not defined
 *
 * @param [in] lr20xx_irq_handler_name  String containing the lr20xx irq name
 */
static void print_undefined_lr20xx_irq_routine( const char* lr20xx_irq_handler_name );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

__attribute__( ( weak ) ) void lr20xx_tx_done_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_rx_done_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_timeout_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_preamble_detected_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_syncword_header_valid_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_lora_header_error_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_rx_len_error_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_crc_error_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_cad_done_undetected_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_cad_done_detected_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_rx_timestamp_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_rx_fifo_threshold_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_tx_fifo_threshold_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_rttof_request_valid_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_rttof_request_discarded_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_rttof_response_done_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_rttof_exchange_valid_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_rttof_timeout_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

__attribute__( ( weak ) ) void lr20xx_host_cmd_error_irq_handler( void )
{
    print_undefined_lr20xx_irq_routine( __func__ );
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

static void print_undefined_lr20xx_irq_routine( const char* lr20xx_irq_handler_name )
{
    HAL_DBG_TRACE_INFO( "No LR20xx IRQ routine defined for function " );
    // HAL_DBG_TRACE_PRINTF( lr20xx_irq_handler_name );
    // HAL_DBG_TRACE_PRINTF( "\n" );
}

/* --- EOF ------------------------------------------------------------------ */