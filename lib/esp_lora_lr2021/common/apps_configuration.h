/*!
 * @file      apps_configuration.h
 *
 * @brief     Common configuration
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

#ifndef APPS_CONFIGURATION_H
    #define APPS_CONFIGURATION_H

    #ifdef __cplusplus
extern "C"
{
    #endif

    /*
     * -----------------------------------------------------------------------------
     * --- DEPENDENCIES ------------------------------------------------------------
     */

    #include <stdint.h>
    #include "lr20xx_radio_ook_types.h"
    #include "lr20xx_radio_lora_types.h"
    #include "lr20xx_radio_flrc_types.h"
    #include "lr20xx_radio_fsk_types.h"
    #include "lr20xx_radio_common.h"

    /*
     * -----------------------------------------------------------------------------
     * --- PUBLIC MACROS -----------------------------------------------------------
     */

    /*!
     * @brief General parameters
     */
    #ifndef PACKET_TYPE
        #define PACKET_TYPE LR20XX_RADIO_COMMON_PKT_TYPE_LORA
    #endif
    #ifndef RF_FREQ_IN_HZ
        #define RF_FREQ_IN_HZ 915230000U
        // #define RF_FREQ_IN_HZ 2475810000U
    #endif
    #ifndef TX_OUTPUT_POWER_HALF_DBM_STEPS
        #define TX_OUTPUT_POWER_HALF_DBM_STEPS 44 /* in 0.5dBm steps, range [-19, +44] for sub-G, range [-38, 24] for 2.4G ( HF_PA ) */
        // #define TX_OUTPUT_POWER_HALF_DBM_STEPS 24
    #endif

    #ifndef PA_RAMP_TIME
        #define PA_RAMP_TIME LR20XX_RADIO_COMMON_RAMP_48_US
    #endif
    #ifndef FALLBACK_MODE
        #define FALLBACK_MODE LR20XX_RADIO_FALLBACK_STDBY_XOSC
    #endif
    #ifndef ENABLE_RX_BOOST_MODE
        #define ENABLE_RX_BOOST_MODE true
    #endif
    #ifndef PAYLOAD_LENGTH
        #define PAYLOAD_LENGTH 255
    #endif

    /*!
     * @brief Modulation parameters for LoRa packets
     */
    #ifndef LORA_SPREADING_FACTOR
        #define LORA_SPREADING_FACTOR LR20XX_RADIO_LORA_SF12
    #endif
    #ifndef LORA_BANDWIDTH
        #define LORA_BANDWIDTH LR20XX_RADIO_LORA_BW_125
    #endif
    #ifndef LORA_CODING_RATE
        #define LORA_CODING_RATE LR20XX_RADIO_LORA_CR_4_5
    #endif

    /*!
     * @brief Packet parameters for LoRa packets
     */
    #ifndef LORA_PREAMBLE_LENGTH
        #define LORA_PREAMBLE_LENGTH 8
    #endif
    #ifndef LORA_PKT_LEN_MODE
        #define LORA_PKT_LEN_MODE LR20XX_RADIO_LORA_PKT_EXPLICIT
    #endif
    #ifndef LORA_IQ
        #define LORA_IQ LR20XX_RADIO_LORA_IQ_STANDARD
    #endif
    #ifndef LORA_CRC
        #define LORA_CRC LR20XX_RADIO_LORA_CRC_ENABLED
    #endif

    /*!
     * @brief LoRa sync word
     */
    #ifndef LORA_SYNCWORD
        #define LORA_SYNCWORD 0x12    // 0x12 Private Network, 0x34 Public Network
    #endif

    /*!
     * @brief Modulation parameters for FSK packets
     */
    #ifndef FSK_FDEV
        #define FSK_FDEV 25000U    // Hz
    #endif
    #ifndef FSK_BITRATE
        #define FSK_BITRATE 50000U    // bps
    #endif
    #ifndef FSK_BANDWIDTH
        #define FSK_BANDWIDTH                                                                                          \
            LR20XX_RADIO_FSK_COMMON_RX_BW_111_000_HZ    // Make sure to follow the rule: (2 * FDEV + BITRATE) < BW
    #endif
    #ifndef FSK_PULSE_SHAPE
        #define FSK_PULSE_SHAPE LR20XX_RADIO_FSK_PULSE_SHAPE_DISABLED
    #endif

    /*!
     * @brief Packet parameters for FSK packets
     */
    #ifndef FSK_PREAMBLE_LENGTH
        #define FSK_PREAMBLE_LENGTH 32    // bits
    #endif
    #ifndef FSK_PREAMBLE_DETECTOR
        #define FSK_PREAMBLE_DETECTOR LR20XX_RADIO_FSK_PREAMBLE_DETECTOR_DISABLED
    #endif
    #ifndef FSK_SYNCWORD_LENGTH
        #define FSK_SYNCWORD_LENGTH 40    // bits
    #endif
    #ifndef FSK_ADDRESS_FILTERING
        #define FSK_ADDRESS_FILTERING LR20XX_RADIO_FSK_ADDRESS_FILTERING_DISABLED
    #endif
    #ifndef FSK_HEADER_TYPE
        #define FSK_HEADER_TYPE LR20XX_RADIO_FSK_HEADER_16BITS
    #endif
    #ifndef FSK_CRC_TYPE
        #define FSK_CRC_TYPE LR20XX_RADIO_FSK_CRC_1_BYTE_INVERTED
    #endif
    #ifndef FSK_DC_FREE
        #define FSK_DC_FREE LR20XX_RADIO_FSK_WHITENING_OFF
    #endif

    /*!
     * @brief FSK sync word
     */
    static const uint8_t gfsk_sync_word[ 8 ] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };

    /*!
     * @brief FSK whitening seed
     */
    #ifndef FSK_WHITENING_SEED
        #define FSK_WHITENING_SEED 0x0123
    #endif

    /*!
     * @brief FSK CRC seed
     */
    #ifndef FSK_CRC_SEED
        #define FSK_CRC_SEED 0x01234567
    #endif

    /*!
     * @brief FSK CRC polynomial
     */
    #ifndef FSK_CRC_POLYNOMIAL
        #define FSK_CRC_POLYNOMIAL 0x01234567
    #endif

    /*!
     * @brief FSK address filtering - node address
     */
    #ifndef FSK_NODE_ADDRESS
        #define FSK_NODE_ADDRESS 0x05
    #endif

    /*!
     * @brief FSK address filtering - broadcast address
     */
    #ifndef FSK_BROADCAST_ADDRESS
        #define FSK_BROADCAST_ADDRESS 0xAB
    #endif

    /*!
     * @brief FSK bandwidth for rssi instantaneous measurement
     */
    #ifndef FSK_RSSI_INST_BW
        #define FSK_RSSI_INST_BW LR20XX_RADIO_FSK_COMMON_RX_BW_444_000_HZ
    #endif

    /*!
     * @brief Modulation parameters for FLRC packets
     */
    #ifndef FLRC_BANDWIDTH
        #define FLRC_BANDWIDTH LR20XX_RADIO_FLRC_BR_2_600_BW_2_666
    #endif

    #ifndef FLRC_CODING_RATE
        #define FLRC_CODING_RATE LR20XX_RADIO_FLRC_CR_3_4
    #endif

    #ifndef FLRC_PULSE_SHAPE
        #define FLRC_PULSE_SHAPE LR20XX_RADIO_FLRC_PULSE_SHAPE_BT_05
    #endif

    /*!
     * @brief Packet parameters for FLRC packets
     */
    #ifndef FLRC_PREAMBLE_LEN
        #define FLRC_PREAMBLE_LEN LR20XX_RADIO_FLRC_PREAMBLE_LEN_16_BITS
    #endif

    #ifndef FLRC_SYNCWORD_LEN
        #define FLRC_SYNCWORD_LEN LR20XX_RADIO_FLRC_SYNCWORD_LENGTH_4_BYTES
    #endif

    #ifndef FLRC_TX_SYNCWORD
        #define FLRC_TX_SYNCWORD LR20XX_RADIO_FLRC_TX_SYNCWORD_1
    #endif

    #ifndef FLRC_RX_SYNCWORD_MATCH
        #define FLRC_RX_SYNCWORD_MATCH LR20XX_RADIO_FLRC_RX_MATCH_SYNCWORD_1
    #endif

    #ifndef FLRC_PKT_LEN_MODE
        #define FLRC_PKT_LEN_MODE LR20XX_RADIO_FLRC_PKT_VAR_LEN
    #endif

    #ifndef FLRC_CRC_TYPE
        #define FLRC_CRC_TYPE LR20XX_RADIO_FLRC_CRC_2_BYTES
    #endif

    /*!
     * @brief FLRC sync word
     */
    #ifndef FLRC_USE_DEFAULT_SYNC_WORD
        #define FLRC_USE_DEFAULT_SYNC_WORD false
    #endif
    static const uint8_t flrc_sync_word[ LR20XX_RADIO_FLRC_SYNCWORD_LENGTH ] = { 0x48, 0x33, 0xFC, 0x71 };

    /*!
     * @brief Packet parameters for LR-FHSS packets
     */
    #ifndef LR_FHSS_BANDWIDTH
        #define LR_FHSS_BANDWIDTH LR_FHSS_V1_BW_136719_HZ
    #endif

    #ifndef LR_FHSS_CODING_RATE
        #define LR_FHSS_CODING_RATE LR_FHSS_V1_CR_5_6
    #endif

    #ifndef LR_FHSS_ENABLE_HOPPING
        #define LR_FHSS_ENABLE_HOPPING true
    #endif

    #ifndef LR_FHSS_GRID
        #define LR_FHSS_GRID LR_FHSS_V1_GRID_3906_HZ
    #endif

    #ifndef LR_FHSS_HEADER_COUNT
        #define LR_FHSS_HEADER_COUNT 2
    #endif

    #ifndef LR_FHSS_MODULATION_TYPE
        #define LR_FHSS_MODULATION_TYPE LR_FHSS_V1_MODULATION_TYPE_GMSK_488
    #endif

    /*!
     * @brief Modulation parameters for OOK packets
     */
    #ifndef OOK_BITRATE
        #define OOK_BITRATE 32000U    // bps
    #endif

    #ifndef OOK_BANDWIDTH
        #define OOK_BANDWIDTH LR20XX_RADIO_FSK_COMMON_RX_BW_153_000_HZ    // Suggested: BW not greater than (5* BITRATE)
    #endif

    #ifndef OOK_PULSE_SHAPE
        #define OOK_PULSE_SHAPE LR20XX_RADIO_OOK_PULSE_SHAPE_BT_0_5
    #endif

    #ifndef OOK_MAG_DEPTH
        #define OOK_MAG_DEPTH LR20XX_RADIO_OOK_MAG_DEPTH_FULL
    #endif

    #ifndef OOK_PULSE_SHAPE_FILTER
        #define OOK_PULSE_SHAPE_FILTER LR20XX_RADIO_OOK_PULSE_SHAPE_NO_FILTER
    #endif

    /*!
     * @brief Packet parameters for OOK packets
     */
    #ifndef OOK_PREAMBLE_LENGTH_BITS
        #define OOK_PREAMBLE_LENGTH_BITS 32U
    #endif

    #ifndef OOK_ADDRESS_FILTERING
        #define OOK_ADDRESS_FILTERING LR20XX_RADIO_OOK_ADDRESS_FILTERING_DISABLED
    #endif

    #ifndef OOK_HEADER_MODE
        #define OOK_HEADER_MODE LR20XX_RADIO_OOK_HEADER_EXPLICIT
    #endif

    #ifndef OOK_CRC
        #define OOK_CRC LR20XX_RADIO_OOK_CRC_OFF
    #endif

    #ifndef OOK_ENCODING
        #define OOK_ENCODING LR20XX_RADIO_OOK_ENCODING_OFF
    #endif

    /*!
     * @brief OOK crc polynomial
     */
    #ifndef OOK_CRC_POLYNOMIAL
        #define OOK_CRC_POLYNOMIAL 0x01234567
    #endif

    /*!
     * @brief OOK crc polynomial
     */
    #ifndef OOK_CRC_SEED
        #define OOK_CRC_SEED 0x01234567
    #endif

    /*!
     * @brief OOK address filtering - node address
     */
    #ifndef OOK_NODE_ADDRESS
        #define OOK_NODE_ADDRESS 0x05
    #endif

    /*!
     * @brief OOK address filtering - broadcast address
     */
    #ifndef OOK_BROADCAST_ADDRESS
        #define OOK_BROADCAST_ADDRESS 0xAB
    #endif

    /*!
     * @brief OOK whitening polynomial
     */
    #ifndef OOK_WHITENING_POLYNOMIAL
        #define OOK_WHITENING_POLYNOMIAL 0    // 0 to disable whitening
    #endif

    /*!
     * @brief OOK whitening seed
     */
    #ifndef OOK_WHITENING_SEED
        #define OOK_WHITENING_SEED 0x0123
    #endif

    /*!
     * @brief OOK sync word
     */
    static const uint8_t ook_sync_word[ LR20XX_RADIO_OOK_SYNCWORD_LENGTH ] = { 0x7f, 0x53, 0x65, 0x64 };

    /*!
     * @brief Sigfox radio configuration
     */
    #ifndef SIGFOX_RC
        #define SIGFOX_RC 1
    #endif

    #if ( SIGFOX_RC == 1 )
        #define SIGFOX_UPLINK_RF_FREQ_IN_HZ           868130000
        #define BPSK_BITRATE_IN_BPS                   100
        #define SIGFOX_TX_OUTPUT_POWER_DBM_HALF_STEPS 28
        #define RAMP_UP_DELAY                         SIGFOX_DBPSK_RAMP_UP_TIME_100_BPS
        #define RAMP_DOWN_DELAY                       SIGFOX_DBPSK_RAMP_DOWN_TIME_100_BPS
    #elif ( SIGFOX_RC == 2 )
        #define SIGFOX_UPLINK_RF_FREQ_IN_HZ           902200000
        #define BPSK_BITRATE_IN_BPS                   600
        #define SIGFOX_TX_OUTPUT_POWER_DBM_HALF_STEPS 44
        #define RAMP_UP_DELAY                         SIGFOX_DBPSK_RAMP_UP_TIME_600_BPS
        #define RAMP_DOWN_DELAY                       SIGFOX_DBPSK_RAMP_DOWN_TIME_600_BPS
    #elif ( SIGFOX_RC == 3 )
        #define SIGFOX_UPLINK_RF_FREQ_IN_HZ           923200000
        #define BPSK_BITRATE_IN_BPS                   100
        #define SIGFOX_TX_OUTPUT_POWER_DBM_HALF_STEPS 28
        #define RAMP_UP_DELAY                         SIGFOX_DBPSK_RAMP_DOWN_TIME_100_BPS
        #define RAMP_DOWN_DELAY                       SIGFOX_DBPSK_RAMP_DOWN_TIME_100_BPS
    #elif ( SIGFOX_RC == 4 )
        #define SIGFOX_UPLINK_RF_FREQ_IN_HZ           920800000
        #define BPSK_BITRATE_IN_BPS                   600
        #define SIGFOX_TX_OUTPUT_POWER_DBM_HALF_STEPS 44
        #define RAMP_UP_DELAY                         SIGFOX_DBPSK_RAMP_UP_TIME_600_BPS
        #define RAMP_DOWN_DELAY                       SIGFOX_DBPSK_RAMP_DOWN_TIME_600_BPS
    #elif ( SIGFOX_RC == 5 )
        #define SIGFOX_UPLINK_RF_FREQ_IN_HZ           923300000
        #define BPSK_BITRATE_IN_BPS                   100
        #define SIGFOX_TX_OUTPUT_POWER_DBM_HALF_STEPS 24
        #define RAMP_UP_DELAY                         SIGFOX_DBPSK_RAMP_DOWN_TIME_100_BPS
        #define RAMP_DOWN_DELAY                       SIGFOX_DBPSK_RAMP_DOWN_TIME_100_BPS
    #elif ( SIGFOX_RC == 6 )
        #define SIGFOX_UPLINK_RF_FREQ_IN_HZ           865200000
        #define BPSK_BITRATE_IN_BPS                   100
        #define SIGFOX_TX_OUTPUT_POWER_DBM_HALF_STEPS 28
        #define RAMP_UP_DELAY                         SIGFOX_DBPSK_RAMP_DOWN_TIME_100_BPS
        #define RAMP_DOWN_DELAY                       SIGFOX_DBPSK_RAMP_DOWN_TIME_100_BPS
    #elif ( SIGFOX_RC == 7 )
        #define SIGFOX_UPLINK_RF_FREQ_IN_HZ           868800000
        #define BPSK_BITRATE_IN_BPS                   100
        #define SIGFOX_TX_OUTPUT_POWER_DBM_HALF_STEPS 28
        #define RAMP_UP_DELAY                         SIGFOX_DBPSK_RAMP_DOWN_TIME_100_BPS
        #define RAMP_DOWN_DELAY                       SIGFOX_DBPSK_RAMP_DOWN_TIME_100_BPS
    #else
        #error "Select a valid Radio Configuration"
    #endif

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

    #ifdef __cplusplus
}
    #endif

#endif    // APPS_CONFIGURATION_H

/* --- EOF ------------------------------------------------------------------ */
