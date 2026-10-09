/**
 *
 * @license MIT License
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * @file      TouchDrvCST3530.hpp
 * @author    自定义（基于原有逻辑重构）
 * @date      2024-XX-XX
 * @brief     仅适配CST3530触摸芯片，移植Hynitron驱动
 */
#pragma once

#include "REG/CST3530Constants.h"
#include "TouchDrvInterface.hpp"
#include "SensorCommon.tpp"

class TouchDrvCST3530 :
    public TouchDrvInterface,
    public SensorCommon<TouchDrvCST3530>
{
    friend class SensorCommon<TouchDrvCST3530>;
public:
    struct hyn_ts_data *hyn_data = NULL; // 仅声明
    struct hyn_ts_data *hyn_66xxdata = NULL; // 仅声明

#if defined(ARDUINO)
    TouchDrvCST3530(PLATFORM_WIRE_TYPE &w, int sda = DEFAULT_SDA, int scl = DEFAULT_SCL, uint8_t addr = CST3530_MAIN_I2C_ADDR)
    {
        __wire = &w;
        __sda = sda;
        __scl = scl;
        __addr = addr;
    }
#endif

    TouchDrvCST3530()
    {
#if defined(ARDUINO)
        __wire = &Wire;
        __sda = DEFAULT_SDA;
        __scl = DEFAULT_SCL;
#endif
        __addr = CST3530_MAIN_I2C_ADDR;
        __last_touch_time = 0;
        __last_x = 0;
        __last_y = 0;
    }

    ~TouchDrvCST3530()
    {
        deinit();
    }

    // -------------------------- 初始化/反初始化 --------------------------
#if defined(ARDUINO)

    bool begin(PLATFORM_WIRE_TYPE &w,
               uint8_t addr = CST3530_MAIN_I2C_ADDR,
               int sda = DEFAULT_SDA,
               int scl = DEFAULT_SCL)
    {
        return SensorCommon::begin(w, addr, sda, scl);
    }

#elif defined(ESP_PLATFORM) && !defined(ARDUINO)

#if ((ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)) && defined(CONFIG_SENSORLIB_ESP_IDF_NEW_API))
    bool begin(i2c_master_bus_handle_t i2c_dev_bus_handle, uint8_t addr)
    {
        return SensorCommon::begin(i2c_dev_bus_handle, addr);
    }
#else
    bool begin(i2c_port_t port_num, uint8_t addr, int sda, int scl)
    {
        return SensorCommon::begin(port_num, addr, sda, scl);
    }
#endif //ESP_IDF_VERSION

#endif

    bool begin(uint8_t addr, iic_fptr_t readRegCallback, iic_fptr_t writeRegCallback)
    {
        return SensorCommon::begin(addr, readRegCallback, writeRegCallback);
    }


    void deinit()
    {
        // end();
    }

    // -------------------------- 核心触摸读取逻辑 --------------------------
    /**
     * @brief 读取触摸点（移植cst66xx_report）
     * @param x_array 输出X坐标数组
     * @param y_array 输出Y坐标数组
     * @param size 最大读取点数（最多5点）
     * @return 实际触摸点数
     */
    uint8_t getPoint(int16_t *x_array, int16_t *y_array, uint8_t size = 1)
    {
        if (!x_array || !y_array || !size) return 0;


        // 读取触摸数据
        int ret = cst66xx_report();
        uint8_t points = hyn_data->rp_buf.rep_num;  // 先保存，避免后续修改

        if (ret == 0 && points > 0) {  // 只在有效触摸时处理和打印
            // 安全复制到输出数组（支持多点，限制在 size 内）
            uint8_t copy_size = min(size, points);
            for (uint8_t i = 0; i < copy_size; i++) {
                int16_t raw_x = hyn_data->rp_buf.pos_info[i].pos_x;
                int16_t raw_y = hyn_data->rp_buf.pos_info[i].pos_y;

                // 先应用 plat_data 中的配置（CST3530 特定）
                if (hyn_data->plat_data.swap_xy) {
                    int16_t tmp = raw_x;
                    raw_x = raw_y;
                    raw_y = tmp;
                }
                if (hyn_data->plat_data.reverse_x) {
                    raw_x = hyn_data->plat_data.x_resolution - raw_x;
                }
                if (hyn_data->plat_data.reverse_y) {
                    raw_y = hyn_data->plat_data.y_resolution - raw_y;
                }

                // 存入输出数组
                x_array[i] = raw_x;
                y_array[i] = raw_y;
            }

            // 应用框架统一的坐标变换（兼容 SensorLib 全局配置，如 setSwapXY/setMirrorXY）
            updateXY(copy_size, x_array, y_array);

            // 只打印第一个点的 xy（调试用，可注释掉）
            // Serial.printf("ret:%d num:%d xy:(%d,%d)\n", ret, points, x_array[0], y_array[0]);

            hyn_data->rp_buf.report_need = REPORT_NONE;

            return copy_size;  // 返回实际点数（不超过用户请求的 size）
        } else {
            // 无触摸或错误时，清零第一个 pos_info 避免残影
            if (points == 0) {
                hyn_data->rp_buf.pos_info[0].pos_x = 0;
                hyn_data->rp_buf.pos_info[0].pos_y = 0;
            }
        }

        return 0;
    }

    // -------------------------- 复位和GPIO --------------------------
    void reset()
    {
        if (__rst != SENSOR_PIN_NONE) {
            pinMode(__rst, OUTPUT);
            digitalWrite(__rst, LOW);
            delay(100);
            digitalWrite(__rst, HIGH);
            delay(200);
        }
    }

    void setPins(int rst, int irq)
    {
        __rst = rst;
        __irq = irq;
    }

    // -------------------------- 电源模式 --------------------------
    void sleep() override
    {
        if (!__wire)
            return;
        (void)cst66xx_set_workmode(DEEPSLEEP, 0);
    }

    void wakeup()
    {
        reset();
        (void)cst66xx_set_workmode(NOMAL_MODE, 0);
    }

    // -------------------------- 基础信息 --------------------------
    uint8_t getSupportTouchPoint()
    {
        return MAX_POINTS_REPORT;
    }

    uint32_t getChipID(void)
    {
        return 0x53; // CST3530固定
    }

    const char *getModelName()
    {
        return "CST3530";
    }

    bool getResolution(int16_t *x, int16_t *y)
    {
        if (x) *x = __plat_data.x_resolution;
        if (y) *y = __plat_data.y_resolution;
        return true;
    }

    bool isPressed() override
    {
        int16_t x = 0, y = 0;
        uint8_t points = getPoint(&x, &y, 1);  // 调用你已实现的getPoint
        return (points > 0);
    }

    void idle() override
    {
        sleep();  // 你已经有了sleep()，调用它进入低功耗
    }

    void  setGpioCallback(gpio_mode_fptr_t mode_cb,
                          gpio_write_fptr_t write_cb,
                          gpio_read_fptr_t read_cb)
    {
        SensorCommon::setGpioModeCallback(mode_cb);
        SensorCommon::setGpioWriteCallback(write_cb);
        SensorCommon::setGpioReadCallback(read_cb);
    }

    uint16_t cst3530_sum16(int val, uint8_t* buf,uint16_t len)
    {
        uint16_t sum = val;
        while(len--) sum += *buf++;
        return sum;
    }

    int cst66xx_set_workmode(enum work_mode mode,uint8_t enable)
    {
        int ret = 0;
        Serial.printf("set_workmode:%d \n",mode);
        if(FAC_TEST_MODE == mode){
            reset();
            delay(50);
        }
        ret = wrRegister(0xD0000400,4,0,0); //0 lp i2c plu
        delay(1);
        ret = wrRegister(0xD0000400,4,0,0);
        switch (mode) {
            case NOMAL_MODE:
                ret |= wrRegister(0xD0000000, 4, 0, 0);
                ret |= wrRegister(0xD0000C00, 4, 0, 0);
                ret |= wrRegister(0xD0000100, 4, 0, 0);
                break;
            case LP_MODE:
                ret |= wrRegister(0xD0000000, 4, 0, 0);
                ret |= wrRegister(0xD0000400, 4, 0, 0);
                break;
            case DEEPSLEEP:
                /* 关机低功耗：Hynitron 命令序列（与 NOMAL 对称，末条为深度休眠寄存器） */
                ret |= wrRegister(0xD0000C00, 4, 0, 0);
                ret |= wrRegister(CST3530_RESET_REG, 4, 0, 0);
                break;
            default:
                ret = -2;
                break;
        }
        return ret;
    }

    int cst66xx_report(void)
    {
        uint8_t buf[80],i=0;
        uint8_t finger_num = 0,key_num=0,report_typ= 0,key_state=0,key_id = 0,tmp_dat=0;
        int ret = 0,retry = 2;

        while(retry--){ //read point
            ret = wrRegister(0xD0070000,0x80|4,buf,9); 
            report_typ = buf[2];//FF:pos F0:ges E0:prox
            finger_num = buf[3]&0x0f;
            key_num    = ((buf[3]&0xf0)>>4);
            if(finger_num+key_num <= MAX_POINTS_REPORT){
                if(key_num + finger_num > 1){ 
                    ret |= readRegister(0xD0070000, 4, &buf[9],(key_num + finger_num -1)*5);
                }
                if(cst3530_sum16(0x55,&buf[4],(key_num + finger_num)*5) != (buf[0] | buf[1]<<8)){
                    ret = -1;
                }
            }
            else{
                ret = -2;
            }
            if(ret && retry) continue;
            ret |= wrRegister(0xD00002AB,4,buf,0);
            if(ret == 0){
                break;
            }
        }
        if(ret) return ret;

        if((report_typ==0xff)&&((finger_num+key_num)>0)){
            if(key_num){
                key_id    = buf[8]&0x0f;
                key_state = buf[8]>>4;
                hyn_66xxdata->rp_buf.key_id = key_id;
                hyn_66xxdata->rp_buf.key_state = key_state !=0 ? 1:0; 
            }

            if(finger_num){ //pos
                uint16_t index = 0;
                uint8_t touch_down = 0;
                hyn_66xxdata->rp_buf.rep_num = finger_num;
                for(i = 0; i < finger_num; i++){
                    index = (key_num+i)*5;
                    hyn_66xxdata->rp_buf.pos_info[i].pos_id = buf[index+8]&0x0F;
                    hyn_66xxdata->rp_buf.pos_info[i].event =  buf[index+8]>>4;
                    hyn_66xxdata->rp_buf.pos_info[i].pos_x =  buf[index+4] + ((uint16_t)(buf[index+7]&0x0F) <<8); //x is rx direction
                    hyn_66xxdata->rp_buf.pos_info[i].pos_y =  buf[index+5] + ((uint16_t)(buf[index+7]&0xf0) <<4);
                    hyn_66xxdata->rp_buf.pos_info[i].pres_z = buf[index+6];
                    if(hyn_66xxdata->rp_buf.pos_info[i].event){
                        touch_down++;
                    }
                }
                if(0== touch_down){
                    hyn_66xxdata->rp_buf.rep_num = 0;
                }
            }
        }else if(report_typ == 0xF0){
            tmp_dat = buf[8]&0xff;
            if((tmp_dat&0x7F) < sizeof(gest_map_tbl) && gest_map_tbl[tmp_dat] != IDX_NULL){  
                hyn_66xxdata->gesture_id = gest_map_tbl[tmp_dat];
                Serial.printf("gesture_id:%d",tmp_dat);
            }
        }else if(report_typ == 0xE0){//proximity 
            uint8_t state = buf[4] ? PS_FAR_AWAY : PS_NEAR;
            if(hyn_66xxdata->prox_is_enable && hyn_66xxdata->prox_state != state){
                hyn_66xxdata->prox_state =state;
            }
        }
        return ret;
    }

private:
    uint32_t __last_touch_time;
    int16_t __last_x;
    int16_t __last_y;
    hyn_plat_data __plat_data;

    // -------------------------- I2C读写（支持32位地址） --------------------------
    // 写：发送 addr_len 字节的 reg_addr（大端），然后 len 字节数据
    int writeRegister(uint32_t reg_addr, uint8_t addr_len, const uint8_t *data, size_t len) {
        if (!__wire) return -1;
        __wire->beginTransmission(__addr);
        for (int i = addr_len - 1; i >= 0; i--) {  // 大端：高字节先
            __wire->write((reg_addr >> (i * 8)) & 0xFF);
        }
        for (size_t i = 0; i < len; i++) {
            __wire->write(data[i]);
        }
        int ret = __wire->endTransmission();
        if (ret != 0) {
            Serial.printf("write failed: %d\n", ret);
        }
        return (ret == 0) ? 0 : -1;
    }

    // 读：先写 addr_len 字节地址（重复启动），然后读 len 字节
    int readRegister(uint32_t reg_addr, uint8_t addr_len, uint8_t *data, size_t len) {
        if (!__wire) return -1;
        __wire->beginTransmission(__addr);
        for (int i = addr_len - 1; i >= 0; i--) {  // 大端
            __wire->write((reg_addr >> (i * 8)) & 0xFF);
        }
        if (__wire->endTransmission(false) != 0) {  // false: 重复启动
            Serial.println("read: endTransmission(false) failed");
            return -1;
        }
        if (len == 0) return 0;
        __wire->requestFrom(__addr, (uint8_t)len);
        size_t read_count = 0;
        while (__wire->available() && read_count < len) {
            data[read_count++] = __wire->read();
        }
        if (read_count != len) {
            Serial.printf("read: only got %d/%d bytes\n", read_count, len);
            return -1;
        }
        return 0;
    }

    int wrRegister(uint32_t reg_addr, uint8_t reg_len, uint8_t *rbuf, uint16_t rlen) {
        int ret = 0;
        uint8_t wbuf[4] = {0};
        reg_len &= 0x0F;  // 最大 15
        if (reg_len > 4) return -1;  // CST3530 地址 <=4 字节

        // 填充 wbuf（小端，但 writeRegister 会大端发送）
        uint32_t temp_addr = reg_addr;
        for (int i = reg_len - 1; i >= 0; i--) {
            wbuf[i] = temp_addr & 0xFF;
            temp_addr >>= 8;
        }

        // Serial.printf("wrRegister: reg_addr=0x%08X, reg_len=%d, rlen=%d\n", reg_addr, reg_len, rlen);
        // for (int j = 0; j < reg_len; j++) {
        //     Serial.printf("wbuf[%d]=0x%02X ", j, wbuf[j]);
        // }
        // Serial.println();

        // 只写地址（无数据）
        ret = writeRegister(reg_addr, reg_len, nullptr, 0);  // 使用新 writeRegister
        if (ret != 0) return -1;

        if (rlen > 0) {
            // Serial.printf("wrRegister: rlen=%d, reading back data...\n", rlen);
            ret = readRegister(reg_addr, reg_len, rbuf, rlen);  // 已写地址，直接读（注意：reg_addr=0, addr_len=0 表示不重写地址）
            if (ret != 0) return -1;
            // for (int j = 0; j < rlen; j++) {
            //     Serial.printf("rbuf[%d]=0x%02X ", j, rbuf[j]);
            // }
            // Serial.println();
        }
        return 0;
    }

    // -------------------------- 初始化实现（移植touch_init和cst226se_init） --------------------------
    bool initImpl()
    {
        Serial.println("[DEBUG] initImpl started - full chip initialization");

        if (__irq != SENSOR_PIN_NONE) {
            this->setGpioMode(__irq, INPUT);
        }

        reset();

        Serial.printf("[DEBUG] Chip reset done, now updating TP info...\n");

        uint8_t buf[60];
        struct tp_info ic;
        int ret = 0;
        int retry = 5;
        while(--retry){
            //get all config info
            ret |= cst66xx_set_workmode(NOMAL_MODE,1);
            ret |= wrRegister(0xD0030000,0x80|4,buf,50);
            if(ret == 0 && buf[3]==0xCA && buf[2]==0xCA) break; 
            delay(1);
            ret |= wrRegister(0xD0000400,4,buf,0);
        }

        if(ret || retry==0){
            Serial.printf("cst66xx_updata_tpinfo failed");
            return false;
        }

        ic.fw_sensor_txnum = buf[48];
        ic.fw_sensor_rxnum = buf[49];
        ic.fw_key_num = buf[27];
        ic.fw_res_y = (buf[31]<<8)|buf[30];
        ic.fw_res_x = (buf[29]<<8)|buf[28];
        Serial.printf("IC_info tx:%d rx:%d key:%d res-x:%d res-y:%d",ic.fw_sensor_txnum,ic.fw_sensor_rxnum,ic.fw_key_num,ic.fw_res_x,ic.fw_res_y);

        ic.fw_project_id = uint8_tTO32(buf[39],buf[38],buf[37],buf[36]);
        ic.fw_chip_type = uint8_tTO32(buf[3],buf[2],buf[1],buf[0]);
        ic.fw_ver = uint8_tTO32(buf[35],buf[34],buf[33],buf[32]);
        Serial.printf("IC_info fw_project_id:%04x ictype:%04x fw_ver:%04x checksum:%04x",ic.fw_project_id,ic.fw_chip_type,ic.fw_ver,ic.ic_fw_checksum);
        
        hyn_data = new hyn_ts_data();      // 或使用 static struct hyn_ts_data hyn_data_inst; hyn_data = &hyn_data_inst;
        hyn_66xxdata = hyn_data;           // 如果是同一个
        memset(hyn_data, 0, sizeof(hyn_ts_data));  // 清零
        // 从 IC_info 填充
        hyn_data->hw_info = ic;            // 复制 IC 信息
        hyn_data->plat_data.x_resolution = ic.fw_res_x;
        hyn_data->plat_data.y_resolution = ic.fw_res_y;
        hyn_data->plat_data.swap_xy = 0;   // 默认值，根据需要设置
        hyn_data->plat_data.reverse_x = 0;
        hyn_data->plat_data.reverse_y = 0;
        hyn_data->prox_is_enable = 0;      // 默认关闭
        hyn_data->gesture_is_enable = 0;

        cst66xx_set_workmode(NOMAL_MODE,0);// 恢复正常模式

        return true;
    }

    int getReadMaskImpl()
    {
        return -1;
    }
};
