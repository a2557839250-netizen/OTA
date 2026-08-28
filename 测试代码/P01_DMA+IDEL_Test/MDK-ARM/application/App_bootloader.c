#include "App_bootloader.h"
#include "usart.h"

uint8_t app_rec_start_buff[64] = {0};
uint16_t app_rec_start_len = 0;

// 记录接收程序的总长度
uint32_t app_rec_total_len = 0;

// 发送完成的标签
uint8_t flag = 0;
// 记录上次接收的时间
extern uint32_t last_rec_time;
// 记录接收实际数据的长度
extern uint16_t uart_rec_full_len;

// 记录当前应用层的状态
Bootloader_status boot_status = BOOTLOADER_STATUS_INIT;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)//外部中断回调函数
{
    if (GPIO_Pin == KEY1_Pin)
    {
        flag = 1;
    }
}

/**
 * @brief 初始化bootloader => 打印日志启动
 *
 */
void App_bootloader_init(void)
{
    printf("bootloader start\r\n");
    printf("wait user send data\r\n");
    printf("send 'start:len' to start\r\n");
    boot_status = BOOTLOADER_STATUS_INIT;
}

/**
 * @brief 等待用户传输确认
 * 如果发送的指令不对  不需要重启程序  重新发送start:len启动命令即可
 *
 */
void App_bootloader_run(void)
{
    // 使用非中断方式接收（会一直读） => 区分接收程序
    // 挂起等待接收  一直等待接收到buff满 或者 收到idle空闲帧
    HAL_UARTEx_ReceiveToIdle(&huart1, app_rec_start_buff, 64, &app_rec_start_len, 0xffffff);
    if (app_rec_start_len > 0)//收到数据
    {
        // 判断数据中是否包含start:len
        char *start_str = strstr((char *)app_rec_start_buff, "start:");//查找字符串
        if (start_str != NULL)
        {
            // 保存len的值  全称是 ASCII to Integer（将字符串转换为整型数）
            app_rec_total_len = atoi((char *)start_str + 6);//把"start:"后面的数字返回len中
            if (app_rec_total_len > 0)
            {
                printf("app len: %d\r\n", app_rec_total_len);
                // 修改状态到下一个阶段
                boot_status = BOOTLOADER_STATUS_RUN;    //接收数据              
            }
            else
            {
                printf("len error\r\n");
            }
        }
        else
        {
            printf("data error\r\n");
        }
    }
}

/**
 * @brief 接收数据
 *
 */
void App_bootloader_rec_data(void)
{
    // 主循环把环形缓冲区中新到的数据写入flash（中断里不写flash，避免丢数据）
    Int_bootloader_flash_write();

    // 优先判断：已收满约定的长度 => 直接进入校验，不必干等2s
    if (uart_rec_full_len >= app_rec_total_len)
    {
        boot_status = BOOTLOADER_STATUS_CHECK_DATA;
        return;
    }
    // 接收完成之后 修改状态为check_data
    //(1) 软件方式 从接收到一次程序开始算   从idle空闲帧开始算等待2s
    //    且已收到至少一帧数据（last_rec_time != 0）
    if (last_rec_time != 0 && (HAL_GetTick() - last_rec_time > 2000))//这一次的时间减去上一次的时间大于2s
    {
        // 已经2s中没有接收到数据了
        boot_status = BOOTLOADER_STATUS_CHECK_DATA;
    }
}

/**
 * @brief 已经传输完成 校验数据
 *  uint8_t: 0 校验通过 1 校验失败
 */
uint8_t App_bootloader_check_data(void)
{
    // 打印实际接收长度，便于和发送长度对比、判断是否有数据遗漏
    printf("rec total len:%d (expect:%d)\r\n", uart_rec_full_len, app_rec_total_len);

    if (uart_rec_full_len == app_rec_total_len)
    {
        // 长度一致，回读flash校验数据准确性
        printf("app rec ok\r\n");

        // 1. 计算接收到的bin文件CRC32（与发送方计算的CRC对比，判断是否准确）
        uint32_t crc = Int_bootloader_calc_crc32(APP_START_ADDR, uart_rec_full_len);
        printf("flash crc32: 0x%08X\r\n", (unsigned int)crc);

        // 2. 回读flash前8个字节，直观对比发送内容
        volatile uint8_t *p = (volatile uint8_t *)APP_START_ADDR;
        printf("flash head: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
               p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);

        printf("rec done, wait next\r\n");
        // 接收测试完成，回到INIT等待下一次测试
        boot_status = BOOTLOADER_STATUS_INIT;
        return 0;
    }
    else
    {
        printf("app rec error or timeout\r\n");
    }
    return 1;
}

/**
 * @brief 在main方法的while循环中调用
 *
 */
void App_bootloader_work(void)//状态机机制
{
    switch (boot_status)
    {
    case BOOTLOADER_STATUS_INIT:
        // 等待用户传输确认
        App_bootloader_run();
        break;
    case BOOTLOADER_STATUS_RUN:
        // 复位接收相关全局计数，避免重启后二次传输长度累加
        uart_rec_full_len = 0;
        last_rec_time = 0;
        // 接收数据的准备工作
        // 确认要写入flash => 提前擦除flash页  直接先擦除10页  也就是10k
        Int_bootloader_erase_flash(APP_START_ADDR, 10);
        printf("flash erase ok\n");
        printf("ready to receive app\n");
        boot_status = BOOTLOADER_STATUS_REC_DATA;
        flag = 0;
        Int_bootloader_receive_app();
        break;
    case BOOTLOADER_STATUS_REC_DATA:
        // 等待接收完成
        App_bootloader_rec_data();
        break;
    case BOOTLOADER_STATUS_CHECK_DATA:
        // 检查数据情况
        if (App_bootloader_check_data())
        {
            printf("app rec error,wait next\n");
            // 接收失败，回到INIT等待重新测试
            boot_status = BOOTLOADER_STATUS_INIT;
        }
        break;
    default:
        break;
    }
}
