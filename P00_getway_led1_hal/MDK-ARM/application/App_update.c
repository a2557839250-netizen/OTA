#include "App_update.h"

// 标记上位机当前的状态
APP_UPDATE_STATE_E g_app_update_state = APP_UPDATE_WAIT_CMD;

// 记录程序的总长度
extern uint16_t uart_rec_full_len;
// 已经发送的数据长度
uint16_t update_data_len = 0;
// lora发送数据的缓存
uint8_t update_buff[256] = {0};

// 使用lora接收消息的结构体
extern llcc68_handle_t gs_handle;

/**
 * @brief 初始化上位机更新程序
 *
 */
void App_update_init(void)
{
    printf("App_update_init\r\n");
    g_app_update_state = APP_UPDATE_WAIT_CMD;
    // Int_CAN_init();

    llcc68_lora_init();
    printf("App_update_wait_cmd\r\n");

    // 如果已经烧录好更新的程序  需要手动填写一下更新程序的长度
    if (uart_rec_full_len == 0)
    {
        uart_rec_full_len = 8316;
    }
}

/**
 * @brief 等待开发板发送更新请求
 *
 */
void App_update_wait_cmd(void)
{
    // 替换成lora通信
    uint8_t res = llcc68_lora_receive();
    if (res == 0)
    {
        // 收到数据
        if (strstr((char *)gs_handle.receive_buf, APP_UPDATE_CMD) != NULL)
        {
            g_app_update_state = APP_UPDATE_SEND_APP;
            printf("App_update_send_app\r\n");
            // 使用玩receive_buff之后清空缓存
            memset(gs_handle.receive_buf, 0, 256);
            gs_handle.receive_buf_len = 0;
            // 可选 => 接收到更新指令之后 延时一会再发送程序
            HAL_Delay(100);
        }
    }
}

static uint32_t App_crc_cal(uint32_t flash_addr, uint16_t len)
{
    // 将flash地址转换为32位指针
    uint32_t *p_data = (uint32_t *)flash_addr;
    uint32_t word_count = (len + 3) / 4;

    // 复位crc
    __HAL_CRC_DR_RESET(&hcrc);

    uint32_t crc_val = HAL_CRC_Calculate(&hcrc, p_data, word_count);
    return crc_val;
}

/**
 * @brief 发送更新程序给开发板
 *
 */
void App_update_send_app(void)
{
    if (update_data_len < uart_rec_full_len)
    {
        uint16_t send_len = 0;
        // 剩下的长度够不够100字节
        if (uart_rec_full_len - update_data_len >= 100)
        {
            send_len = 100;
        }
        else
        {
            send_len = uart_rec_full_len - update_data_len;
        }

        for (uint16_t i = 0; i < send_len; i++)
        {
            update_buff[i] = *(volatile uint8_t *)(APP_START_ADDR + update_data_len + i);
        }
        update_data_len += send_len;
        // 发送lora消息 => 一次发送100字节
        llcc68_lora_send(update_buff, send_len);
        // 添加延迟 保证无线通信的稳定性
        HAL_Delay(350);
    }
    else
    {
        // 已经发送完毕
        printf("App_update_send_app_finish\r\n");
        update_data_len = 0;
        g_app_update_state = APP_UPDATE_WAIT_CMD;

        // 发送CRC校验
        HAL_Delay(3100);
        uint32_t crc_value = App_crc_cal(APP_START_ADDR, uart_rec_full_len);
        memset(update_buff, 0, 8);
        // 将crc_value转换为4字节的数组
        update_buff[0] = crc_value & 0xFF;
        update_buff[1] = (crc_value >> 8) & 0xFF;
        update_buff[2] = (crc_value >> 16) & 0xFF;
        update_buff[3] = (crc_value >> 24) & 0xFF;
        // 替换成lora发送
        llcc68_lora_send(update_buff, 4);
    }
}

/**
 * @brief 循环程序
 *
 */
void App_update_work(void)
{
    switch (g_app_update_state)
    {
    case APP_UPDATE_WAIT_CMD:
        /* code */
        App_update_wait_cmd();
        break;
    case APP_UPDATE_SEND_APP:
        /* code */
        App_update_send_app();
        break;
    default:
        break;
    }
}
