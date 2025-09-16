# Model Selection - Set default model path here
#MODEL_PATH ?= models/v2_0_0_combined_model_dense/af_detection.cc
#MODEL_PATH ?= models/v2_0_1_model_dense/af_detection.cc
#MODEL_PATH ?= models/v2_0_1_model_dense/v2_0_1_model_dense_fold_1
MODEL_PATH ?= models/v2_0_1_model_dense/v2_0_0_combined_model_dense

override SCENARIO_APP_SUPPORT_LIST := $(APP_TYPE)

# Add model path as a define for the application
APPL_DEFINES += -DMODEL_PATH=\"$(MODEL_PATH)\"
APPL_DEFINES += -DAF_DETECT_TESTBENCH
APPL_DEFINES += -DIP_xdma
APPL_DEFINES += -DEVT_DATAPATH
#APPL_DEFINES += -DEVT_CM55MTIMER -DEVT_CM55MMB
APPL_DEFINES += -DDBG_MORE

# Add model include path relative to the project root
APPL_DEFINES += -I$(SCENARIO_APP_ROOT)/$(APP_TYPE)/$(dir $(MODEL_PATH))

# Add the selected model to source files
SRC_FILES += $(MODEL_PATH)

# Rest of your existing Makefile remains unchanged...
EVENTHANDLER_SUPPORT = event_handler
EVENTHANDLER_SUPPORT_LIST += evt_datapath

LIB_SEL = pwrmgmt sensordp tflmtag2412_u55tag2411 spi_ptl spi_eeprom hxevent

MID_SEL = fatfs
FATFS_PORT_LIST = mmc_spi
CMSIS_DRIVERS_LIST = SPI

override OS_SEL:=
override TRUSTZONE := y
override TRUSTZONE_TYPE := security
override TRUSTZONE_FW_TYPE := 1
override CIS_SEL := HM_COMMON
override EPII_USECASE_SEL := drv_user_defined

CIS_SUPPORT_INAPP = cis_sensor
CIS_SUPPORT_INAPP_MODEL = cis_imx219

ifeq ($(CIS_SUPPORT_INAPP_MODEL), cis_imx219)
APPL_DEFINES += -DCIS_IMX
else ifeq ($(CIS_SUPPORT_INAPP_MODEL), cis_imx477)
APPL_DEFINES += -DCIS_IMX
else ifeq ($(CIS_SUPPORT_INAPP_MODEL), cis_imx708)
APPL_DEFINES += -DCIS_IMX
endif

ifeq ($(strip $(TOOLCHAIN)), arm)
override LINKER_SCRIPT_FILE := $(SCENARIO_APP_ROOT)/$(APP_TYPE)/af_detect_testbench.sct
else
override LINKER_SCRIPT_FILE := $(SCENARIO_APP_ROOT)/$(APP_TYPE)/af_detect_testbench.ld
endif
