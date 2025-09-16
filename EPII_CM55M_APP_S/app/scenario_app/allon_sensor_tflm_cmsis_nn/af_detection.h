#ifndef APP_TESTBENCH_AF_DETECTION_MODEL_DATA_H_
#define APP_TESTBENCH_AF_DETECTION_MODEL_DATA_H_

#ifdef __cplusplus
extern "C" {
#endif

    extern const unsigned char af_detection_vela_tflite[];
    // MANUALLY SET THE CORRECT PARAMETERS (from Python output)
    const float input_scale = 1.7476615f;
    const int input_zero_point = -17;
    const float output_scale = 0.00390625f;
    const int output_zero_point = -128;

#ifdef __cplusplus
}
#endif
#endif /* APP_TESTBENCH_AF_DETECTION_MODEL_DATA_H_ */
