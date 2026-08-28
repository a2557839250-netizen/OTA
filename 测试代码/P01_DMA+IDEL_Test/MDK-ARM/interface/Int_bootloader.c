#include "Int_bootloader.h"

/**
 * @brief 健壮性增强版：DMA环形缓冲区 + IDLE中断 + 主循环写flash
 *
 * 设计要点：
 * 1. DMA环形模式持续接收，任意大小bin文件都不会截断
 * 2. IDLE中断只更新DMA写指针和接收总长度，不写flash、不printf（避免中断耗时丢数据）
 * 3. 主循环 Int_bootloader_flash_write() 从环形缓冲区取数据写flash
 * 4. 超时/收满判定由应用层状态机完成，失败自动回到INIT等待重传
 */

// DMA环形接收缓冲区（DMA直接把数据搬到这里）
uint8_t uart_dma_buff[UART_DMA_BUFF_SIZE] = {0};
volatile uint16_t uart_dma_read_index = 0;  // 读指针（已写入flash的位置）
volatile uint16_t uart_dma_write_index = 0; // 写指针（DMA当前写入位置）

uint16_t uart_rec_len = 0;        // 本次需要写入flash的一帧数据长度
uint16_t uart_rec_full_len = 0;   // 数据字节总长度

// 记录当前写入程序的偏移量
uint32_t flash_write_offset = 0;
// 记录当前一次接收数据的时间
uint32_t last_rec_time = 0;
// 末尾可能出现的单独字节
uint8_t last_byte_flag = 0;//上次是否有遗留单字节
uint8_t last_byte = 0;//上次遗留字节的值是多少

// 从环形缓冲区读取相对读指针偏移i处的字节
static uint8_t Int_ring_read_byte(uint16_t i)
{
    return uart_dma_buff[(uart_dma_read_index + i) & UART_DMA_BUFF_MASK];
}

//2.1 擦除需要写入的flash页
static void Int_flash_erase(void)
{
    uint8_t is_erase = 0;    //是否需要擦除
    uint32_t page_addr = 0;  //需要擦除的页地址
    for (uint16_t i = 0; i < uart_rec_len; i++)
    {
        // 读取每一个位置的值
        uint8_t data = *(volatile uint8_t *)(APP_START_ADDR + i + flash_write_offset);//解引用
        if (data != 0xff)
        {
            is_erase = 1;//如果有数据不为0xff 则需要擦除
            // 记录当前需要擦除页的起始地址
            page_addr = (APP_START_ADDR + i + flash_write_offset) - (APP_START_ADDR + i + flash_write_offset) % FLASH_PAGE_SIZE;
            break;
        }
    }
    // 2.2 如果需要擦除  则擦除当前页
    if (is_erase)
    {
        FLASH_EraseInitTypeDef erase_init;
        // 擦除单独页
        erase_init.TypeErase = FLASH_TYPEERASE_PAGES;////页擦除 FLASH_TYPEERASE_MASSERASE 全擦除
        // 擦除第1个bank的页
        erase_init.Banks = FLASH_BANK_1;
        // 擦除页的起始地址
        erase_init.PageAddress = page_addr;
        // 擦除几页
        erase_init.NbPages = 1;
        uint32_t page_error = 0;
        // flash擦除比较耗费性能
        HAL_FLASHEx_Erase(&erase_init, &page_error);//page_error 是用来接收擦除失败时的出错页地址的。
    }
}

//有剩余字节：从环形缓冲区读取，拼接上一次的遗留字节
static void Int_flash_write_with_last(void)
{
    for (uint16_t i = 0; i < uart_rec_len; i += 2)
    {
        uint32_t flash_addr = APP_START_ADDR + i + flash_write_offset;
        uint16_t data16;
        if (i == 0)
        {
            // 拼接上一次的字节
            data16 = last_byte | (Int_ring_read_byte(i) << 8); //这次的字节（高八位）上次的字节（低八位）
        }
        else
        {   //保证之后数据正常传输
            data16 = Int_ring_read_byte(i - 1) | (Int_ring_read_byte(i) << 8);
        }
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, flash_addr, data16);
    }
}

//没有剩余字节：从环形缓冲区读取，正好两个一组
static void Int_flash_write_no_last(void)
{
    // 正好能够写入 => 不再有遗留的字节  0   6
    for (uint16_t i = 0; i < uart_rec_len; i += 2)
    {
        uint32_t flash_addr = APP_START_ADDR + i + flash_write_offset;
        uint16_t data16;

        if (i + 1 < uart_rec_len)
        {
            data16 = Int_ring_read_byte(i) | (Int_ring_read_byte(i + 1) << 8);//低八位和高八位（左移）
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, flash_addr, data16);
        }
    }
}

//2.3 写入数据  写入半字 = 2个字节 = 16位
static void Int_flash_write_halfword(void)
{
    // 本次之后没剩余
    if ((uart_rec_len + last_byte_flag) % 2 == 0)//判断当前写入的数据长度是否是偶数
    {
        if (last_byte_flag)
        {
            // 上次有剩余 数据长度是奇数 5 => 这次需要作为第一个字节写入  1  5 => 写入六个字节
            Int_flash_write_with_last();
            // 2.4 记录偏移量
            flash_write_offset += uart_rec_len + 1;
        }
        else
        {
            // 上次无剩余  数据长度是偶数 => 不再有遗留的字节  0   6 =》 写入六个字节
            Int_flash_write_no_last();
            // 2.4 记录偏移量
            flash_write_offset += uart_rec_len;
        }
        last_byte_flag = 0;
    }
    // 本次之后有剩余
    else
    {
        if (last_byte_flag)
        {
            // 有剩余 数据长度是偶数
            Int_flash_write_with_last();
            // 修改最后剩下的字节
            last_byte = Int_ring_read_byte(uart_rec_len - 1);
            // 2.4 记录偏移量
            flash_write_offset += uart_rec_len;
        }
        else
        {
            // 上次没有遗留字节  这次会留下一个
            Int_flash_write_no_last();

            last_byte = Int_ring_read_byte(uart_rec_len - 1);
            // 2.4 记录偏移量
            flash_write_offset += uart_rec_len - 1;
        }
        last_byte_flag = 1;
    }
}

/**
 * @brief USART1 空闲中断处理（DMA环形方式）
 *        在 USART1_IRQHandler 中检测到 IDLE 标志后调用
 *        只更新DMA写指针和接收总长度，不写flash（写flash在主循环完成）
 */
void Int_bootloader_uart_idle_process(void)
{
    // 读取DMA当前写入位置（DMA已搬运到缓冲区的字节数）
    uart_dma_write_index = UART_DMA_BUFF_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);

    // 计算自上次处理以来新到达的字节数（环形）
    uint16_t new_len = (uint16_t)(uart_dma_write_index - uart_dma_read_index) & UART_DMA_BUFF_MASK;

    if (new_len > 0)
    {
        // 记录最近接收时间
        last_rec_time = HAL_GetTick();
        // 累加接收总长度（实际收到的字节数）
        uart_rec_full_len += new_len;
    }

    // 清空闲标志（必须在IDLE中断处理中清除）
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
}

/**
 * @brief 主循环调用：从环形缓冲区取出新数据写入flash
 *        返回值：本次写入的字节数（0表示无新数据）
 */
uint16_t Int_bootloader_flash_write(void)
{
    // 计算待处理的新数据长度（write - read）
    uart_rec_len = (uint16_t)(uart_dma_write_index - uart_dma_read_index) & UART_DMA_BUFF_MASK;

    if (uart_rec_len == 0)
    {
        return 0;   // 无新数据
    }

    // 将新数据写入flash
    HAL_FLASH_Unlock();
    Int_flash_erase();
    Int_flash_write_halfword();
    HAL_FLASH_Lock();

    // 更新读指针（这一帧数据已消费）
    uart_dma_read_index = uart_dma_write_index;

    return uart_rec_len;
}

/**
 * @brief 1.串口接收 => 准备接收A程序（DMA环形 + IDLE中断方式）
 *
 */
void Int_bootloader_receive_app(void)
{
    // 复位所有接收/写入相关的全局变量，防止重启后二次传输残留旧数据
    uart_rec_full_len = 0;          // 接收总长度
    uart_rec_len = 0;               // 单次接收长度
    flash_write_offset = 0;         // flash写入偏移量
    last_rec_time = 0;              // 上次接收时间
    last_byte_flag = 0;             // 是否遗留单字节
    last_byte = 0;                  // 遗留字节值
    uart_dma_read_index = 0;        // 读指针
    uart_dma_write_index = 0;       // 写指针
    memset(uart_dma_buff, 0, UART_DMA_BUFF_SIZE);

    // 清空掉初始化串口使用之前的所有问题
    __HAL_UART_CLEAR_OREFLAG(&huart1);//过载错误标志位
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);//空闲标志位

    // 开启DMA环形接收（DMA自动把数据搬到缓冲区，无需CPU干预）
    HAL_UART_Receive_DMA(&huart1, uart_dma_buff, UART_DMA_BUFF_SIZE);
}

/**
 * @brief 外部可调用 提前擦除flash空间
 *
 * @param page_addr
 * @param pages
 */
void Int_bootloader_erase_flash(uint32_t page_addr, uint16_t pages)
{
    // 解锁flash
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase_init;
    // 擦除单独页
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    // 擦除第1个bank的页
    erase_init.Banks = FLASH_BANK_1;
    // 擦除页的起始地址
    erase_init.PageAddress = page_addr;
    // 擦除几页
    erase_init.NbPages = pages;
    uint32_t page_error = 0;
    // flash擦除比较耗费性能
    HAL_FLASHEx_Erase(&erase_init, &page_error);
    // 加锁flash
    HAL_FLASH_Lock();
}

// CRC32标准多项式（IEEE 802.3）
#define CRC32_POLY 0xEDB88320UL

/**
 * @brief 计算flash中bin文件数据的CRC32（用于与发送方对比，验证数据准确性）
 *
 * @param addr   起始地址
 * @param len    数据长度
 * @return uint32_t CRC32校验值
 */
uint32_t Int_bootloader_calc_crc32(uint32_t addr, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    volatile uint8_t *p = (volatile uint8_t *)addr;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= p[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 1)
            {
                crc = (crc >> 1) ^ CRC32_POLY;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}
