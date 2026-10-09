#ifndef LR20XX_RADIO_LORA_TYPES_STR_H
#define LR20XX_RADIO_LORA_TYPES_STR_H
#include "lr20xx_radio_lora_types.h"
#ifdef __cplusplus
extern "C"
{
#endif
    const char* lr20xx_radio_lora_sf_to_str( const lr20xx_radio_lora_sf_t value );
    const char* lr20xx_radio_lora_bw_to_str( const lr20xx_radio_lora_bw_t value );
    const char* lr20xx_radio_lora_cr_to_str( const lr20xx_radio_lora_cr_t value );
    const char* lr20xx_radio_lora_ppm_to_str( const lr20xx_radio_lora_ppm_t value );
    const char* lr20xx_radio_lora_pkt_mode_to_str( const lr20xx_radio_lora_pkt_mode_t value );
    const char* lr20xx_radio_lora_crc_to_str( const lr20xx_radio_lora_crc_t value );
    const char* lr20xx_radio_lora_iq_to_str( const lr20xx_radio_lora_iq_t value );
    const char* lr20xx_radio_lora_cad_exit_mode_to_str( const lr20xx_radio_lora_cad_exit_mode_t value );
    const char* lr20xx_radio_lora_hooping_ctrl_to_str( const lr20xx_radio_lora_hooping_ctrl_t value );
#ifdef __cplusplus
}
#endif
#endif    // LR20XX_RADIO_LORA_TYPES_STR_H
