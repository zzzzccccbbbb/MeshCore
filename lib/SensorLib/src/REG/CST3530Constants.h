/**
 * @file CST3530Constants.h
 * @brief CST3530 触摸屏芯片寄存器及事件常量定义
 * @note 兼容 CST3XX 系列通用定义，与 FT6X36 寄存器地址风格对齐
 */
#pragma once


// ==================== I2C地址 ====================
#define CST3530_BOOT_I2C_ADDR    0x5A    // boot模式I2C地址
#define CST3530_MAIN_I2C_ADDR    0x58    // 工作模式I2C地址

// ==================== 寄存器地址 ====================
// 固件相关地址
#define CST3530_MODULE_ID_ADDR   0xA400  // 模块ID地址
#define CST3530_PARTNUM_ADDR     0xFF10  // 批次号地址
// 数据上报相关
#define CST3530_REPORT_REG       0xD0070000 // 触摸数据上报寄存器
#define CST3530_CMD_REG_BASE     0xD0000000 // 命令寄存器基地址
#define CST3530_WORK_MODE_REG    (CST3530_CMD_REG_BASE + 0x0400) // 工作模式寄存器
#define CST3530_IRQ_CLEAR_REG    (CST3530_CMD_REG_BASE + 0x02AB) // 中断清除寄存器
#define CST3530_PROX_CTRL_REG    (CST3530_CMD_REG_BASE + 0x04B0) // 接近感应控制寄存器
#define CST3530_RESET_REG        (CST3530_CMD_REG_BASE + 0x22AB) // 深度休眠/复位相关
#define CST3530_THRESHOLD_REG    0xD00004A0  // 阈值寄存器（示例，实际根据芯片调整）
#define CST3530_INT_MODE_REG     0xD00004B1  // 中断模式寄存器（示例）

// ==================== 固件常量 ====================
#define CST3530_BIN_SIZE         (40 * 1024) // 固件大小
#define CST3530_MODULE_ID        0xCACA220E  // CST3530对应的module_id
#define CST3530_PART_NO          0xFFFFFFFF  // CST3530批次号（默认不校验）

// ==================== 上报类型 ====================
#define CST3530_REPORT_TYPE_POS  0xFF    // 坐标/按键上报
#define CST3530_REPORT_TYPE_GES  0xF0    // 手势上报
#define CST3530_REPORT_TYPE_PROX 0xE0    // 接近感应上报

// ==================== 最大上报点数 ====================
#define MAX_POINTS_REPORT  5    // 最大支持的触摸点数
#define CST3530_REPORT_BUF_LEN 80 // 数据上报缓冲区长度

// ==================== 按键/坐标定义 ====================
#define CST3530_KEY_STATE_PRESS   1 // 按键按下
#define CST3530_KEY_STATE_RELEASE 0 // 按键释放

#define PS_FAR_AWAY    1
#define PS_NEAR        0


enum ges_idx{
    IDX_U = 0,
    IDX_UP,
    IDX_DOWN,
    IDX_LEFT,
    IDX_RIGHT,
    IDX_O,
    IDX_e,
    IDX_M,
    IDX_L,
    IDX_W,
    IDX_S,
    IDX_V,
    IDX_C,
    IDX_Z,
    IDX_POWER,
    IDX_NULL = 0xFF,
};

static const uint8_t gest_map_tbl[] = { 
    IDX_POWER,   //GESTURE_LABEL_CLICK, 
    IDX_POWER,  //GESTURE_LABEL_CLICK2, 
    IDX_UP,     //GESTURE_LABEL_TOP, 
    IDX_DOWN,   //GESTURE_LABEL_BOTTOM, 
    IDX_LEFT,   //GESTURE_LABEL_LEFT, 
    IDX_RIGHT,  //GESTURE_LABEL_RIGHT,
    IDX_C,      //GESTURE_LABEL_C,
    IDX_e,      //GESTURE_LABEL_E,
    IDX_V,      //GESTURE_LABEL_v,
    IDX_NULL,   //GESTURE_LABEL_^,
    IDX_NULL,   //GESTURE_LABEL_>,
    IDX_NULL,   //GESTURE_LABEL_<,
    IDX_M,      //GESTURE_LABEL_M,
    IDX_W,      //GESTURE_LABEL_W,
    IDX_O,      //GESTURE_LABEL_O,
    IDX_S,      //GESTURE_LABEL_S,
    IDX_Z,      //GESTURE_LABEL_Z
};

enum work_mode{
    NOMAL_MODE = 0,
    GESTURE_MODE = 1,
    LP_MODE = 2,
    DEEPSLEEP = 3,
    DIFF_MODE = 4,
    RAWDATA_MODE = 5,
    BASELINE_MODE = 6,
    CALIBRATE_MODE = 7,
    FAC_TEST_MODE = 8,
    ENTER_BOOT_MODE = 0xCA,
};

enum report_typ{
    REPORT_NONE = 0,
    REPORT_POS = 0x01,
    REPORT_KEY = 0x02,
    REPORT_GES = 0x04,
    REPORT_PROX = 0x08
};

struct hyn_plat_data {
    int reset_gpio;
    int irq_gpio;

    uint32_t x_resolution;
    uint32_t y_resolution;
    int swap_xy;
    int reverse_x;
    int reverse_y;

    int max_touch_num;
    int key_num;
    uint32_t  key_x_coords[8]; // max support 8 keys
    uint32_t  key_y_coords;
    uint32_t  key_code[8];
};

struct ts_frame{
    uint8_t rep_num;
    enum report_typ report_need;

    uint8_t key_id;
    uint8_t key_state;
    struct {
        uint8_t pos_id;
        uint8_t event;
        uint16_t pos_x;
        uint16_t pos_y;
        uint16_t pres_z;
    }pos_info[MAX_POINTS_REPORT];
};

struct tp_info{
    uint8_t  fw_sensor_txnum;
    uint8_t  fw_sensor_rxnum;
    uint8_t  fw_key_num;
    uint8_t  reserve;
    uint16_t fw_res_y;
    uint16_t fw_res_x;
    uint32_t fw_boot_time;
    uint32_t fw_project_id;
    uint32_t fw_chip_type;
    uint32_t fw_ver;
    uint32_t ic_fw_checksum;
    uint32_t fw_module_id;
};

struct hyn_ts_data {
    int gpio_irq;
    int esd_fail_cnt;
    uint32_t esd_last_value;
    enum work_mode work_mode;

    int power_is_on;
    uint8_t  hyn_irq_flg;
    struct hyn_plat_data plat_data;
    struct tp_info hw_info;
    struct ts_frame rp_buf;

    int boot_is_pass;
   
    uint8_t host_cmd_save[16];

    uint8_t log_level;
    uint8_t prox_is_enable;
    uint8_t prox_state;
    
    uint8_t gesture_is_enable;
    uint8_t gesture_id;
};

// 宏定义
#define uint8_tTO16(high, low) ((((high) & 0xFF) << 8) | ((low) & 0xFF))
#define uint8_tTO32(x1,x2,x3,x4) ((((x1)&0xFF)<<24)|(((x2)&0xFF)<<16)|(((x3)&0xFF)<<8)|((x4)&0xFF))


