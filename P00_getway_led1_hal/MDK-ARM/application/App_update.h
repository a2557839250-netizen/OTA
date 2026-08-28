#ifndef __APP_UPDATE__
#define __APP_UPDATE__

#include "Int_can.h"
#include "usart.h"
#include "Int_bootloader.h"
#include "crc.h"
#include "Int_llcc68.h"

#define APP_UPDATE_CMD "ZGF"
#define APP_UPDATE_CMD_1 'Z'
#define APP_UPDATE_CMD_2 'G'
#define APP_UPDATE_CMD_3 'F'

#define APP_UPDATE_CMD_ID 1

typedef enum
{
    APP_UPDATE_WAIT_CMD = 0,
    APP_UPDATE_SEND_APP,
} APP_UPDATE_STATE_E;

/**
 * @brief ��ʼ����λ�����³���
 *
 */
void App_update_init(void);

/**
 * @brief �ȴ������巢�͸�������
 *
 */
void App_update_wait_cmd(void);

/**
 * @brief ���͸��³����������
 *
 */
void App_update_send_app(void);

/**
 * @brief ѭ������
 *
 */
void App_update_work(void);

#endif // __APP_UPDATE__
