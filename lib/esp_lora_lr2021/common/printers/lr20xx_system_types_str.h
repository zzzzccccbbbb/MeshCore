#ifndef LR20XX_SYSTEM_TYPES_STR_H
#define LR20XX_SYSTEM_TYPES_STR_H
#include "lr20xx_system_types.h"
#ifdef __cplusplus
extern "C" {
#endif
const char* lr20xx_system_command_status_to_str( const lr20xx_system_command_status_t value );
const char* lr20xx_system_reset_status_to_str( const lr20xx_system_reset_status_t value );
const char* lr20xx_system_chip_modes_to_str( const lr20xx_system_chip_modes_t value );
const char* lr20xx_system_dio_to_str( const lr20xx_system_dio_t value );
const char* lr20xx_system_dio_func_to_str( const lr20xx_system_dio_func_t value );
const char* lr20xx_system_dio_drive_to_str( const lr20xx_system_dio_drive_t value );
const char* lr20xx_system_dio_rf_switch_cfg_state_to_str( const lr20xx_system_dio_rf_switch_cfg_state_t value );
const char* lr20xx_system_lfclk_cfg_to_str( const lr20xx_system_lfclk_cfg_t value );
const char* lr20xx_system_tcxo_supply_voltage_to_str( const lr20xx_system_tcxo_supply_voltage_t value );
const char* lr20xx_system_reg_mode_to_str( const lr20xx_system_reg_mode_t value );
const char* lr20xx_system_value_format_to_str( const lr20xx_system_value_format_t value );
const char* lr20xx_system_meas_res_to_str( const lr20xx_system_meas_res_t value );
const char* lr20xx_system_temp_src_to_str( const lr20xx_system_temp_src_t value );
const char* lr20xx_system_random_entropy_source_to_str( const lr20xx_system_random_entropy_source_t value );
const char* lr20xx_system_standby_mode_to_str( const lr20xx_system_standby_mode_t value );
const char* lr20xx_system_hf_clk_scaling_to_str( const lr20xx_system_hf_clk_scaling_t value );
const char* lr20xx_system_lbd_trim_to_str( const lr20xx_system_lbd_trim_t value );
#ifdef __cplusplus
}
#endif
#endif  // LR20XX_SYSTEM_TYPES_STR_H
