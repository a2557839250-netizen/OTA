#ifndef __APP_BOOTLOADER_H
#define __APP_BOOTLOADER_H

#include "Int_bootloader.h"


typedef enum
{
    BOOTLOADER_STATUS_INIT,        //初始化
    BOOTLOADER_STATUS_RUN,         //运行
    BOOTLOADER_STATUS_REC_DATA,    //接收数据
    BOOTLOADER_STATUS_CHECK_DATA   //校验数据
}Bootloader_status;

/**
 * @brief 1.初始化bootloader => 打印日志启动
 *
 */
void App_bootloader_init(void);

/**
 * @brief 2.等待用户传输确认
 *
 */
void App_bootloader_run(void);

/**
 * @brief 3.4.5.接收数据
 *
 */
void App_bootloader_rec_data(void);

/**
 * @brief 6.已经传输完成 校验数据
 *  uint8_t: 0 校验通过 1 校验失败
 */
uint8_t App_bootloader_check_data(void);

/**
 * @brief 在main方法的while循环中调用
 * 
 */
void App_bootloader_work(void);

#endif // !__APP_BOOTLOADER_H
