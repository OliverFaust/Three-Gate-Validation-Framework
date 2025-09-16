/*
 * cvapp.h
 *
 *  Created on: 2018�~12��4��
 *      Author: 902452
 */

#ifndef APP_SCENARIO_ALLON_SENSOR_TFLM_CVAPP_
#define APP_SCENARIO_ALLON_SENSOR_TFLM_CVAPP_

#include "spi_protocol.h"
#include "sd_card_testbench.h"
#ifdef __cplusplus
extern "C" {
#endif

int init_model(bool security_enable, bool privilege_enable);

int run_af_model(test_sample_t* sample, int8_t *model_output, uint32_t output_length);

int cv_deinit();
#ifdef __cplusplus
}
#endif

#endif /* APP_SCENARIO_ALLON_SENSOR_TFLM_CVAPP_ */
