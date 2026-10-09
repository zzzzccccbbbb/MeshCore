#ifndef LR20XX_RADIO_BPSK_TYPES_STR_H
#define LR20XX_RADIO_BPSK_TYPES_STR_H
#include "lr20xx_radio_bpsk_types.h"
#ifdef __cplusplus
extern "C"
{
#endif
    const char* lr20xx_radio_bpsk_mod_params_br_unit_to_str( const lr20xx_radio_bpsk_mod_params_br_unit_t value );
    const char* lr20xx_radio_bpsk_mod_params_shape_filter_to_str(
        const lr20xx_radio_bpsk_mod_params_shape_filter_t value );
    const char* lr20xx_radio_bpsk_differential_encoding_enable_to_str(
        const lr20xx_radio_bpsk_differential_encoding_enable_t value );
    const char* lr20xx_radio_bpsk_phy_mode_to_str( const lr20xx_radio_bpsk_phy_mode_t value );
    const char* lr20xx_radio_bpsk_sigfox_message_type_to_str( const lr20xx_radio_bpsk_sigfox_message_type_t value );
    const char* lr20xx_radio_bpsk_sigfox_frame_rank_to_str( const lr20xx_radio_bpsk_sigfox_frame_rank_t value );
#ifdef __cplusplus
}
#endif
#endif    // LR20XX_RADIO_BPSK_TYPES_STR_H
