#include "lr20xx_radio_ook_types_str.h"

const char* lr20xx_radio_ook_pulse_shape_filter_to_str( const lr20xx_radio_ook_pulse_shape_filter_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_OOK_PULSE_SHAPE_NO_FILTER:
        {
            return (const char*) "NO_FILTER";
        }

        case LR20XX_RADIO_OOK_PULSE_SHAPE_FILTER_GAUSSIAN:
        {
            return (const char*) "FILTER_GAUSSIAN";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_ook_pulse_shape_bt_to_str( const lr20xx_radio_ook_pulse_shape_bt_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_OOK_PULSE_SHAPE_BT_0_3:
        {
            return (const char*) "0_3";
        }

        case LR20XX_RADIO_OOK_PULSE_SHAPE_BT_0_5:
        {
            return (const char*) "0_5";
        }

        case LR20XX_RADIO_OOK_PULSE_SHAPE_BT_0_7:
        {
            return (const char*) "0_7";
        }

        case LR20XX_RADIO_OOK_PULSE_SHAPE_BT_1_0:
        {
            return (const char*) "1_0";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_ook_mag_depth_to_str( const lr20xx_radio_ook_mag_depth_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_OOK_MAG_DEPTH_FULL:
        {
            return (const char*) "FULL";
        }

        case LR20XX_RADIO_OOK_MAG_DEPTH_UP_TO_20DB:
        {
            return (const char*) "UP_TO_20DB";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_ook_address_filtering_to_str( const lr20xx_radio_ook_address_filtering_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_OOK_ADDRESS_FILTERING_DISABLED:
        {
            return (const char*) "DISABLED";
        }

        case LR20XX_RADIO_OOK_ADDRESS_FILTERING_NODE:
        {
            return (const char*) "NODE";
        }

        case LR20XX_RADIO_OOK_ADDRESS_FILTERING_NODE_BROADCAST:
        {
            return (const char*) "NODE_BROADCAST";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_ook_header_mode_to_str( const lr20xx_radio_ook_header_mode_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_OOK_HEADER_IMPLICIT:
        {
            return (const char*) "IMPLICIT";
        }

        case LR20XX_RADIO_OOK_HEADER_EXPLICIT:
        {
            return (const char*) "EXPLICIT";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_ook_crc_to_str( const lr20xx_radio_ook_crc_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_OOK_CRC_OFF:
        {
            return (const char*) "OFF";
        }

        case LR20XX_RADIO_OOK_CRC_1_BYTE:
        {
            return (const char*) "1_BYTE";
        }

        case LR20XX_RADIO_OOK_CRC_2_BYTES:
        {
            return (const char*) "2_BYTES";
        }

        case LR20XX_RADIO_OOK_CRC_3_BYTES:
        {
            return (const char*) "3_BYTES";
        }

        case LR20XX_RADIO_OOK_CRC_4_BYTES:
        {
            return (const char*) "4_BYTES";
        }

        case LR20XX_RADIO_OOK_CRC_1_BYTE_INVERTED:
        {
            return (const char*) "1_BYTE_INVERTED";
        }

        case LR20XX_RADIO_OOK_CRC_2_BYTES_INVERTED:
        {
            return (const char*) "2_BYTES_INVERTED";
        }

        case LR20XX_RADIO_OOK_CRC_3_BYTES_INVERTED:
        {
            return (const char*) "3_BYTES_INVERTED";
        }

        case LR20XX_RADIO_OOK_CRC_4_BYTES_INVERTED:
        {
            return (const char*) "4_BYTES_INVERTED";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_ook_encoding_to_str( const lr20xx_radio_ook_encoding_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_OOK_ENCODING_OFF:
        {
            return (const char*) "OFF";
        }

        case LR20XX_RADIO_OOK_ENCODING_MANCHESTER:
        {
            return (const char*) "MANCHESTER";
        }

        case LR20XX_RADIO_OOK_ENCODING_MANCHESTER_INV:
        {
            return (const char*) "MANCHESTER_INV";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_ook_syncword_bit_order_to_str( const lr20xx_radio_ook_syncword_bit_order_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_OOK_SYNCWORD_LSBF:
        {
            return (const char*) "LSBF";
        }

        case LR20XX_RADIO_OOK_SYNCWORD_MSBF:
        {
            return (const char*) "MSBF";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}

const char* lr20xx_radio_ook_rx_detector_sfd_type_to_str( const lr20xx_radio_ook_rx_detector_sfd_type_t value )
{
    switch( value )
    {
        case LR20XX_RADIO_OOK_RX_DETECTOR_SFD_TYPE_FALLING_EDGE:
        {
            return (const char*) "FALLING_EDGE";
        }

        case LR20XX_RADIO_OOK_RX_DETECTOR_SFD_TYPE_RISING_EDGE:
        {
            return (const char*) "RISING_EDGE";
        }

        default:
        {
            return (const char*) "Unknown";
        }
    }
}
