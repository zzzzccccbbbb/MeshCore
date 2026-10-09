#include "lr20xx_radio_common_types_str.h"

const char* lr20xx_radio_common_rx_path_to_str( const lr20xx_radio_common_rx_path_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_COMMON_RX_PATH_LF:
        {
            return (const char*) "LF";
        }

        case LR20XX_RADIO_COMMON_RX_PATH_HF:
        {
            return (const char*) "HF";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_common_rx_path_boost_mode_to_str( const lr20xx_radio_common_rx_path_boost_mode_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_COMMON_RX_PATH_BOOST_MODE_NONE:
        {
            return (const char*) "NONE";
        }

        case LR20XX_RADIO_COMMON_RX_PATH_BOOST_MODE_1:
        {
            return (const char*) "1";
        }

        case LR20XX_RADIO_COMMON_RX_PATH_BOOST_MODE_2:
        {
            return (const char*) "2";
        }

        case LR20XX_RADIO_COMMON_RX_PATH_BOOST_MODE_3:
        {
            return (const char*) "3";
        }

        case LR20XX_RADIO_COMMON_RX_PATH_BOOST_MODE_4:
        {
            return (const char*) "4";
        }

        case LR20XX_RADIO_COMMON_RX_PATH_BOOST_MODE_5:
        {
            return (const char*) "5";
        }

        case LR20XX_RADIO_COMMON_RX_PATH_BOOST_MODE_6:
        {
            return (const char*) "6";
        }

        case LR20XX_RADIO_COMMON_RX_PATH_BOOST_MODE_7:
        {
            return (const char*) "7";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_common_pa_selection_to_str( const lr20xx_radio_common_pa_selection_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_COMMON_PA_SEL_LF:
        {
            return (const char*) "LF";
        }

        case LR20XX_RADIO_COMMON_PA_SEL_HF:
        {
            return (const char*) "HF";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

// const char* lr20xx_radio_common_pa_lf_mode_to_str( const lr20xx_radio_common_pa_lf_mode_t value )
// {
//     switch( value )
//     {
//         case LR20XX_RADIO_COMMON_PA_LF_MODE_FSM:
//         {
//             return (const char*) "FSM";
//         }

//         case LR20XX_RADIO_COMMON_PA_LF_MODE_FDM:
//         {
//             return (const char*) "FDM";
//         }

//         case LR20XX_RADIO_COMMON_PA_LF_MODE_HSM_RFO_LF1:
//         {
//             return (const char*) "HSM_RFO_LF1";
//         }

//         case LR20XX_RADIO_COMMON_PA_LF_MODE_HSM_RFO_LF2:
//         {
//             return (const char*) "HSM_RFO_LF2";
//         }

//         default:
//         {
//             return (const char*) "Unknown";
//         }
//     }
// }

const char* lr20xx_radio_common_ramp_time_to_str( const lr20xx_radio_common_ramp_time_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_COMMON_RAMP_2_US:
        {
            return (const char*) "2_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_4_US:
        {
            return (const char*) "4_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_8_US:
        {
            return (const char*) "8_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_16_US:
        {
            return (const char*) "16_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_32_US:
        {
            return (const char*) "32_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_48_US:
        {
            return (const char*) "48_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_64_US:
        {
            return (const char*) "64_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_80_US:
        {
            return (const char*) "80_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_96_US:
        {
            return (const char*) "96_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_112_US:
        {
            return (const char*) "112_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_128_US:
        {
            return (const char*) "128_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_144_US:
        {
            return (const char*) "144_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_160_US:
        {
            return (const char*) "160_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_176_US:
        {
            return (const char*) "176_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_192_US:
        {
            return (const char*) "192_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_208_US:
        {
            return (const char*) "208_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_240_US:
        {
            return (const char*) "240_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_272_US:
        {
            return (const char*) "272_US";
        }

        case LR20XX_RADIO_COMMON_RAMP_304_US:
        {
            return (const char*) "304_US";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_common_fallback_modes_to_str( const lr20xx_radio_common_fallback_modes_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_FALLBACK_STDBY_RC:
        {
            return (const char*) "STDBY_RC";
        }

        case LR20XX_RADIO_FALLBACK_STDBY_XOSC:
        {
            return (const char*) "STDBY_XOSC";
        }

        case LR20XX_RADIO_FALLBACK_FS:
        {
            return (const char*) "FS";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_common_pkt_type_to_str( const lr20xx_radio_common_pkt_type_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_COMMON_PKT_TYPE_LORA:
        {
            return (const char*) "LORA";
        }

        case LR20XX_RADIO_COMMON_PKT_TYPE_FSK:
        {
            return (const char*) "FSK";
        }

        case LR20XX_RADIO_COMMON_PKT_TYPE_BLUETOOTH_LE:
        {
            return (const char*) "BLUETOOTH_LE";
        }

        case LR20XX_RADIO_COMMON_PKT_TYPE_RTTOF:
        {
            return (const char*) "RTTOF";
        }

        case LR20XX_RADIO_COMMON_PKT_TYPE_FLRC:
        {
            return (const char*) "FLRC";
        }

        case LR20XX_RADIO_COMMON_PKT_TYPE_BPSK:
        {
            return (const char*) "BPSK";
        }

        case LR20XX_RADIO_COMMON_PKT_TYPE_LRFHSS:
        {
            return (const char*) "LRFHSS";
        }

        case LR20XX_RADIO_COMMON_PKT_TYPE_WM_BUS:
        {
            return (const char*) "WM_BUS";
        }

        case LR20XX_RADIO_COMMON_PKT_TYPE_WI_SUN:
        {
            return (const char*) "WI_SUN";
        }

        case LR20XX_RADIO_COMMON_PKT_TYPE_Z_WAVE:
        {
            return (const char*) "Z_WAVE";
        }

        case LR20XX_RADIO_COMMON_PKT_TYPE_OOK:
        {
            return (const char*) "OOK";
        }

        case LR20XX_RADIO_COMMON_PKT_TYPE_OQPSK_15_4:
        {
            return (const char*) "OQPSK_15_4";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_common_rx_duty_cycle_mode_to_str( const lr20xx_radio_common_rx_duty_cycle_mode_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_COMMON_RX_DUTY_CYCLE_MODE_RX:
        {
            return (const char*) "RX";
        }

        case LR20XX_RADIO_COMMON_RX_DUTY_CYCLE_MODE_CAD:
        {
            return (const char*) "CAD";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_common_timestamp_cfg_slot_to_str( const lr20xx_radio_common_timestamp_cfg_slot_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_COMMON_TIMESTAMP_CFG_SLOT_0:
        {
            return (const char*) "0";
        }

        case LR20XX_RADIO_COMMON_TIMESTAMP_CFG_SLOT_1:
        {
            return (const char*) "1";
        }

        case LR20XX_RADIO_COMMON_TIMESTAMP_CFG_SLOT_2:
        {
            return (const char*) "2";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_common_timestamp_source_to_str( const lr20xx_radio_common_timestamp_source_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_COMMON_TIMESTAMP_SOURCE_NONE:
        {
            return (const char*) "NONE";
        }

        case LR20XX_RADIO_COMMON_TIMESTAMP_SOURCE_TX_DONE:
        {
            return (const char*) "TX_DONE";
        }

        case LR20XX_RADIO_COMMON_TIMESTAMP_SOURCE_RX_DONE:
        {
            return (const char*) "RX_DONE";
        }

        case LR20XX_RADIO_COMMON_TIMESTAMP_SOURCE_SYNC:
        {
            return (const char*) "SYNC";
        }

        case LR20XX_RADIO_COMMON_TIMESTAMP_SOURCE_HEADER:
        {
            return (const char*) "HEADER";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_common_tx_test_mode_to_str( const lr20xx_radio_common_tx_test_mode_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_COMMON_TX_TEST_MODE_NORMAL:
        {
            return (const char*) "NORMAL";
        }

        case LR20XX_RADIO_COMMON_TX_TEST_MODE_INFINITE_PREAMBLE:
        {
            return (const char*) "INFINITE_PREAMBLE";
        }

        case LR20XX_RADIO_COMMON_TX_TEST_MODE_CONTINUOUS_WAVE:
        {
            return (const char*) "CONTINUOUS_WAVE";
        }

        case LR20XX_RADIO_COMMON_TX_TEST_MODE_PRBS9:
        {
            return (const char*) "PRBS9";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_common_gain_step_to_str( const lr20xx_radio_common_gain_step_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_COMMON_GAIN_STEP_AUTO:
        {
            return (const char*) "AUTO";
        }

        case LR20XX_RADIO_COMMON_GAIN_STEP_G1:
        {
            return (const char*) "G1";
        }

        case LR20XX_RADIO_COMMON_GAIN_STEP_G2:
        {
            return (const char*) "G2";
        }

        case LR20XX_RADIO_COMMON_GAIN_STEP_G3:
        {
            return (const char*) "G3";
        }

        case LR20XX_RADIO_COMMON_GAIN_STEP_G4:
        {
            return (const char*) "G4";
        }

        case LR20XX_RADIO_COMMON_GAIN_STEP_G5:
        {
            return (const char*) "G5";
        }

        case LR20XX_RADIO_COMMON_GAIN_STEP_G6:
        {
            return (const char*) "G6";
        }

        case LR20XX_RADIO_COMMON_GAIN_STEP_G7:
        {
            return (const char*) "G7";
        }

        case LR20XX_RADIO_COMMON_GAIN_STEP_G8:
        {
            return (const char*) "G8";
        }

        case LR20XX_RADIO_COMMON_GAIN_STEP_G9:
        {
            return (const char*) "G9";
        }

        case LR20XX_RADIO_COMMON_GAIN_STEP_G10:
        {
            return (const char*) "G10";
        }

        case LR20XX_RADIO_COMMON_GAIN_STEP_G11:
        {
            return (const char*) "G11";
        }

        case LR20XX_RADIO_COMMON_GAIN_STEP_G12:
        {
            return (const char*) "G12";
        }

        case LR20XX_RADIO_COMMON_GAIN_STEP_G13:
        {
            return (const char*) "G13";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_common_cad_exit_mode_to_str( const lr20xx_radio_common_cad_exit_mode_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_COMMON_CAD_EXIT_MODE_FALLBACK:
        {
            return (const char*) "FALLBACK";
        }

        case LR20XX_RADIO_COMMON_CAD_EXIT_MODE_TX:
        {
            return (const char*) "TX";
        }

        case LR20XX_RADIO_COMMON_CAD_EXIT_MODE_RX:
        {
            return (const char*) "RX";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_common_auto_tx_rx_conditions_to_str( const lr20xx_radio_common_auto_tx_rx_conditions_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_COMMON_AUTO_TX_RX_OFF:
        {
            return (const char*) "OFF";
        }

        case LR20XX_RADIO_COMMON_AUTO_TX_RX_ALWAYS:
        {
            return (const char*) "ALWAYS";
        }

        case LR20XX_RADIO_COMMON_AUTO_TX_RX_RX_DONE_ONLY:
        {
            return (const char*) "RX_DONE_ONLY";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}
