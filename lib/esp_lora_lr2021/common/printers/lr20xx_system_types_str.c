#include "lr20xx_system_types_str.h"

const char* lr20xx_system_command_status_to_str( const lr20xx_system_command_status_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_CMD_STATUS_FAIL:
    {
        return ( const char* ) "FAIL";
    }

    case LR20XX_SYSTEM_CMD_STATUS_PERR:
    {
        return ( const char* ) "PERR";
    }

    case LR20XX_SYSTEM_CMD_STATUS_OK:
    {
        return ( const char* ) "OK";
    }

    case LR20XX_SYSTEM_CMD_STATUS_DATA:
    {
        return ( const char* ) "DATA";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_reset_status_to_str( const lr20xx_system_reset_status_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_RESET_STATUS_CLEARED:
    {
        return ( const char* ) "CLEARED";
    }

    case LR20XX_SYSTEM_RESET_STATUS_PWR_ON_BROWN_OUT:
    {
        return ( const char* ) "PWR_ON_BROWN_OUT";
    }

    case LR20XX_SYSTEM_RESET_STATUS_RESET_PIN:
    {
        return ( const char* ) "RESET_PIN";
    }

    // case LR20XX_SYSTEM_RESET_STATUS_RESET_CMD:
    // {
    //     return ( const char* ) "RESET_CMD";
    // }

    // case LR20XX_SYSTEM_RESET_STATUS_WATCHDOG:
    // {
    //     return ( const char* ) "WATCHDOG";
    // }

    case LR20XX_SYSTEM_RESET_STATUS_NSS_WAKEUP:
    {
        return ( const char* ) "NSS_WAKEUP";
    }

    case LR20XX_SYSTEM_RESET_STATUS_RTC_WAKEUP:
    {
        return ( const char* ) "RTC_WAKEUP";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_chip_modes_to_str( const lr20xx_system_chip_modes_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_CHIP_MODE_SLEEP:
    {
        return ( const char* ) "SLEEP";
    }

    case LR20XX_SYSTEM_CHIP_MODE_STBY_RC:
    {
        return ( const char* ) "STBY_RC";
    }

    case LR20XX_SYSTEM_CHIP_MODE_STBY_XOSC:
    {
        return ( const char* ) "STBY_XOSC";
    }

    case LR20XX_SYSTEM_CHIP_MODE_FS:
    {
        return ( const char* ) "FS";
    }

    case LR20XX_SYSTEM_CHIP_MODE_RX:
    {
        return ( const char* ) "RX";
    }

    case LR20XX_SYSTEM_CHIP_MODE_TX:
    {
        return ( const char* ) "TX";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_dio_to_str( const lr20xx_system_dio_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_DIO_5:
    {
        return ( const char* ) "5";
    }

    case LR20XX_SYSTEM_DIO_6:
    {
        return ( const char* ) "6";
    }

    case LR20XX_SYSTEM_DIO_7:
    {
        return ( const char* ) "7";
    }

    case LR20XX_SYSTEM_DIO_8:
    {
        return ( const char* ) "8";
    }

    case LR20XX_SYSTEM_DIO_9:
    {
        return ( const char* ) "9";
    }

    case LR20XX_SYSTEM_DIO_10:
    {
        return ( const char* ) "10";
    }

    case LR20XX_SYSTEM_DIO_11:
    {
        return ( const char* ) "11";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_dio_func_to_str( const lr20xx_system_dio_func_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_DIO_FUNC_NONE:
    {
        return ( const char* ) "NONE";
    }

    case LR20XX_SYSTEM_DIO_FUNC_IRQ:
    {
        return ( const char* ) "IRQ";
    }

    case LR20XX_SYSTEM_DIO_FUNC_RF_SWITCH:
    {
        return ( const char* ) "RF_SWITCH";
    }

    case LR20XX_SYSTEM_DIO_FUNC_GPIO_LOW:
    {
        return ( const char* ) "GPIO_LOW";
    }

    case LR20XX_SYSTEM_DIO_FUNC_GPIO_HIGH:
    {
        return ( const char* ) "GPIO_HIGH";
    }

    case LR20XX_SYSTEM_DIO_FUNC_HF_CLK_OUT:
    {
        return ( const char* ) "HF_CLK_OUT";
    }

    case LR20XX_SYSTEM_DIO_FUNC_LF_CLK_OUT:
    {
        return ( const char* ) "LF_CLK_OUT";
    }

    case LR20XX_SYSTEM_DIO_FUNC_TX_TRIGGER:
    {
        return ( const char* ) "TX_TRIGGER";
    }

    case LR20XX_SYSTEM_DIO_FUNC_RX_TRIGGER:
    {
        return ( const char* ) "RX_TRIGGER";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_dio_drive_to_str( const lr20xx_system_dio_drive_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_DIO_DRIVE_NONE:
    {
        return ( const char* ) "NONE";
    }

    case LR20XX_SYSTEM_DIO_DRIVE_PULL_DOWN:
    {
        return ( const char* ) "PULL_DOWN";
    }

    case LR20XX_SYSTEM_DIO_DRIVE_PULL_UP:
    {
        return ( const char* ) "PULL_UP";
    }

    case LR20XX_SYSTEM_DIO_DRIVE_AUTO:
    {
        return ( const char* ) "AUTO";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_dio_rf_switch_cfg_state_to_str( const lr20xx_system_dio_rf_switch_cfg_state_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_DIO_RF_SWITCH_CFG_STATE_LOW:
    {
        return ( const char* ) "LOW";
    }

    case LR20XX_SYSTEM_DIO_RF_SWITCH_CFG_STATE_HIGH:
    {
        return ( const char* ) "HIGH";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_lfclk_cfg_to_str( const lr20xx_system_lfclk_cfg_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_LFCLK_RC:
    {
        return ( const char* ) "RC";
    }

    // case LR20XX_SYSTEM_LFCLK_XTAL:
    // {
    //     return ( const char* ) "XTAL";
    // }

    case LR20XX_SYSTEM_LFCLK_EXT:
    {
        return ( const char* ) "EXT";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_tcxo_supply_voltage_to_str( const lr20xx_system_tcxo_supply_voltage_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_TCXO_CTRL_1_6V:
    {
        return ( const char* ) "1_6V";
    }

    case LR20XX_SYSTEM_TCXO_CTRL_1_7V:
    {
        return ( const char* ) "1_7V";
    }

    case LR20XX_SYSTEM_TCXO_CTRL_1_8V:
    {
        return ( const char* ) "1_8V";
    }

    case LR20XX_SYSTEM_TCXO_CTRL_2_2V:
    {
        return ( const char* ) "2_2V";
    }

    case LR20XX_SYSTEM_TCXO_CTRL_2_4V:
    {
        return ( const char* ) "2_4V";
    }

    case LR20XX_SYSTEM_TCXO_CTRL_2_7V:
    {
        return ( const char* ) "2_7V";
    }

    case LR20XX_SYSTEM_TCXO_CTRL_3_0V:
    {
        return ( const char* ) "3_0V";
    }

    case LR20XX_SYSTEM_TCXO_CTRL_3_3V:
    {
        return ( const char* ) "3_3V";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_reg_mode_to_str( const lr20xx_system_reg_mode_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_REG_MODE_LDO:
    {
        return ( const char* ) "LDO";
    }

    case LR20XX_SYSTEM_REG_MODE_DCDC:
    {
        return ( const char* ) "DCDC";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_value_format_to_str( const lr20xx_system_value_format_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_VALUE_FORMAT_RAW:
    {
        return ( const char* ) "RAW";
    }

    case LR20XX_SYSTEM_VALUE_FORMAT_UNIT:
    {
        return ( const char* ) "UNIT";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_meas_res_to_str( const lr20xx_system_meas_res_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_MEAS_RES_8_BITS:
    {
        return ( const char* ) "8_BITS";
    }

    case LR20XX_SYSTEM_MEAS_RES_9_BITS:
    {
        return ( const char* ) "9_BITS";
    }

    case LR20XX_SYSTEM_MEAS_RES_10_BITS:
    {
        return ( const char* ) "10_BITS";
    }

    case LR20XX_SYSTEM_MEAS_RES_11_BITS:
    {
        return ( const char* ) "11_BITS";
    }

    case LR20XX_SYSTEM_MEAS_RES_12_BITS:
    {
        return ( const char* ) "12_BITS";
    }

    case LR20XX_SYSTEM_MEAS_RES_13_BITS:
    {
        return ( const char* ) "13_BITS";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_temp_src_to_str( const lr20xx_system_temp_src_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_TEMP_SRC_VBE:
    {
        return ( const char* ) "VBE";
    }

    case LR20XX_SYSTEM_TEMP_SRC_XOSC:
    {
        return ( const char* ) "XOSC";
    }

    case LR20XX_SYSTEM_TEMP_SRC_NTC:
    {
        return ( const char* ) "NTC";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_random_entropy_source_to_str( const lr20xx_system_random_entropy_source_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_RANDOM_ENTROPY_SOURCE_PLL:
    {
        return ( const char* ) "PLL";
    }

    case LR20XX_SYSTEM_RANDOM_ENTROPY_SOURCE_ADC:
    {
        return ( const char* ) "ADC";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_standby_mode_to_str( const lr20xx_system_standby_mode_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_STANDBY_MODE_RC:
    {
        return ( const char* ) "RC";
    }

    case LR20XX_SYSTEM_STANDBY_MODE_XOSC:
    {
        return ( const char* ) "XOSC";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_hf_clk_scaling_to_str( const lr20xx_system_hf_clk_scaling_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_HF_CLK_SCALING_32_MHZ:
    {
        return ( const char* ) "32_MHZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_16_MHZ:
    {
        return ( const char* ) "16_MHZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_8_MHZ:
    {
        return ( const char* ) "8_MHZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_4_MHZ:
    {
        return ( const char* ) "4_MHZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_2_MHZ:
    {
        return ( const char* ) "2_MHZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_1_MHZ:
    {
        return ( const char* ) "1_MHZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_500_KHZ:
    {
        return ( const char* ) "500_KHZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_250_KHZ:
    {
        return ( const char* ) "250_KHZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_125_KHZ:
    {
        return ( const char* ) "125_KHZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_62500_HZ:
    {
        return ( const char* ) "62500_HZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_31250_HZ:
    {
        return ( const char* ) "31250_HZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_15625_HZ:
    {
        return ( const char* ) "15625_HZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_7813_HZ:
    {
        return ( const char* ) "7813_HZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_3906_HZ:
    {
        return ( const char* ) "3906_HZ";
    }

    case LR20XX_SYSTEM_HF_CLK_SCALING_1953_HZ:
    {
        return ( const char* ) "1953_HZ";
    }

    // case LR20XX_SYSTEM_HF_CLK_SCALING_977_MHZ:
    // {
    //     return ( const char* ) "977_MHZ";
    // }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}

const char* lr20xx_system_lbd_trim_to_str( const lr20xx_system_lbd_trim_t value )
{
    switch( value )
    {
    case LR20XX_SYSTEM_LBD_TRIM_1_60_V:
    {
        return ( const char* ) "1_60_V";
    }

    case LR20XX_SYSTEM_LBD_TRIM_1_67_V:
    {
        return ( const char* ) "1_67_V";
    }

    case LR20XX_SYSTEM_LBD_TRIM_1_74_V:
    {
        return ( const char* ) "1_74_V";
    }

    case LR20XX_SYSTEM_LBD_TRIM_1_80_V:
    {
        return ( const char* ) "1_80_V";
    }

    case LR20XX_SYSTEM_LBD_TRIM_1_88_V:
    {
        return ( const char* ) "1_88_V";
    }

    case LR20XX_SYSTEM_LBD_TRIM_1_95_V:
    {
        return ( const char* ) "1_95_V";
    }

    case LR20XX_SYSTEM_LBD_TRIM_2_00_V:
    {
        return ( const char* ) "2_00_V";
    }

    case LR20XX_SYSTEM_LBD_TRIM_2_10_V:
    {
        return ( const char* ) "2_10_V";
    }

    default:
    {
        return ( const char* ) "Unknown";
    }
    }
}
