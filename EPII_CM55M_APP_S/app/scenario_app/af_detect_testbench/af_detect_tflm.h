/*
 * hello_world.h
 *
 *  Created on: Dec 3, 2020
 *      Author: 902447
 */

#ifndef APP_AF_DETECT_TFLM_
#define APP_AF_DETECT_TFLM_

#define APP_BLOCK_FUNC() do{ \
	__asm volatile("b    .");\
	}while(0)

int app_main(void);

#endif /* APP_AF_DETECT_TFLM_ */
