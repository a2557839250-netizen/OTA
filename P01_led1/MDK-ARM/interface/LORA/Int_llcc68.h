#ifndef __INT_LLCC68__
#define __INT_LLCC68__

#include "driver_llcc68_interface.h"

#define LLCC68_LORA_DEFAULT_STOP_TIMER_ON_PREAMBLE LLCC68_BOOL_FALSE       /**< disable stop timer on preamble */
#define LLCC68_LORA_DEFAULT_REGULATOR_MODE LLCC68_REGULATOR_MODE_DC_DC_LDO /**< only ldo */
#define LLCC68_LORA_DEFAULT_PA_CONFIG_DUTY_CYCLE 0x02                      /**< set +17dBm power */
#define LLCC68_LORA_DEFAULT_PA_CONFIG_HP_MAX 0x03                          /**< set +17dBm power */
#define LLCC68_LORA_DEFAULT_TX_DBM 17                                      /**< +17dBm */
#define LLCC68_LORA_DEFAULT_RAMP_TIME LLCC68_RAMP_TIME_10US                /**< set ramp time 10 us */
#define LLCC68_LORA_DEFAULT_SF LLCC68_LORA_SF_9                            /**< sf9 */
#define LLCC68_LORA_DEFAULT_BANDWIDTH LLCC68_LORA_BANDWIDTH_125_KHZ        /**< 125khz */
#define LLCC68_LORA_DEFAULT_CR LLCC68_LORA_CR_4_5                          /**< cr4/5 */
#define LLCC68_LORA_DEFAULT_LOW_DATA_RATE_OPTIMIZE LLCC68_BOOL_FALSE       /**< disable low data rate optimize */
#define LLCC68_LORA_DEFAULT_RF_FREQUENCY 480000000U                        /**< 480000000Hz */
#define LLCC68_LORA_DEFAULT_SYMB_NUM_TIMEOUT 0                             /**< 0 */

// LoRa 通讯的同步字 => 同步字相同的情况下  完成匹配通信
#define LLCC68_LORA_DEFAULT_SYNC_WORD 0x3456U                           /**< public network */
#define LLCC68_LORA_DEFAULT_RX_GAIN 0x94                                /**< common rx gain */
#define LLCC68_LORA_DEFAULT_OCP 0x38                                    /**< 140 mA */
#define LLCC68_LORA_DEFAULT_PREAMBLE_LENGTH 12                          /**< 12 */
#define LLCC68_LORA_DEFAULT_HEADER LLCC68_LORA_HEADER_EXPLICIT          /**< explicit header */
#define LLCC68_LORA_DEFAULT_BUFFER_SIZE 255                             /**< 255 */
#define LLCC68_LORA_DEFAULT_CRC_TYPE LLCC68_LORA_CRC_TYPE_ON            /**< crc on */
#define LLCC68_LORA_DEFAULT_INVERT_IQ LLCC68_BOOL_FALSE                 /**< disable invert iq */
#define LLCC68_LORA_DEFAULT_CAD_SYMBOL_NUM LLCC68_LORA_CAD_SYMBOL_NUM_2 /**< 2 symbol */
#define LLCC68_LORA_DEFAULT_CAD_DET_PEAK 24                             /**< 24 */
#define LLCC68_LORA_DEFAULT_CAD_DET_MIN 10                              /**< 10 */
#define LLCC68_LORA_DEFAULT_START_MODE LLCC68_START_MODE_WARM           /**< warm mode */
#define LLCC68_LORA_DEFAULT_RTC_WAKE_UP LLCC68_BOOL_TRUE                /**< enable rtc wake up */

/**
 * @brief     lora example init
 * @param[in] *callback points to a callback address
 * @return    status code
 *            - 0 success
 *            - 1 init failed
 * @note      none
 */
uint8_t llcc68_lora_init(void);

/**
 * @brief  lora example enter to the continuous receive mode
 * @return status code
 *         - 0 success
 *         - 1 enter failed
 * @note   none
 */
uint8_t llcc68_lora_set_continuous_receive_mode(void);

/**
 * @brief  lora example enter to the shot receive mode
 * @return status code
 *         - 0 success
 *         - 1 enter failed
 * @note   none
 */
uint8_t llcc68_lora_set_shot_receive_mode(double us);
/**
 * @brief  lora example enter to the send mode
 * @return status code
 *         - 0 success
 *         - 1 enter failed
 * @note   none
 */
uint8_t llcc68_lora_set_send_mode(void);
/**
 * @brief     lora example send lora data
 * @param[in] *buf points to a data buffer
 * @param[in] len is the data length
 * @return    status code
 *            - 0 success
 *            - 1 send failed
 * @note      none
 */
uint8_t llcc68_lora_send(uint8_t *buf, uint16_t len);


/**
 * @brief lora example receive data
 * 
 * @param buf 
 * @param len 
 * @return uint8_t 
 */
uint8_t llcc68_lora_receive(void);

#endif // __INT_LLCC68__
