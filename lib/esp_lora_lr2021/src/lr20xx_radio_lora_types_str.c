#include "lr20xx_radio_lora_types_str.h"

const char* lr20xx_radio_lora_sf_to_str( const lr20xx_radio_lora_sf_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_LORA_SF5:
        {
            return (const char*) "SF5";
        }

        case LR20XX_RADIO_LORA_SF6:
        {
            return (const char*) "SF6";
        }

        case LR20XX_RADIO_LORA_SF7:
        {
            return (const char*) "SF7";
        }

        case LR20XX_RADIO_LORA_SF8:
        {
            return (const char*) "SF8";
        }

        case LR20XX_RADIO_LORA_SF9:
        {
            return (const char*) "SF9";
        }

        case LR20XX_RADIO_LORA_SF10:
        {
            return (const char*) "SF10";
        }

        case LR20XX_RADIO_LORA_SF11:
        {
            return (const char*) "SF11";
        }

        case LR20XX_RADIO_LORA_SF12:
        {
            return (const char*) "SF12";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_lora_bw_to_str( const lr20xx_radio_lora_bw_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_LORA_BW_7:
        {
            return (const char*) "7";
        }

        case LR20XX_RADIO_LORA_BW_10:
        {
            return (const char*) "10";
        }

        case LR20XX_RADIO_LORA_BW_15:
        {
            return (const char*) "15";
        }

        case LR20XX_RADIO_LORA_BW_20:
        {
            return (const char*) "20";
        }

        case LR20XX_RADIO_LORA_BW_31:
        {
            return (const char*) "31";
        }

        case LR20XX_RADIO_LORA_BW_41:
        {
            return (const char*) "41";
        }

        case LR20XX_RADIO_LORA_BW_83:
        {
            return (const char*) "83";
        }

        case LR20XX_RADIO_LORA_BW_62:
        {
            return (const char*) "62";
        }

        case LR20XX_RADIO_LORA_BW_101:
        {
            return (const char*) "101";
        }

        case LR20XX_RADIO_LORA_BW_125:
        {
            return (const char*) "125";
        }

        case LR20XX_RADIO_LORA_BW_203:
        {
            return (const char*) "203";
        }

        case LR20XX_RADIO_LORA_BW_250:
        {
            return (const char*) "250";
        }

        case LR20XX_RADIO_LORA_BW_406:
        {
            return (const char*) "406";
        }

        case LR20XX_RADIO_LORA_BW_500:
        {
            return (const char*) "500";
        }

        case LR20XX_RADIO_LORA_BW_812:
        {
            return (const char*) "812";
        }

        case LR20XX_RADIO_LORA_BW_1000:
        {
            return (const char*) "1000";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_lora_cr_to_str( const lr20xx_radio_lora_cr_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_LORA_NO_CR:
        {
            return (const char*) "NO_CR";
        }

        case LR20XX_RADIO_LORA_CR_4_5:
        {
            return (const char*) "CR_4_5";
        }

        case LR20XX_RADIO_LORA_CR_4_6:
        {
            return (const char*) "CR_4_6";
        }

        case LR20XX_RADIO_LORA_CR_4_7:
        {
            return (const char*) "CR_4_7";
        }

        case LR20XX_RADIO_LORA_CR_4_8:
        {
            return (const char*) "CR_4_8";
        }

        case LR20XX_RADIO_LORA_CR_LI_4_5:
        {
            return (const char*) "CR_LI_4_5";
        }

        case LR20XX_RADIO_LORA_CR_LI_4_6:
        {
            return (const char*) "CR_LI_4_6";
        }

        case LR20XX_RADIO_LORA_CR_LI_4_8:
        {
            return (const char*) "CR_LI_4_8";
        }

        case LR20XX_RADIO_LORA_CR_LI_CONVOLUTIONAL_4_6:
        {
            return (const char*) "CR_LI_CONVOLUTIONAL_4_6";
        }

        case LR20XX_RADIO_LORA_CR_LI_CONVOLUTIONAL_4_8:
        {
            return (const char*) "CR_LI_CONVOLUTIONAL_4_8";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_lora_ppm_to_str( const lr20xx_radio_lora_ppm_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_LORA_NO_PPM:
        {
            return (const char*) "NO_PPM";
        }

        case LR20XX_RADIO_LORA_PPM_1_4:
        {
            return (const char*) "PPM_1_4";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_lora_pkt_mode_to_str( const lr20xx_radio_lora_pkt_mode_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_LORA_PKT_EXPLICIT:
        {
            return (const char*) "EXPLICIT";
        }

        case LR20XX_RADIO_LORA_PKT_IMPLICIT:
        {
            return (const char*) "IMPLICIT";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_lora_crc_to_str( const lr20xx_radio_lora_crc_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_LORA_CRC_DISABLED:
        {
            return (const char*) "DISABLED";
        }

        case LR20XX_RADIO_LORA_CRC_ENABLED:
        {
            return (const char*) "ENABLED";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_lora_iq_to_str( const lr20xx_radio_lora_iq_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_LORA_IQ_STANDARD:
        {
            return (const char*) "STANDARD";
        }

        case LR20XX_RADIO_LORA_IQ_INVERTED:
        {
            return (const char*) "INVERTED";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_lora_cad_exit_mode_to_str( const lr20xx_radio_lora_cad_exit_mode_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_LORA_CAD_EXIT_MODE_STANDBYRC:
        {
            return (const char*) "STANDBYRC";
        }

        case LR20XX_RADIO_LORA_CAD_EXIT_MODE_RX:
        {
            return (const char*) "RX";
        }

        case LR20XX_RADIO_LORA_CAD_EXIT_MODE_TX:
        {
            return (const char*) "TX";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_lora_hooping_ctrl_to_str( const lr20xx_radio_lora_hooping_ctrl_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_LORA_HOPPING_CTRL_DISABLED:
        {
            return (const char*) "DISABLED";
        }

        case LR20XX_RADIO_LORA_HOPPING_CTRL_ENABLED:
        {
            return (const char*) "ENABLED";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}
