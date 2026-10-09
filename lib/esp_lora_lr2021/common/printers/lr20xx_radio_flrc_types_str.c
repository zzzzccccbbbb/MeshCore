#include "lr20xx_radio_flrc_types_str.h"

const char* lr20xx_radio_flrc_br_bw_to_str( const lr20xx_radio_flrc_br_bw_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_FLRC_BR_2_600_BW_2_666:
        {
            return (const char*) "2_600_BW_2_666";
        }

        case LR20XX_RADIO_FLRC_BR_2_080_BW_2_222:
        {
            return (const char*) "2_080_BW_2_222";
        }

        case LR20XX_RADIO_FLRC_BR_1_300_BW_1_333:
        {
            return (const char*) "1_300_BW_1_333";
        }

        case LR20XX_RADIO_FLRC_BR_1_040_BW_1_333:
        {
            return (const char*) "1_040_BW_1_333";
        }

        case LR20XX_RADIO_FLRC_BR_0_650_BW_0_740:
        {
            return (const char*) "0_650_BW_0_740";
        }

        case LR20XX_RADIO_FLRC_BR_0_520_BW_0_571:
        {
            return (const char*) "0_520_BW_0_571";
        }

        case LR20XX_RADIO_FLRC_BR_0_325_BW_0_357:
        {
            return (const char*) "0_325_BW_0_357";
        }

        case LR20XX_RADIO_FLRC_BR_0_260_BW_0_307:
        {
            return (const char*) "0_260_BW_0_307";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_flrc_cr_to_str( const lr20xx_radio_flrc_cr_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_FLRC_CR_1_2:
        {
            return (const char*) "1_2";
        }

        case LR20XX_RADIO_FLRC_CR_3_4:
        {
            return (const char*) "3_4";
        }

        case LR20XX_RADIO_FLRC_CR_NONE:
        {
            return (const char*) "NONE";
        }

        case LR20XX_RADIO_FLRC_CR_2_3:
        {
            return (const char*) "2_3";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_flrc_pulse_shape_to_str( const lr20xx_radio_flrc_pulse_shape_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_FLRC_PULSE_SHAPE_OFF:
        {
            return (const char*) "OFF";
        }

        case LR20XX_RADIO_FLRC_PULSE_SHAPE_BT_05:
        {
            return (const char*) "BT_05";
        }

        case LR20XX_RADIO_FLRC_PULSE_SHAPE_BT_1:
        {
            return (const char*) "BT_1";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_flrc_preamble_len_to_str( const lr20xx_radio_flrc_preamble_len_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_FLRC_PREAMBLE_LEN_04_BITS:
        {
            return (const char*) "04_BITS";
        }

        case LR20XX_RADIO_FLRC_PREAMBLE_LEN_08_BITS:
        {
            return (const char*) "08_BITS";
        }

        case LR20XX_RADIO_FLRC_PREAMBLE_LEN_12_BITS:
        {
            return (const char*) "12_BITS";
        }

        case LR20XX_RADIO_FLRC_PREAMBLE_LEN_16_BITS:
        {
            return (const char*) "16_BITS";
        }

        case LR20XX_RADIO_FLRC_PREAMBLE_LEN_20_BITS:
        {
            return (const char*) "20_BITS";
        }

        case LR20XX_RADIO_FLRC_PREAMBLE_LEN_24_BITS:
        {
            return (const char*) "24_BITS";
        }

        case LR20XX_RADIO_FLRC_PREAMBLE_LEN_28_BITS:
        {
            return (const char*) "28_BITS";
        }

        case LR20XX_RADIO_FLRC_PREAMBLE_LEN_32_BITS:
        {
            return (const char*) "32_BITS";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_flrc_rx_match_sync_word_to_str( const lr20xx_radio_flrc_rx_match_sync_word_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_FLRC_RX_MATCH_SYNCWORD_OFF:
        {
            return (const char*) "OFF";
        }

        case LR20XX_RADIO_FLRC_RX_MATCH_SYNCWORD_1:
        {
            return (const char*) "1";
        }

        case LR20XX_RADIO_FLRC_RX_MATCH_SYNCWORD_2:
        {
            return (const char*) "2";
        }

        case LR20XX_RADIO_FLRC_RX_MATCH_SYNCWORD_1_OR_2:
        {
            return (const char*) "1_OR_2";
        }

        case LR20XX_RADIO_FLRC_RX_MATCH_SYNCWORD_3:
        {
            return (const char*) "3";
        }

        case LR20XX_RADIO_FLRC_RX_MATCH_SYNCWORD_1_OR_3:
        {
            return (const char*) "1_OR_3";
        }

        case LR20XX_RADIO_FLRC_RX_MATCH_SYNCWORD_2_OR_3:
        {
            return (const char*) "2_OR_3";
        }

        case LR20XX_RADIO_FLRC_RX_MATCH_SYNCWORD_1_OR_2_OR_3:
        {
            return (const char*) "1_OR_2_OR_3";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_flrc_sync_word_len_to_str( const lr20xx_radio_flrc_sync_word_len_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_FLRC_SYNCWORD_LENGTH_OFF:
        {
            return (const char*) "OFF";
        }

        case LR20XX_RADIO_FLRC_SYNCWORD_LENGTH_2_BYTES:
        {
            return (const char*) "2_BYTES";
        }

        case LR20XX_RADIO_FLRC_SYNCWORD_LENGTH_4_BYTES:
        {
            return (const char*) "4_BYTES";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_flrc_pkt_len_modes_to_str( const lr20xx_radio_flrc_pkt_len_modes_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_FLRC_PKT_VAR_LEN:
        {
            return (const char*) "VAR_LEN";
        }

        case LR20XX_RADIO_FLRC_PKT_FIX_LEN:
        {
            return (const char*) "FIX_LEN";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_flrc_crc_types_to_str( const lr20xx_radio_flrc_crc_types_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_FLRC_CRC_OFF:
        {
            return (const char*) "OFF";
        }

        case LR20XX_RADIO_FLRC_CRC_2_BYTES:
        {
            return (const char*) "2_BYTES";
        }

        case LR20XX_RADIO_FLRC_CRC_3_BYTES:
        {
            return (const char*) "3_BYTES";
        }

        case LR20XX_RADIO_FLRC_CRC_4_BYTES:
        {
            return (const char*) "4_BYTES";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}
