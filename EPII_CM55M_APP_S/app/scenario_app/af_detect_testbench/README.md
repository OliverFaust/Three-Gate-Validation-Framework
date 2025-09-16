# AI@Edge Model Testbench for Atrial Fibrillation Detection

This module implements the **System-Gated Validation (Gate 3)** for the Three-Gate Validation Framework. It is a dedicated testbench designed to validate the performance of the quantized AFib detection model on the actual target hardware (Seeed Grove Vision AI Module V2), ensuring operational reliability in a realistic embedded environment.

## Overview

The testbench automates the process of streaming test vectors through the fully integrated system—from data input to model inference and result output. Its primary purpose is to provide a rigorous, repeatable, and measurable validation step that must be passed before a model is considered ready for deployment.

Key functionalities include:
*   **Automated Testing:** Sequentially processes a suite of test vectors from storage.
*   **Hardware-in-the-Loop Validation:** Runs the model on the actual target MCU, testing the entire toolchain.
*   **Result Logging:** Saves both the model's output and the corresponding expected results for analysis.
*   **Performance Benchmarking:** Enables measurement of real-world inference latency and stability.

## Configuration

### Model Selection
The specific AI model validated by the testbench is configured at build time through the linker script `af_detect_testbench.mk`. This allows for easy switching between different model versions or architectures without modifying the core application code.

**To select a model:**
1.  Open the file `af_detect_testbench.mk`.
2.  Locate the `MODEL_PATH` variable.
3.  Uncomment and set the path to your desired TensorFlow Lite model file (`.cc` or `.tflite` format).

```makefile
# Model Selection - Set default model path here
MODEL_PATH ?= models/my_new_optimized_model/af_detection.cc
# MODEL_PATH ?= models/v2_0_0_combined_model_dense/af_detection.cc
```

## Component Architecture

The testbench is built from three core components that work in concert:

### 1. `af_testbench.c`
This is the main orchestrator of the System-Gated Validation process.
*   **Role:** Manages the high-level test flow and sequence.
*   **Key Function:** `app_main(void)` - Initializes the system, iterates through all test vectors, and coordinates the testing process.
*   **Functionality:** It calls the initialisation routines, then for each test vector, it passes the data to the model runner and handles the saving of results.

### 2. `af_model_run.cpp`
This component acts as the bridge between the testbench and the TensorFlow Lite for Microcontrollers (TFLM) interpreter.
*   **Role:** Executes the model inference on the provided input data.
*   **Key Function:** `run_af_model(test_sample_t* sample, int8_t *model_output, uint32_t output_length)` - Takes a pointer to input data, invokes the TFLM interpreter, and returns the result via an output pointer.
*   **Functionality:** This function encapsulates the entire model invocation, ensuring the input tensor is properly populated and the output tensor is read correctly.

### 3. `sd_card_testbench.c`
This module handles all interactions with the test data stored on the SD card, a crucial part of the automated testing pipeline.
*   **Role:** Data I/O manager for the testbench.
*   **Key Functions:**
    *   `FRESULT load_next_test_vector(uint32_t start_index, test_sample_t *sample_data, uint32_t *actual_index_loaded)`: Loads the next test vector (input data and expected label) from the SD card into memory.
    *   `FRESULT save_result_vector(uint32_t index, int8_t *model_output, uint32_t output_length, const char *file_prefix)`: Saves the model's output for a single test vector to the SD card for later analysis.
    *   `FRESULT save_result_vector_bulk(...)`: A variant for efficient bulk saving operations.

## Workflow: How the Testbench Operates

The validation process follows a precise sequence:

1.  **Initialization:** `af_testbench.c` initializes the system, the SD card, and the TFLM model via `af_model_run.cpp`.
2.  **Data Loading:** The testbench calls `load_next_test_vector()` from `sd_card_testbench.c` to fetch a test sample (e.g., a pre-processed ECG segment) from the SD card.
3.  **Model Inference:** The loaded input data is passed to `run_af_model()` in `af_model_run.cpp`. This function executes the model on the target hardware and returns the inference result (e.g., a score indicating the probability of AFib).
4.  **Result Saving:** The testbench calls `save_result_vector()` to write the model's output and the expected result back to the SD card.
5.  **Iteration:** Steps 2-4 are repeated for the entire set of test vectors.
6.  **Analysis:** After the test run, the saved result files on the SD card can be transferred to a host computer to calculate final performance metrics (accuracy, sensitivity, specificity, etc.), determining if the system passes Gate 3.

## Usage

1.  **Prepare the SD Card:**
    *   Format an SD card to FAT32.
    *   Populate it with your test dataset. The data should be in a binary format that your `load_next_test_vector()` function is designed to read (e.g., pre-processed 40-point RR interval sequences).

2.  **Build and Flash:**
    *   Build the firmware for the `af_detect_testbench` scenario application.
    *   Flash the resulting `output.img` file to the Grove Vision AI Module V2.

3.  **Run the Testbench:**
    *   Insert the prepared SD card into the device.
    *   Power on the device. The testbench should run automatically, processing all test vectors.
    *   Status and progress can typically be monitored via a connected serial terminal (e.g., UART output).

4.  **Analyze Results:**
    *   After the test completes, power off the device and remove the SD card.
    *   Transfer the generated result files (e.g., `result_*.bin`) to a host machine.
    *   Use a provided analysis script (e.g., a Python script) to compare the model's outputs against the expected values and generate a performance report (confusion matrix, accuracy, etc.).

## Integration within the Three-Gate Framework

This testbench is the final validation step (**Gate 3: System Gate**). Successfully passing this testbench means:
*   The model is fully integrated into the target system.
*   The entire data pipeline (I/O, pre-processing, inference) is functionally correct.
*   The system performs reliably on the target hardware with real-world data.
*   The model is ready for final deployment.

This objective, automated check is what transforms a prototype into a validated, deployable asset.
