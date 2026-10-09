#ifndef LR20XX_RADIO_FSK_TYPES_STR_H
#define LR20XX_RADIO_FSK_TYPES_STR_H
#include "lr20xx_radio_fsk_types.h"
#ifdef __cplusplus
extern "C"
{
#endif
    const char* lr20xx_radio_fsk_common_bw_to_str( const lr20xx_radio_fsk_common_bw_t value );
    const char* lr20xx_radio_fsk_pulse_shape_to_str( const lr20xx_radio_fsk_pulse_shape_t value );
    const char* lr20xx_radio_fsk_preamble_detector_to_str( const lr20xx_radio_fsk_preamble_detector_t value );
    const char* lr20xx_radio_fsk_address_filtering_to_str( const lr20xx_radio_fsk_address_filtering_t value );
    const char* lr20xx_radio_fsk_header_mode_to_str( const lr20xx_radio_fsk_header_mode_t value );
    const char* lr20xx_radio_fsk_crc_to_str( const lr20xx_radio_fsk_crc_t value );
    const char* lr20xx_radio_fsk_whitening_to_str( const lr20xx_radio_fsk_whitening_t value );
    const char* lr20xx_radio_fsk_whitening_compatibility_to_str(
        const lr20xx_radio_fsk_whitening_compatibility_t value );
    const char* lr20xx_radio_fsk_syncword_bit_order_to_str( const lr20xx_radio_fsk_syncword_bit_order_t value );
#ifdef __cplusplus
}
#endif
#endif    // LR20XX_RADIO_FSK_TYPES_STR_H
