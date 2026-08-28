#ifndef __APP_UPDATE__
#define __APP_UPDATE__

#include "usart.h"

#include "crc.h"
#include "Int_w25q32.h"
#include "Int_w24c02.h"
#include "Int_llcc68.h"
#define BOOTLOADER_UART_REC_BUFF_LEN 32

#define CAN_UPDATE_CMD_ID 0
// ����ָ��
#define APP_UPDATE_CMD "ZGF"
#define APP_UPDATE_CMD_LEN 3

// �洢����״̬��λ��
#define CHECK_UPDATE_ADDR 0x10
// ����״̬��ֵ
#define BOOT_UPDATE 0X01
#define BOOT_NO_UPDATE 0X02
// ����У�����Կ
#define CHECK_KEY_ADDR 0X11
#define CHECK_KEY 0X5A6B

// 16k������ ������������
#define APP_DATA_MAX_LEN 16384

// w25q32 ���Ԫ���ݵĵ�ַ  ��ŵ���0��
#define FLASH_META_ADDR 0x000000

// w25q32 ��ų���ĵ�ַ  ��ŵ���һ��
#define FLASH_APP_ADDR 0x001000

// ����״̬��
typedef enum
{
    UPDATE_IDLE = 0,
    UPDATE_RECV_SEND_CMD,
    UPDATE_RECV_DATA,
    UPDATE_RECV_CHECK_DATA,
    UPDATE_RECV_BOOT_UPDATE,
    UPDATE_END
} Update_State_t;

/**
 * @brief ���մ�������  =>  �յ����±�� => ����CAN�ĸ���ָ��
 *
 */
void App_update_init(void);

/**
 * @brief ʹ��CAN���͸���ָ��
 *
 */
void App_update_send_update_cmd(void);

/**
 * @brief CAN���ճ�������  ���浽W25Q32
 *
 */
void App_update_receive_app_data(void);

/**
 * @brief �޸���W24C02�еĸ��±�־λ
 *
 */
void App_update_change_boot_mode(void);

/**
 * @brief ѭ������ ִ��״̬���߼�
 *
 */
void App_update_work(void);

#endif // __APP_UPDATE__
