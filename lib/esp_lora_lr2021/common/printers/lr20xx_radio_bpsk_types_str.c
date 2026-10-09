#include "lr20xx_radio_bpsk_types_str.h"

const char* lr20xx_radio_bpsk_mod_params_br_unit_to_str( const lr20xx_radio_bpsk_mod_params_br_unit_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_BPSK_MOD_PARAMS_BR_IN_BPS:
        {
            return (const char*) "BPS";
        }

        case LR20XX_RADIO_BPSK_MOD_PARAMS_BR_IN_BPS_DIV_256:
        {
            return (const char*) "BPS_DIV_256";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_bpsk_mod_params_shape_filter_to_str( const lr20xx_radio_bpsk_mod_params_shape_filter_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_BPSK_MOD_PARAMS_SHAPE_FILTER_NONE:
        {
            return (const char*) "NONE";
        }

        case LR20XX_RADIO_BPSK_MOD_PARAMS_SHAPE_FILTER_GAUSSIAN_BT_0_3:
        {
            return (const char*) "GAUSSIAN_BT_0_3";
        }

        case LR20XX_RADIO_BPSK_MOD_PARAMS_SHAPE_FILTER_GAUSSIAN_BT_0_5:
        {
            return (const char*) "GAUSSIAN_BT_0_5";
        }

        case LR20XX_RADIO_BPSK_MOD_PARAMS_SHAPE_FILTER_GAUSSIAN_BT_0_7:
        {
            return (const char*) "GAUSSIAN_BT_0_7";
        }

        case LR20XX_RADIO_BPSK_MOD_PARAMS_SHAPE_FILTER_GAUSSIAN_BT_1_0:
        {
            return (const char*) "GAUSSIAN_BT_1_0";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_bpsk_differential_encoding_enable_to_str(
    const lr20xx_radio_bpsk_differential_encoding_enable_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_BPSK_DIFFERENTIAL_ENCODING_DISABLE:
        {
            return (const char*) "DISABLE";
        }

        case LR20XX_RADIO_BPSK_DIFFERENTIAL_ENCODING_ENABLE:
        {
            return (const char*) "ENABLE";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_bpsk_phy_mode_to_str( const lr20xx_radio_bpsk_phy_mode_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_BPSK_PHY_MODE_RAW:
        {
            return (const char*) "RAW";
        }

        case LR20XX_RADIO_BPSK_PHY_MODE_SIGFOX:
        {
            return (const char*) "SIGFOX";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_bpsk_sigfox_message_type_to_str( const lr20xx_radio_bpsk_sigfox_message_type_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_BPSK_SIGFOX_MESSAGE_TYPE_APPLICATION:
        {
            return (const char*) "APPLICATION";
        }

        case LR20XX_RADIO_BPSK_SIGFOX_MESSAGE_TYPE_CONTROL:
        {
            return (const char*) "CONTROL";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_bpsk_sigfox_frame_rank_to_str( const lr20xx_radio_bpsk_sigfox_frame_rank_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_BPSK_SIGFOX_FRAME_RANK_FIRST_LAST:
        {
            return (const char*) "FIRST_LAST";
        }

        case LR20XX_RADIO_BPSK_SIGFOX_FRAME_RANK_SECOND:
        {
            return (const char*) "SECOND";
        }

        case LR20XX_RADIO_BPSK_SIGFOX_FRAME_RANK_THIRD:
        {
            return (const char*) "THIRD";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}
