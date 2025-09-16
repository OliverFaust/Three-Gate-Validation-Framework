/*
 * cvapp.cpp
 *
 *  Created on: 2022�~11��18��
 *      Author: 902452
 */

#include <cstdio>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "WE2_device.h"
#include "board.h"
#include "af_model_run.h"
#include "cisdp_sensor.h"

#include "WE2_core.h"
#include "WE2_device.h"

#include "ethosu_driver.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/kernels/strided_slice.h"
#include "tensorflow/lite/c/common.h"
#if TFLM2209_U55TAG2205
#include "tensorflow/lite/micro/micro_error_reporter.h"
#endif
#include "xprintf.h"
#include "cisdp_cfg.h"

//#include "af_detection.h"
#include "model_data.h"
#include "common_config.h"
#include "model_params.h" 


#define LOCAL_FRAQ_BITS (8)
#define SC(A, B) ((A<<8)/B)

#define INPUT_SIZE_X 96
#define INPUT_SIZE_Y 96

#ifdef TRUSTZONE_SEC
#define U55_BASE	BASE_ADDR_APB_U55_CTRL_ALIAS
#else
#ifndef TRUSTZONE
#define U55_BASE	BASE_ADDR_APB_U55_CTRL_ALIAS
#else
#define U55_BASE	BASE_ADDR_APB_U55_CTRL
#endif
#endif

#define TENSOR_ARENA_BUFSIZE  (125*1024)
__attribute__(( section(".bss.NoInit"))) uint8_t tensor_arena_buf[TENSOR_ARENA_BUFSIZE] __ALIGNED(32);

using namespace std;

namespace {

constexpr int tensor_arena_size = TENSOR_ARENA_BUFSIZE;
uint32_t tensor_arena = (uint32_t)tensor_arena_buf;

struct ethosu_driver ethosu_drv; /* Default Ethos-U device driver */
tflite::MicroInterpreter *int_ptr=nullptr;
TfLiteTensor* input, *output;
};

static void _arm_npu_irq_handler(void)
{
    /* Call the default interrupt handler from the NPU driver */
    ethosu_irq_handler(&ethosu_drv);
}

/**
 * @brief  Initialises the NPU IRQ
 **/
static void _arm_npu_irq_init(void)
{
    const IRQn_Type ethosu_irqnum = (IRQn_Type)U55_IRQn;

    /* Register the EthosU IRQ handler in our vector table.
     * Note, this handler comes from the EthosU driver */
    EPII_NVIC_SetVector(ethosu_irqnum, (uint32_t)_arm_npu_irq_handler);

    /* Enable the IRQ */
    NVIC_EnableIRQ(ethosu_irqnum);

}

static int _arm_npu_init(bool security_enable, bool privilege_enable)
{
    int err = 0;

    /* Initialise the IRQ */
    _arm_npu_irq_init();

    /* Initialise Ethos-U55 device */
#if TFLM2209_U55TAG2205
	const void * ethosu_base_address = (void *)(U55_BASE);
#else 
	void * const ethosu_base_address = (void *)(U55_BASE);
#endif
    if (0 != (err = ethosu_init(
                            &ethosu_drv,             /* Ethos-U driver device pointer */
                            ethosu_base_address,     /* Ethos-U NPU's base address. */
                            NULL,       /* Pointer to fast mem area - NULL for U55. */
                            0, /* Fast mem region size. */
							security_enable,                       /* Security enable. */
							privilege_enable))) {                   /* Privilege enable. */
    	xprintf("failed to initalise Ethos-U device\n");
            return err;
        }

    xprintf("Ethos-U55 device initialised\n");

    return 0;
}

int init_model(bool security_enable, bool privilege_enable)
{
    int ercode = 0;
    const float input_scale = MODEL_INPUT_SCALE;
    const int input_zero_point = MODEL_INPUT_ZERO_POINT;
    const float output_scale = MODEL_OUTPUT_SCALE;
    const int output_zero_point = MODEL_OUTPUT_ZERO_POINT;


	if(_arm_npu_init(security_enable, privilege_enable)!=0)
		return -1;

#if (FLASH_XIP_MODEL == 1)
	static const tflite::Model*model = tflite::GetModel((const void *)0x3A180000);
#else
	//static const tflite::Model*model = tflite::GetModel((const void *)af_detection_vela_tflite);
	static const tflite::Model*model = tflite::GetModel((const void *)model_data);
#endif

	if (model->version() != TFLITE_SCHEMA_VERSION) {
		xprintf(
			"[ERROR] model's schema version %d is not equal "
			"to supported version %d\n",
			model->version(), TFLITE_SCHEMA_VERSION);
		return -1;
	}
	else {
		xprintf("model's schema version %d\n", model->version());
	}
	#if TFLM2209_U55TAG2205
	static tflite::MicroErrorReporter micro_error_reporter;
	#endif
	static tflite::MicroMutableOpResolver<12> op_resolver;

    op_resolver.AddDepthwiseConv2D();
	op_resolver.AddRelu6();
	op_resolver.AddConv2D();
	op_resolver.AddAveragePool2D();
	op_resolver.AddReshape();
	op_resolver.AddSoftmax();
	op_resolver.AddDequantize();
	op_resolver.AddStridedSlice();
	op_resolver.AddPack();
	op_resolver.AddFill();
	if (kTfLiteOk != op_resolver.AddEthosU()){
		xprintf("Failed to add Arm NPU support to op resolver.");
		return false;
	}
	#if TFLM2209_U55TAG2205
	static tflite::MicroInterpreter static_interpreter(model, op_resolver, (uint8_t*)tensor_arena, tensor_arena_size, &micro_error_reporter);
	#else
	static tflite::MicroInterpreter static_interpreter(model, op_resolver, (uint8_t*)tensor_arena, tensor_arena_size);
	#endif
	if(static_interpreter.AllocateTensors()!= kTfLiteOk) {
		return false;
	}
	int_ptr = &static_interpreter;
	input = static_interpreter.input(0);
	output = static_interpreter.output(0);

	xprintf("initial done\n");
	
    // Debug print (using integer-only formatting)
    xprintf("Model parameter\n");
    xprintf("Input: scale=%d/%d, zp=%d\n", (int)(input_scale*1e6), 1000000, input_zero_point);
    xprintf("Output: scale=%d/%d, zp=%d\n", (int)(output_scale*1e6), 1000000, output_zero_point);

	return ercode;
}

int run_model(test_sample_t* sample, int8_t *model_output, uint32_t output_length) {
    int ercode = 0;
    // MANUALLY SET THE CORRECT PARAMETERS (from Python output)
    // Use the model-specific parameters
    const float input_scale = MODEL_INPUT_SCALE;
    const int input_zero_point = MODEL_INPUT_ZERO_POINT;
    const float output_scale = MODEL_OUTPUT_SCALE;
    const int output_zero_point = MODEL_OUTPUT_ZERO_POINT;
    
    // Quantize input
    int8_t* tensor_data = input->data.int8;
    for (int i = 0; i < 40; i++) {
        float val = sample->x_data[i];
        tensor_data[i] = (int8_t)(roundf(val / input_scale) + input_zero_point);
    }

    // Run inference
    if(int_ptr->Invoke() != kTfLiteOk) {
        xprintf("Inference failed\n");
        return -1;
    }

    // Dequantize output
    memcpy(model_output, output->data.int8, output_length * sizeof(int8_t));
    float af_score = (model_output[0] - output_zero_point) * output_scale;
    af_score = fmaxf(0.0f, fminf(1.0f, af_score));  // Clamp to [0,1]

    // Print results (integer formatting workaround)
    int score_percent = (int)(af_score * 100);
    xprintf("AF: raw=%d, score=%d%%, %s\n", 
           model_output[0], score_percent,
           af_score >= 0.5f ? "DETECTED" : "normal");

    return 0;
}

int cv_deinit()
{
	//TODO: add more deinit items here if need.
	return 0;
}

