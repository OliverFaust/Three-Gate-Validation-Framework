#include "sd_card_testbench.h"
#include <math.h> // Corrected: Using C math header for roundf()
#include <string.h> // Required for strcpy

// Directory definitions
#define DRV ""
#define BULK_RESULT_COUNT 256

static FATFS fs; // Filesystem object

// Global variables to hold the directory paths
static char g_x_test_folder[MAX_PATH_LEN];
static char g_y_test_folder[MAX_PATH_LEN];

// Static variables for bulk writing
static int8_t bulk_results[BULK_RESULT_COUNT];
static uint32_t bulk_results_count = 0;

// Forward declarations for internal helper functions (optional, but good practice)
static FRESULT read_binary_file(const char *filepath, void *buffer, uint32_t size, uint32_t *bytes_read);


/**
 * @brief Initializes the SD card and FatFs filesystem.
 *
 * This function mounts the SD card, sets up necessary GPIO pinmuxes
 * for SPI communication, and initializes the test vector directories.
 *
 * @param x_test_folder The directory for input test vectors.
 * @param y_test_folder The directory for ground truth labels.
 * @return FRESULT FatFs result code (FR_OK if successful).
 */
FRESULT sd_card_init(const char *x_test_folder, const char *y_test_folder)
{
    FRESULT res;

    // Copy the folder names into the global variables
    strncpy(g_x_test_folder, x_test_folder, MAX_PATH_LEN - 1);
    g_x_test_folder[MAX_PATH_LEN - 1] = '\0'; // Ensure null-termination
    strncpy(g_y_test_folder, y_test_folder, MAX_PATH_LEN - 1);
    g_y_test_folder[MAX_PATH_LEN - 1] = '\0'; // Ensure null-termination


    // Configure GPIO pinmuxes for SPI communication with SD card
    // Assuming these pins are correct for your board
    hx_drv_scu_set_PB2_pinmux(SCU_PB2_PINMUX_SPI_M_DO_1, 1);   // SPI MOSI
    hx_drv_scu_set_PB3_pinmux(SCU_PB3_PINMUX_SPI_M_DI_1, 1);   // SPI MISO
    hx_drv_scu_set_PB4_pinmux(SCU_PB4_PINMUX_SPI_M_SCLK_1, 1); // SPI Clock
    hx_drv_scu_set_PB5_pinmux(SCU_PB5_PINMUX_SPI_M_CS_1, 1);   // SPI Chip Select (Software controlled)

    xprintf("Attempting to mount SD card...\r\n");

    // Mount the FatFs filesystem
    res = f_mount(&fs, DRV, 1); // 1 means immediate mount
    if (res != FR_OK)
    {
        xprintf("f_mount failed with error: %d\r\n", res);
        return res;
    }
    xprintf("SD card mounted successfully.\r\n");

    // Optional: Change to root directory if not already there,
    // or just assume paths are absolute from root for g_x_test_folder/g_y_test_folder
    // For this design, we'll assume g_x_test_folder and g_y_test_folder are relative to root,
    // or are absolute paths if defined as such.

    // Check if x_test and y_test directories exist, create if not (though Python script should handle this)
    // For read-only testbench, we might not need to create them, but useful for robustness.
    FILINFO fno;
    res = f_stat(g_x_test_folder, &fno);
    if (res != FR_OK) {
        xprintf("Directory '%s' not found. Creating...\r\n", g_x_test_folder);
        res = f_mkdir(g_x_test_folder);
        if (res != FR_OK) {
            xprintf("Failed to create directory '%s': %d\r\n", g_x_test_folder, res);
            return res;
        }
    }
    res = f_stat(g_y_test_folder, &fno);
    if (res != FR_OK) {
        xprintf("Directory '%s' not found. Creating...\r\r\n", g_y_test_folder);
        res = f_mkdir(g_y_test_folder);
        if (res != FR_OK) {
            xprintf("Failed to create directory '%s': %d\r\n", g_y_test_folder, res);
            return res;
        }
    }

    return FR_OK;
}


/**
 * @brief Helper function to read data from a binary file.
 *
 * @param filepath The path to the binary file.
 * @param buffer Pointer to the buffer to store the read data.
 * @param size The number of bytes to read.
 * @param bytes_read Pointer to a UINT to store the actual number of bytes read.
 * @return FRESULT FatFs result code.
 */
static FRESULT read_binary_file(const char *filepath, void *buffer, uint32_t size, uint32_t *bytes_read)
{
    FIL fil;
    FRESULT res;
    UINT br;

    res = f_open(&fil, filepath, FA_READ);
    if (res != FR_OK)
    {
        // xprintf("Failed to open file '%s' for reading. Error: %d\r\n", filepath, res);
        return res; // Return error if file cannot be opened
    }

    res = f_read(&fil, buffer, size, &br);
    if (res != FR_OK)
    {
        xprintf("Failed to read from file '%s'. Error: %d\r\n", filepath, res);
    }
    else if (br != size)
    {
        xprintf("Warning: Read %lu bytes from '%s', expected %lu.\r\n", br, filepath, size);
        res = FR_DENIED; // Or FR_RW_ERROR, or a custom error code
    }

    *bytes_read = br;
    f_close(&fil);
    return res;
}


/**
 * @brief Loads a test vector (X) and its corresponding ground truth (Y) from the SD card using hex directory structure.
 *
 * @param start_index The starting index to attempt loading from.
 * @param sample_data Pointer to a test_sample_t structure to store the loaded data.
 * @param actual_index_loaded Pointer to a uint32_t to store the actual index of the loaded sample.
 * @return FRESULT FR_OK if successful, FR_NO_FILE if not found, or other FatFs error codes.
 */
FRESULT load_next_test_vector(uint32_t start_index, test_sample_t *sample_data, uint32_t *actual_index_loaded)
{
    char x_filepath[MAX_PATH_LEN];
    char y_filepath[MAX_PATH_LEN];
    FRESULT res_x = FR_OK;
    FRESULT res_y = FR_OK;
    uint32_t current_index = start_index;
    uint32_t bytes_read_x = 0;
    uint32_t bytes_read_y = 0;

    if (sample_data == NULL || actual_index_loaded == NULL) {
        return FR_INVALID_PARAMETER;
    }

    // Loop through indices to find a valid pair
    while (current_index < NUM_TEST_SAMPLES)
    {
        // Generate hex-based directory structure paths
        uint8_t dir1 = (current_index >> 16) & 0xFF;
        uint8_t dir2 = (current_index >> 8) & 0xFF;

        // Construct file paths using hex directory structure
        xsprintf(x_filepath, "%s/%02x/%02x/x_test_%06lu.bin",
                 g_x_test_folder, dir1, dir2, current_index);
        xsprintf(y_filepath, "%s/%02x/%02x/y_test_%06lu.bin",
                 g_y_test_folder, dir1, dir2, current_index);

        //xprintf("Attempting to load sample %lu:\r\n", current_index);
        xprintf("  X file: %s\r\n", x_filepath);
        //xprintf("  Y file: %s\r\n", y_filepath);

        // Try to read both X and Y files
        res_x = read_binary_file(x_filepath, sample_data->x_data, X_TEST_VECTOR_SIZE, &bytes_read_x);
        //res_y = read_binary_file(y_filepath, &(sample_data->y_data), Y_TEST_VECTOR_SIZE, &bytes_read_y);

        if (res_x == FR_OK && res_y == FR_OK)
        {
            // Both files found and read successfully
            sample_data->x_data_size = bytes_read_x;
            sample_data->y_data_size = bytes_read_y;
            *actual_index_loaded = current_index;
            //xprintf("Successfully loaded sample %lu.\r\n", current_index);
            return FR_OK;
        }
        else
        {
            xprintf("  Failed to load sample %lu. X_res: %d, Y_res: %d\r\n", current_index, res_x, res_y);
            current_index++;
        }
    }

    xprintf("No more valid test vector pairs found within the range (0 to %lu).\r\n", NUM_TEST_SAMPLES -1);
    return FR_NO_FILE;
}

/**
 * @brief Saves the raw model output (int8_t vector) using the 2-Level Hex Hash Structure
 * * @param index Index of the test sample (determines directory structure)
 * @param model_output Pointer to the int8_t model output array
 * @param output_length Number of elements in the output array
 * @param file_prefix Prefix for the output filename (e.g., "result_test_")
 * @return FRESULT FR_OK if successful, or FatFs error code
 */
FRESULT save_result_vector(uint32_t index, int8_t *model_output, uint32_t output_length, const char *file_prefix)
{
    char result_path[MAX_PATH_LEN];
    FIL file;
    UINT bytes_written;
    FRESULT res;

    if (index >= NUM_TEST_SAMPLES || model_output == NULL || output_length == 0 || file_prefix == NULL) {
        return FR_INVALID_PARAMETER;
    }

    // Calculate directory levels from index
    uint8_t dir1 = (index >> 16) & 0xFF;
    uint8_t dir2 = (index >> 8) & 0xFF;

    // Construct result file path with variable prefix
    xsprintf(result_path, "%s/%02x/%02x/%s%06lu.bin",
             g_x_test_folder, dir1, dir2, file_prefix, index);

    xprintf("Saving model output for sample %lu:\r\n", index);
    xprintf("  Path: %s\r\n", result_path);
    xprintf("  Output length: %lu elements\r\n", output_length);
    xprintf("  First result value: raw=%d\r\n", model_output[0]);

    // Ensure directory exists
    char dir_path[MAX_PATH_LEN];
    xsprintf(dir_path, "%s/%02x/%02x", g_x_test_folder, dir1, dir2);
    // Try to create the directory, ignore if it already exists
    res = f_mkdir(dir_path);
    if (res != FR_OK && res != FR_EXIST) {
        xprintf("  Directory creation failed: %d\r\n", res);
        return res;
    }

    // Open file for writing
    res = f_open(&file, result_path, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        xprintf("  File open failed: %d\r\n", res);
        return res;
    }

    // Write raw int8_t data
    res = f_write(&file, model_output, output_length * sizeof(int8_t), &bytes_written);
    if (res != FR_OK || bytes_written != output_length * sizeof(int8_t)) {
        xprintf("  Write failed: %d, bytes %u/%u\r\n",
                res, bytes_written, output_length * sizeof(int8_t));
        f_close(&file);
        return (res == FR_OK) ? FR_DISK_ERR : res;
    }

    f_close(&file);
    xprintf("  Successfully saved %d bytes of model output\r\n", bytes_written);
    return FR_OK;
}

/**
 * @brief Collects model outputs and saves them in a single binary file once 256 results have been collected.
 *
 * @param index Index of the test sample.
 * @param model_output Pointer to the int8_t model output array.
 * @param output_length Number of elements in the output array (must be 1).
 * @param file_prefix Prefix for the output filename.
 * @return FRESULT FR_OK if successful, or FatFs error code.
 */
FRESULT save_result_vector_bulk(uint32_t index, int8_t *model_output, uint32_t output_length, const char *file_prefix)
{
    if (output_length != 1 || model_output == NULL) {
        return FR_INVALID_PARAMETER;
    }

    // Store the result in the bulk array
    if (bulk_results_count < BULK_RESULT_COUNT) {
        bulk_results[bulk_results_count++] = *model_output;
    }

    // If we have collected 256 results, write them to the file
    if (bulk_results_count == BULK_RESULT_COUNT) {
        char result_path[MAX_PATH_LEN];
        FIL file;
        UINT bytes_written;
        FRESULT res;

        // Create the directory path based on the last index
        uint8_t dir1 = (index >> 16) & 0xFF;
        uint8_t dir2 = (index >> 8) & 0xFF;

        xsprintf(result_path, "%s/%02x/%02x/%s.bin",
                 g_x_test_folder, dir1, dir2, file_prefix);

        xprintf("Writing %d collected results to %s\r\n", BULK_RESULT_COUNT, result_path);

        // Ensure the directory exists
        char dir_path[MAX_PATH_LEN];
        xsprintf(dir_path, "%s/%02x/%02x", g_x_test_folder, dir1, dir2);
        res = f_mkdir(dir_path);
        if (res != FR_OK && res != FR_EXIST) {
            xprintf("  Directory creation for bulk file failed: %d\r\n", res);
            return res;
        }

        // Open the file for writing
        res = f_open(&file, result_path, FA_WRITE | FA_CREATE_ALWAYS);
        if (res != FR_OK) {
            xprintf("  Bulk file open failed: %d\r\n", res);
            return res;
        }

        // Write the entire bulk results array
        res = f_write(&file, bulk_results, sizeof(bulk_results), &bytes_written);
        if (res != FR_OK || bytes_written != sizeof(bulk_results)) {
            xprintf("  Bulk write failed: %d, bytes %u/%u\r\n",
                    res, bytes_written, sizeof(bulk_results));
            f_close(&file);
            return (res == FR_OK) ? FR_DISK_ERR : res;
        }

        f_close(&file);
        xprintf("  Successfully saved %d bytes in bulk.\r\n", bytes_written);

        // Reset the counter for the next batch
        bulk_results_count = 0;
    }

    return FR_OK;
}

// Implement your original GPIO functions (if not already in a separate file and linked)
void SSPI_CS_GPIO_Output_Level(bool setLevelHigh)
{
    hx_drv_gpio_set_out_value(GPIO16, (GPIO_OUT_LEVEL_E) setLevelHigh);
}

void SSPI_CS_GPIO_Pinmux(bool setGpioFn)
{
    if (setGpioFn)
        hx_drv_scu_set_PB5_pinmux(SCU_PB5_PINMUX_GPIO16, 0);
    else
        hx_drv_scu_set_PB5_pinmux(SCU_PB5_PINMUX_SPI_M_CS_1, 0);
}

void SSPI_CS_GPIO_Dir(bool setDirOut)
{
    if (setDirOut)
        hx_drv_gpio_set_output(GPIO16, GPIO_OUT_HIGH);
    else
        hx_drv_gpio_set_input(GPIO16);
}

/*
// The wav_header_init and fastfs_write_audio functions are specific to writing WAV files.
// They are not directly needed for the testbench reading functionality as requested,
// but I'm including them here as comments in case you integrate audio recording later.

// Assuming WAV_HDR typedef is globally available or included
int wav_header_init(int32_t srate, int16_t num_chans)
{
    WAV_HDR wavh; // Assuming this is passed or is a global variable
    strncpy(wavh.riff, "RIFF", 4);
    strncpy(wavh.wave, "WAVE", 4);
    strncpy(wavh.fmt, "fmt ", 4);
    strncpy(wavh.data, "data", 4);

    wavh.chunk_size = 16;
    wavh.format_tag = 1;
    wavh.num_chans = num_chans;
    wavh.srate = srate;
    wavh.bytes_per_sec = srate * 2; // Assuming 16-bit samples (2 bytes/sample)
    wavh.bytes_per_samp = 2;
    wavh.bits_per_samp = 16;
    return 0;
}

int fastfs_write_audio(uint32_t SRAM_addr, uint32_t pcm_size, uint8_t *filename)
{
    FIL fil_w;
    FRESULT res;
    UINT bw;
    WAV_HDR wavh_local; // Need a way to get/pass the WAV_HDR data here

    // This part assumes wavh is a global variable or passed.
    // For a robust function, wavh would be an argument.
    // For example purposes, let's assume it's copied from a global `wavh` or configured
    // prior to this call.
    // For this example, let's just make it clear that WAV_HDR
    // initialization must happen elsewhere or be passed.
    // wavh_local = some_global_wavh_instance;
    // Or, if always the same, hardcode some fields for writing raw bin.
    // For writing .bin files as per the prompt, you wouldn't use WAV_HDR at all.

    // If writing RAW audio binary, you just open and write:
    res = f_open(&fil_w, filename, FA_CREATE_ALWAYS | FA_WRITE); // Use FA_CREATE_ALWAYS to overwrite
    if (res == FR_OK)
    {
        xprintf("Writing raw audio data to file : %s.\r\n", filename);
        res = f_write(&fil_w, (void*)SRAM_addr, pcm_size, &bw);
        if (res != FR_OK) { xprintf("f_write res = %d\r\n", res); }
        else if (bw != pcm_size) { xprintf("Warning: Wrote %lu bytes, expected %lu.\r\n", bw, pcm_size); }
        f_close(&fil_w);
    }
    else
    {
        xprintf("f_open res = %d\r\n", res);
    }
    return 0;
}
*/

/*
// The list_dir and scan_files functions were for directory Browse.
// They are not directly needed for loading specific test vectors.
// I've commented them out but you can keep them if you need general file system utilities.
FRESULT list_dir (const char *path)
{
    FRESULT res;
    DIR dir;
    FILINFO fno;
    int nfile, ndir;

    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        nfile = ndir = 0;
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;
            if (fno.fattrib & AM_DIR) {
                xprintf("   <DIR>   %s\r\n", fno.fname);
                ndir++;
            } else {
                xprintf("%10u %s\r\n", (unsigned int)fno.fsize, fno.fname);
                nfile++;
            }
        }
        f_closedir(&dir);
        xprintf("%d dirs, %d files.\r\n", ndir, nfile);
    } else {
        xprintf("Failed to open \"%s\". (%u)\r\n", path, res);
    }
    return res;
}

FRESULT scan_files (char* path)
{
    FRESULT res;
    DIR dir;
    UINT i;
    static FILINFO fno;

    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;
            if (fno.fattrib & AM_DIR) {
                i = strlen(path);
                xsprintf(&path[i], "/%s", fno.fname);
                res = scan_files(path);
                if (res != FR_OK) break;
                path[i] = 0;
            } else {
                xprintf("%s/%s\r\n", path, fno.fname);
            }
        }
        f_closedir(&dir);
    }
    return res;
}
*/
