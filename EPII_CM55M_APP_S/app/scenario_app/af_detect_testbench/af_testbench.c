#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "powermode_export.h"

#define WE2_CHIP_VERSION_C		0x8538000c
#define FRAME_CHECK_DEBUG		1
#ifdef TRUSTZONE_SEC
#ifdef FREERTOS
/* Trustzone config. */
//
/* FreeRTOS includes. */
//#include "secure_port_macros.h"
#else
#if (__ARM_FEATURE_CMSE & 1) == 0
#error "Need ARMv8-M security extensions"
#elif (__ARM_FEATURE_CMSE & 2) == 0
#error "Compile with --cmse"
#endif
#include "arm_cmse.h"
//#include "veneer_table.h"
//
#endif
#endif

#include "WE2_device.h"
#include "af_model_run.h"
#include "spi_master_protocol.h"
#include "hx_drv_spi.h"
#include "spi_eeprom_comm.h"
#include "board.h"
#include "xprintf.h"
//#include "allon_sensor_tflm.h"
#include "board.h"
#include "WE2_core.h"
#include "hx_drv_scu.h"
#include "hx_drv_swreg_aon.h"
#ifdef IP_sensorctrl
#include "hx_drv_sensorctrl.h"
#endif
#ifdef IP_xdma
#include "hx_drv_xdma.h"
#include "sensor_dp_lib.h"
#endif
#ifdef IP_cdm
#include "hx_drv_cdm.h"
#endif
#ifdef IP_gpio
#include "hx_drv_gpio.h"
#endif
#include "hx_drv_pmu_export.h"
#include "hx_drv_pmu.h"
#include "powermode.h"
//#include "dp_task.h"
#include "BITOPS.h"

#include "cisdp_sensor.h"
#include "event_handler.h"
#include "common_config.h"
//#include "af_detection.h"
#include "model_data.h"
#include "sd_card_testbench.h"

#ifdef EPII_FPGA
#define DBG_APP_LOG             (1)
#else
#define DBG_APP_LOG             (1)
#endif
#if DBG_APP_LOG
    #define dbg_app_log(fmt, ...)       xprintf(fmt, ##__VA_ARGS__)
#else
    #define dbg_app_log(fmt, ...)
#endif


#define MAX_STRING  100
#define DEBUG_SPIMST_SENDPICS		(0x01) //0x00: off/ 0x01: JPEG/0x02: YUV422/0x03: YUV420/0x04: YUV400/0x05: RGB
#define SPI_SEN_PIC_CLK				(10000000)


/*******************************************************************************
 * Code
 ******************************************************************************/
/*!
 * @brief Main function
 */
int app_main(void) {
        test_sample_t my_test_sample;
        uint32_t current_index = 0;
        const uint32_t max_index = 51200 + 25600 + 25600;
        uint32_t loaded_index = 0;
        int8_t model_output[1];
        FRESULT fr;
	uint32_t wakeup_event;
	uint32_t wakeup_event1;
	model_output[0] = 123;
	
	if (sd_card_init("blindfold_test_vectors","blindfold_test_vectors") != FR_OK) { // Use FR_OK for success check
          xprintf("SD card FatFs initialization failed in testbench_init!\r\n");
          return -1; // Indicate failure
        }
        
        xprintf("Attempting to load sample from index %lu...\r\n", current_index);

        fr = load_next_test_vector(current_index, &my_test_sample, &loaded_index);

        if (fr == FR_OK) {
            xprintf("Successfully loaded sample %lu. First X value: %f, Y value: %f\r\n",
                    loaded_index, my_test_sample.x_data[0], my_test_sample.y_data);
            // Process my_test_sample.x_data and my_test_sample.y_data here
            // e.g., pass x_data to your inference engine
        } else if (fr == FR_NO_FILE) {
            xprintf("No more test files found after index %lu.\r\n", current_index);
            return 0; // Exit loop if no more files
        } else {
            xprintf("Error loading test vector at index %lu: %d\r\n", current_index, fr);
            // Handle other FatFs errors
            return -1;
        }

	hx_drv_pmu_get_ctrl(PMU_pmu_wakeup_EVT, &wakeup_event);
	hx_drv_pmu_get_ctrl(PMU_pmu_wakeup_EVT1, &wakeup_event1);
    xprintf("wakeup_event=0x%x,WakeupEvt1=0x%x\n", wakeup_event, wakeup_event1);

#if (FLASH_XIP_MODEL == 1)
    hx_lib_spi_eeprom_open(USE_DW_SPI_MST_Q);
    hx_lib_spi_eeprom_enable_XIP(USE_DW_SPI_MST_Q, true, FLASH_QUAD, true);
#endif

    if(init_init(true, true)<0) {
    	xprintf("cv init fail\n");
    	return -1;
    }
    
while(1) {
    // 1. Load test vector with error handling
    fr = load_next_test_vector(current_index, &my_test_sample, &loaded_index);
    if (fr != FR_OK) {
        if (fr == FR_NO_FILE) {
            xprintf("Reached end of test samples at index %lu\n", current_index);
        } else {
            xprintf("Fatal error loading sample %lu: %d\n", current_index, fr);
        }
        break;
    }

    // 2. Run inference
    if (run_model(&my_test_sample, model_output, 1) != 0) {
        xprintf("Inference failed for sample %lu\n", loaded_index);
        current_index = loaded_index + 1; // Skip to next sample
        continue;
    }
    // xprintf("Main loop first result value: raw=%d\r\n", model_output[0]);
    // 3. Get and save results
    fr = save_result_vector_bulk(loaded_index, model_output, 1, "qdense_bulk_result_");
    if (fr != FR_OK) {
        xprintf("Failed to save results for sample %lu: %d\n", loaded_index, fr);
        // Continue processing next sample despite save failure
    }

    // 4. Progress update every 100 samples
    if (loaded_index % 100 == 0) {
        xprintf("Processed %lu samples...\n", loaded_index + 1);
    }

    // 5. Update index (use the actually loaded index)
    current_index = loaded_index + 1;

    // 6. Has all requested data been processed
    if (current_index >= max_index){
        break;
    }
}

xprintf("Test sequence completed. Last processed sample: %lu\n", loaded_index);
	return 0;
}
