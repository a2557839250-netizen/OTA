#ifndef __INT_BOOTLOADER_H
#define __INT_BOOTLOADER_H

#include "usart.h"
#include "stdlib.h"
#include "string.h"
#include <stdio.h>

// DMA环形缓冲区大小（必须2的幂，方便取模）
#define UART_DMA_BUFF_SIZE  2048
#define UART_DMA_BUFF_MASK  (UART_DMA_BUFF_SIZE - 1)

// 程序写入的起始位置 => A区起始位置  假设B区16K 0x4000    A区(512-16)K  0x7C000
#define APP_START_ADDR 0x08004000

// DMA环形缓冲区及读写指针（全局）
extern uint8_t uart_dma_buff[UART_DMA_BUFF_SIZE];
extern volatile uint16_t uart_dma_read_index;  // 读指针（主循环已写入flash的位置）
extern volatile uint16_t uart_dma_write_index; // 写指针（DMA当前写入位置）

// 外部可获取的接收总长度
extern uint16_t uart_rec_full_len;

/**
 * @brief 串口接收 => 准备接收A程序（DMA环形 + IDLE中断方式）
 * 
 */
void Int_bootloader_receive_app(void);

/**
 * @brief USART1 空闲中断处理（在stm32f1xx_it.c的USART1_IRQHandler中调用）
 *        更新DMA写指针和接收总长度，数据写入flash在主循环完成
 */
void Int_bootloader_uart_idle_process(void);

/**
 * @brief 主循环调用：从环形缓冲区取出新数据写入flash
 *        返回值：本次写入的字节数
 */
uint16_t Int_bootloader_flash_write(void);

/**
 * @brief 外部可调用 提前擦除flash空间
 * 
 * @param page_addr 
 * @param pages 
 */
void Int_bootloader_erase_flash(uint32_t page_addr,uint16_t pages);

/**
 * @brief 计算flash中bin文件数据的CRC32（用于与发送方对比，验证数据准确性）
 * 
 * @param addr   起始地址
 * @param len    数据长度
 * @return uint32_t CRC32校验值
 */
uint32_t Int_bootloader_calc_crc32(uint32_t addr, uint32_t len);

#endif
