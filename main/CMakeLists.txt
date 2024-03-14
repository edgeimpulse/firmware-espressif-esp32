cmake_minimum_required(VERSION 3.13.1)

set(MODEL_FOLDER ..)
set(EI_SDK_FOLDER ../edge-impulse-sdk)
set(EI_PLATFORM_FOLDER ../edge-impulse)
set(FIRMWARE_SDK_FOLDER ../firmware-sdk)

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
add_definitions(-DEI_CLASSIFIER_TFLITE_ENABLE_ESP_NN=1) # enables ESP-NN optimizations by Espressif
endif()

set(include_dirs
    ${MODEL_FOLDER}
    ${MODEL_FOLDER}/tflite-model
    ${MODEL_FOLDER}/model-parameters
    ${EI_SDK_FOLDER}
    ${EI_SDK_FOLDER}/classifier
    ${EI_SDK_FOLDER}/dsp
    ${EI_SDK_FOLDER}/porting
    ${EI_SDK_FOLDER}/porting/espressif/ESP-NN
    ${EI_SDK_FOLDER}/porting/espressif/ESP-NN/include
    ${EI_SDK_FOLDER}/tensorflow
    ${EI_SDK_FOLDER}/third_party
    ${EI_SDK_FOLDER}/third_party/ruy
    ${EI_SDK_FOLDER}/third_party/gemmlowp
    ${EI_SDK_FOLDER}/third_party/flatbuffers/include
    ${EI_PLATFORM_FOLDER}
    ${EI_PLATFORM_FOLDER}/ingestion-sdk-c
    ${EI_PLATFORM_FOLDER}/ingestion-sdk-platform/espressif_esp32
    ${EI_PLATFORM_FOLDER}/ingestion-sdk-platform/sensors
    ${EI_PLATFORM_FOLDER}/inference
    ${FIRMWARE_SDK_FOLDER}
    ${FIRMWARE_SDK_FOLDER}/at-server
)

include(${EI_SDK_FOLDER}/cmake/utils.cmake)

RECURSIVE_FIND_FILE_EXCLUDE_DIR(SOURCE_FILES "../edge-impulse-sdk" "CMSIS" "*.cpp")
RECURSIVE_FIND_FILE_EXCLUDE_DIR(CC_FILES "../edge-impulse-sdk" "CMSIS" "*.cc")
RECURSIVE_FIND_FILE_EXCLUDE_DIR(S_FILES "../edge-impulse-sdk" "CMSIS" "*.s")
RECURSIVE_FIND_FILE_EXCLUDE_DIR(C_FILES "../edge-impulse-sdk" "CMSIS" "*.c")

# Manually handle needed C only files b/c of build errors
list(APPEND C_FILES "../edge-impulse-sdk/CMSIS/DSP/Source/StatisticsFunctions/arm_rms_f32.c")

RECURSIVE_FIND_FILE(PORTING_FILES "../edge-impulse-sdk/porting/espressif" "*.cpp")

RECURSIVE_FIND_FILE(FIRMWARE_SDK_FILES "../firmware-sdk" "*.cpp")
RECURSIVE_FIND_FILE(FIRMWARE_SDK_C_FILES "../firmware-sdk" "*.c")

RECURSIVE_FIND_FILE(MODEL_FILES "../tflite-model" "*.cpp")

RECURSIVE_FIND_FILE(PLATFORM_FILES "../edge-impulse/ingestion-sdk-platform/espressif_esp32" "*.cpp")
RECURSIVE_FIND_FILE(SENSORS_FILES "../edge-impulse/ingestion-sdk-platform/sensors" "*.cpp")
RECURSIVE_FIND_FILE(INGESTION_FILES "../edge-impulse/ingestion-sdk-c/" "*.cpp")

RECURSIVE_FIND_FILE(INFERENCE_FILES "../edge-impulse/inference" "*.cpp")
RECURSIVE_FIND_FILE(MBEDTLS_FILES "../edge-impulse/mbedtls_hmac_sha256_sw/mbedtls/src" "*.c")

list(APPEND SOURCE_FILES ${S_FILES})
list(APPEND SOURCE_FILES ${C_FILES})
list(APPEND SOURCE_FILES ${CC_FILES})

list(APPEND SOURCE_FILES ${PORTING_FILES})

list(APPEND SOURCE_FILES ${FIRMWARE_SDK_FILES})
list(APPEND SOURCE_FILES ${FIRMWARE_SDK_C_FILES})

list(APPEND SOURCE_FILES ${MODEL_FILES})

list(APPEND SOURCE_FILES ${PLATFORM_FILES})
list(APPEND SOURCE_FILES ${SENSORS_FILES})
list(APPEND SOURCE_FILES ${INGESTION_FILES})

list(APPEND SOURCE_FILES ${INFERENCE_FILES})
list(APPEND SOURCE_FILES ${MBEDTLS_FILES})

idf_component_register(SRCS "main.cpp" "${SOURCE_FILES}"
                       INCLUDE_DIRS "${include_dirs}")

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++17)