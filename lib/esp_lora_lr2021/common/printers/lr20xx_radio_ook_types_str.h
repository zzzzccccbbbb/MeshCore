#ifndef LR20XX_RADIO_OOK_TYPES_STR_H
#define LR20XX_RADIO_OOK_TYPES_STR_H
#include "lr20xx_radio_ook_types.h"
#ifdef __cplusplus
extern "C"
{
#endif
    const char* lr20xx_radio_fsk_common_bw_to_str( const lr20xx_radio_fsk_common_bw_t value );
    const char* lr20xx_radio_ook_pulse_shape_filter_to_str( const lr20xx_radio_ook_pulse_shape_filter_t value );
    const char* lr20xx_radio_ook_pulse_shape_bt_to_str( const lr20xx_radio_ook_pulse_shape_bt_t value );
    const char* lr20xx_radio_ook_mag_depth_to_str( const lr20xx_radio_ook_mag_depth_t value );
    const char* lr20xx_radio_ook_address_filtering_to_str( const lr20xx_radio_ook_address_filtering_t value );
    const char* lr20xx_radio_ook_header_mode_to_str( const lr20xx_radio_ook_header_mode_t value );
    const char* lr20xx_radio_ook_crc_to_str( const lr20xx_radio_ook_crc_t value );
    const char* lr20xx_radio_ook_encoding_to_str( const lr20xx_radio_ook_encoding_t value );
    const char* lr20xx_radio_ook_syncword_bit_order_to_str( const lr20xx_radio_ook_syncword_bit_order_t value );
    const char* lr20xx_radio_ook_rx_detector_sfd_type_to_str( const lr20xx_radio_ook_rx_detector_sfd_type_t value );
#ifdef __cplusplus
}
#endif
#endif    // LR20XX_RADIO_OOK_TYPES_STR_H
