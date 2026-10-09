#ifndef LR20XX_RADIO_COMMON_TYPES_STR_H
#define LR20XX_RADIO_COMMON_TYPES_STR_H
#include "lr20xx_radio_common_types.h"
#ifdef __cplusplus
extern "C"
{
#endif
    const char* lr20xx_radio_common_rx_path_to_str( const lr20xx_radio_common_rx_path_t value );
    const char* lr20xx_radio_common_rx_path_boost_mode_to_str( const lr20xx_radio_common_rx_path_boost_mode_t value );
    const char* lr20xx_radio_common_pa_selection_to_str( const lr20xx_radio_common_pa_selection_t value );
    const char* lr20xx_radio_common_pa_lf_mode_to_str( const lr20xx_radio_common_pa_lf_mode_t value );
    const char* lr20xx_radio_common_ramp_time_to_str( const lr20xx_radio_common_ramp_time_t value );
    const char* lr20xx_radio_common_fallback_modes_to_str( const lr20xx_radio_common_fallback_modes_t value );
    const char* lr20xx_radio_common_pkt_type_to_str( const lr20xx_radio_common_pkt_type_t value );
    const char* lr20xx_radio_common_rx_duty_cycle_mode_to_str( const lr20xx_radio_common_rx_duty_cycle_mode_t value );
    const char* lr20xx_radio_common_timestamp_cfg_slot_to_str( const lr20xx_radio_common_timestamp_cfg_slot_t value );
    const char* lr20xx_radio_common_timestamp_source_to_str( const lr20xx_radio_common_timestamp_source_t value );
    const char* lr20xx_radio_common_tx_test_mode_to_str( const lr20xx_radio_common_tx_test_mode_t value );
    const char* lr20xx_radio_common_gain_step_to_str( const lr20xx_radio_common_gain_step_t value );
    const char* lr20xx_radio_common_cad_exit_mode_to_str( const lr20xx_radio_common_cad_exit_mode_t value );
    const char* lr20xx_radio_common_auto_tx_rx_conditions_to_str(
        const lr20xx_radio_common_auto_tx_rx_conditions_t value );
#ifdef __cplusplus
}
#endif
#endif    // LR20XX_RADIO_COMMON_TYPES_STR_H
