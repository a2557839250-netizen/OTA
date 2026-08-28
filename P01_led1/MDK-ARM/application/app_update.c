#include "app_update.h"

uint8_t uart_rec_buff[BOOTLOADER_UART_REC_BUFF_LEN] = {0};

Update_State_t update_state = UPDATE_IDLE;

// 声明一个能够容纳整个程序的静态缓存  BSS断里面  SRAM空间
uint8_t app_data_buff[APP_DATA_MAX_LEN] = {0};

extern llcc68_handle_t gs_handle;
// 接收程序的长度
uint16_t lora_rec_msg_len = 0;
// 记录当前一次接收的时间
uint32_t lora_rec_time = 0;

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    // 串口接收到了数据 -> 如果是cmd
    if ((huart->Instance == USART1) && (update_state == UPDATE_IDLE))
    {
        // 校验数据 => cmd  => 让开发板给网关发送更新指令  使用CAN发送
        if (strstr((char *)uart_rec_buff, "cmd"))
        {
            update_state = UPDATE_RECV_SEND_CMD;
            // 添加回滚逻辑 => 如果校验失败  还能重新接收cmd
            //  清空掉初始化串口使用之前的所有问题
            __HAL_UART_CLEAR_OREFLAG(&huart1);
            __HAL_UART_CLEAR_IDLEFLAG(&huart1);
            // 带有中断的串口接收函数
            // 少一个参数 => 超时时间  因为IT带中断的函数方法是异步执行的
            HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart_rec_buff, BOOTLOADER_UART_REC_BUFF_LEN);
        }
    }
}

static void App_run(void)
{
    // 熄灭LED3 点亮LED1
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
    HAL_Delay(200);
    // 熄灭LED1 点亮LED2
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
    HAL_Delay(200);
    // 熄灭LED2 点亮LED3
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
    HAL_Delay(200);
}

/**
 * @brief 接收串口数据  =>  收到更新标记 => 发送CAN的更新指令
 *
 */
void App_update_init(void)
{
    // 启动串口的接收程序
    //  清空掉初始化串口使用之前的所有问题
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    // 带有中断的串口接收函数
    // 少一个参数 => 超时时间  因为IT带中断的函数方法是异步执行的
    HAL_UARTEx_ReceiveToIdle_IT(&huart1, uart_rec_buff, BOOTLOADER_UART_REC_BUFF_LEN);
    // 初始化lora
    llcc68_lora_init();
}

/**
 * @brief 使用CAN发送更新指令
 *
 */
void App_update_send_update_cmd(void)
{
    // 使用lora发送命令
    llcc68_lora_send(APP_UPDATE_CMD, sizeof(APP_UPDATE_CMD));
    // 更新命令已经发送  修改状态外接收程序
    update_state = UPDATE_RECV_DATA;
}

/**
 * @brief CAN接收程序数据  保存到W25Q32
 *
 */
void App_update_receive_app_data(void)
{
    // 使用lora接收程序 一次性接收256字节
    uint8_t res = llcc68_lora_receive();
    if (res == 0)
    {
        lora_rec_time = HAL_GetTick();
        // 将gs_handle中的数据保存到app_data_buff中
        memcpy(app_data_buff + lora_rec_msg_len, gs_handle.receive_buf, gs_handle.receive_buf_len);
        lora_rec_msg_len += gs_handle.receive_buf_len;
        memset(gs_handle.receive_buf, 0, gs_handle.receive_buf_len);
        gs_handle.receive_buf_len = 0;
    }

    // 判断接收完成
    if (lora_rec_time != 0 && (lora_rec_time + 3000 < HAL_GetTick()))
    {
        // 已经断开发送数据3s
        // 打印接收长度
        printf("can_rec_msg_len:%d\n", lora_rec_msg_len);
        update_state = UPDATE_RECV_CHECK_DATA;
    }
}

static uint32_t App_crc_cal(uint8_t *data, uint16_t len)
{
    uint32_t *p_data = (uint32_t *)data;
    uint32_t word_count = (len + 3) / 4;

    // 复位crc
    __HAL_CRC_DR_RESET(&hcrc);

    uint32_t crc_val = HAL_CRC_Calculate(&hcrc, p_data, word_count);
    return crc_val;
}

/**
 * @brief 添加校验逻辑
 *
 */
void App_update_check_data(void)
{
    // 接收crc校验的值
    uint8_t res = llcc68_lora_receive();
    if (res == 0)
    {
        uint32_t rec_crc_val = gs_handle.receive_buf[0] | gs_handle.receive_buf[1] << 8 | gs_handle.receive_buf[2] << 16 | gs_handle.receive_buf[3] << 24;
        uint32_t cru_crc_val = App_crc_cal(app_data_buff, lora_rec_msg_len);
        if (rec_crc_val == cru_crc_val)
        {
            // 校验通过
            printf("crc check pass\r\n");
            update_state = UPDATE_RECV_BOOT_UPDATE;
        }
        else
        {
            // 校验没通过  => 回滚到idle状态
            printf("crc check fail\r\n");
            // 清空缓存和状态
            memset(app_data_buff, 0, APP_DATA_MAX_LEN);
            lora_rec_msg_len = 0;
            lora_rec_time = 0;
            // 回滚到发送更新指令
            update_state = UPDATE_IDLE;
        }
        // 清空lora的缓存
        memset(gs_handle.receive_buf, 0, gs_handle.receive_buf_len);
        gs_handle.receive_buf_len = 0;
    }
}

uint8_t w25q32_write_buff[8] = {0};
/**
 * @brief 修改在W24C02中的更新标志位
 *
 */
void App_update_change_boot_mode(void)
{
    // 1. 将程序写入到外置flash  w25q32
    // 1.1 擦除正确的区域和足够的空间
    uint16_t sector_erase_count = (lora_rec_msg_len / 4096) + 2;

    for (uint8_t i = 0; i < sector_erase_count; i++)
    {
        Int_w25q32_erase_sector(0, i);
    }

    // 1.2 写入元数据
    w25q32_write_buff[0] = (FLASH_APP_ADDR & 0xff);
    w25q32_write_buff[1] = ((FLASH_APP_ADDR >> 8) & 0xff);
    w25q32_write_buff[2] = ((FLASH_APP_ADDR >> 16) & 0xff);
    w25q32_write_buff[3] = ((FLASH_APP_ADDR >> 24) & 0xff);
    w25q32_write_buff[4] = ((lora_rec_msg_len) & 0xff);
    w25q32_write_buff[5] = ((lora_rec_msg_len >> 8) & 0xff);
    w25q32_write_buff[6] = ((lora_rec_msg_len >> 16) & 0xff);
    w25q32_write_buff[7] = ((lora_rec_msg_len >> 24) & 0xff);
    Int_w25q32_write_data_with_32addr(FLASH_META_ADDR, w25q32_write_buff, 8);
    uint16_t write_len = 0;
    uint16_t write_tmp_len = 0;
    // 1.3 按照页 将程序写入到w25q32中
    while (write_len < lora_rec_msg_len)
    {
        // 剩下的长度是否超过1页
        if (lora_rec_msg_len - write_len > 256)
        {
            write_tmp_len = 256;
        }
        else
        {
            write_tmp_len = lora_rec_msg_len - write_len;
        }
        Int_w25q32_write_data_with_32addr(FLASH_APP_ADDR + write_len, app_data_buff + write_len, write_tmp_len);
        write_len += write_tmp_len;
    }

    // 2. 修改w24c02的更新标志位  秘钥高8位在前
    uint8_t eeprom_buff[3] = {0};
    eeprom_buff[0] = BOOT_UPDATE;
    eeprom_buff[1] = (CHECK_KEY >> 8) & 0xff;
    eeprom_buff[2] = CHECK_KEY & 0xff;

    Int_w24c02_write_bytes(CHECK_UPDATE_ADDR, eeprom_buff, 3);

    // 修改状态
    update_state = UPDATE_END;
}

/**
 * @brief 循环调用 执行状态机逻辑
 *
 */
void App_update_work(void)
{
    switch (update_state)
    {
    case UPDATE_IDLE:
        // 只有不需要进行更新程序的时候  才会运行程序之前的功能
        App_run();
        break;
    case UPDATE_RECV_SEND_CMD:
        printf("recv cmd\r\n");
        printf("send cmd\r\n");
        App_update_send_update_cmd();
        // 在接受数据之前  清空缓冲区
        memset(app_data_buff, 0, APP_DATA_MAX_LEN);
        lora_rec_msg_len = 0;
        break;
    case UPDATE_RECV_DATA:
        App_update_receive_app_data();
        break;
    case UPDATE_RECV_CHECK_DATA:
        App_update_check_data();
        break;
    case UPDATE_RECV_BOOT_UPDATE:
        App_update_change_boot_mode();
        break;
    case UPDATE_END:
        // 延时 => 重启
        HAL_Delay(1000);
        HAL_NVIC_SystemReset();
        break;
    default:
        break;
    }
}
