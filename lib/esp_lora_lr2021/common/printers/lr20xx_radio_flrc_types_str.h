#ifndef LR20XX_RADIO_FLRC_TYPES_STR_H
#define LR20XX_RADIO_FLRC_TYPES_STR_H
#include "lr20xx_radio_flrc_types.h"
#ifdef __cplusplus
extern "C"
{
#endif
    const char* lr20xx_radio_flrc_br_bw_to_str( const lr20xx_radio_flrc_br_bw_t value );
    const char* lr20xx_radio_flrc_cr_to_str( const lr20xx_radio_flrc_cr_t value );
    const char* lr20xx_radio_flrc_pulse_shape_to_str( const lr20xx_radio_flrc_pulse_shape_t value );
    const char* lr20xx_radio_flrc_preamble_len_to_str( const lr20xx_radio_flrc_preamble_len_t value );
    const char* lr20xx_radio_flrc_rx_match_sync_word_to_str( const lr20xx_radio_flrc_rx_match_sync_word_t value );
    const char* lr20xx_radio_flrc_sync_word_len_to_str( const lr20xx_radio_flrc_sync_word_len_t value );
    const char* lr20xx_radio_flrc_pkt_len_modes_to_str( const lr20xx_radio_flrc_pkt_len_modes_t value );
    const char* lr20xx_radio_flrc_crc_types_to_str( const lr20xx_radio_flrc_crc_types_t value );
#ifdef __cplusplus
}
#endif
#endif    // LR20XX_RADIO_FLRC_TYPES_STR_H
