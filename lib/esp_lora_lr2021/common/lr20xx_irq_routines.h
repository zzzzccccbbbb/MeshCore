/*!
 * @file      lr20xx_irq_routines.h
 *
 * @brief     Functions to handle lr20xx interrupts
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
#ifndef LR20XX_IRQ_ROUTINES_H
    #define LR20XX_IRQ_ROUTINES_H

    #ifdef __cplusplus
extern "C"
{
    #endif

    /*
     * -----------------------------------------------------------------------------
     * --- DEPENDENCIES ------------------------------------------------------------
     */

    /*
     * -----------------------------------------------------------------------------
     * --- PUBLIC MACROS -----------------------------------------------------------
     */

    /*
     * -----------------------------------------------------------------------------
     * --- PUBLIC CONSTANTS --------------------------------------------------------
     */

    /*
     * -----------------------------------------------------------------------------
     * --- PUBLIC TYPES ------------------------------------------------------------
     */

    /*
     * -----------------------------------------------------------------------------
     * --- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------
     */

    /*!
     * @brief IRQ routine to handle the tx done interrupt
     */
    void lr20xx_tx_done_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the rx done interrupt
     */
    void lr20xx_rx_done_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the timeout interrupt
     */
    void lr20xx_timeout_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the rx fifo threshold interrupt
     */
    void lr20xx_rx_fifo_threshold_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the tx fifo threshold interrupt
     */
    void lr20xx_tx_fifo_threshold_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the preamble detected interrupt
     */
    void lr20xx_preamble_detected_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the syncword or header valid interrupt
     */
    void lr20xx_syncword_header_valid_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the lora header interrupt
     */
    void lr20xx_lora_header_error_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the rx length interrupt
     */
    void lr20xx_rx_len_error_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the crc interrupt
     */
    void lr20xx_crc_error_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the CAD non-detected interrupt
     */
    void lr20xx_cad_done_undetected_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the CAD detected interrupt
     */
    void lr20xx_cad_done_detected_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the rx timestamp interrupt
     */
    void lr20xx_rx_timestamp_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the rttof valid request interrupt
     */
    void lr20xx_rttof_request_valid_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the rttof discarded request interrupt
     */
    void lr20xx_rttof_request_discarded_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the rttof response completed interrupt
     */
    void lr20xx_rttof_response_done_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the rttof valid exchange interrupt
     */
    void lr20xx_rttof_exchange_valid_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the rttof timeout interrupt
     */
    void lr20xx_rttof_timeout_irq_handler( void );

    /*!
     * @brief IRQ routine to handle the host command error interrupt
     */
    void lr20xx_host_cmd_error_irq_handler( void );

    #ifdef __cplusplus
}
    #endif

#endif    // LR20XX_IRQ_ROUTINES_H

/* --- EOF ------------------------------------------------------------------ */
