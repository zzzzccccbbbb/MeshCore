#ifndef LR20XX_RADIO_LR_FHSS_TYPES_STR_H
#define LR20XX_RADIO_LR_FHSS_TYPES_STR_H
#include "lr20xx_radio_lr_fhss_types.h"
#ifdef __cplusplus
extern "C"
{
#endif
    const char* lr_fhss_v1_modulation_type_to_str( const lr_fhss_v1_modulation_type_t value );
    const char* lr_fhss_v1_cr_to_str( const lr_fhss_v1_cr_t value );
    const char* lr_fhss_v1_grid_to_str( const lr_fhss_v1_grid_t value );
    const char* lr_fhss_v1_bw_to_str( const lr_fhss_v1_bw_t value );
#ifdef __cplusplus
}
#endif
#endif    // LR20XX_RADIO_LR_FHSS_TYPES_STR_H
