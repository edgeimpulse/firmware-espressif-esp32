cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EI_SDK_FOLDER ../../)

include(${EI_SDK_FOLDER}/cmake/utils.cmake)

if(DEFINED CONFIG_EDGE_IMPULSE_SDK) # using the edge-impulse-sdk-zephyr module
    zephyr_library_named(edge-impulse-sdk)
    
    if("${BOARD}" MATCHES "esp32")
        zephyr_compile_definitions(
            asm=__asm__                           # needed for ESP-NN headers
        )

        set(EI_ESP_INCLUDES
            ${EI_SDK_FOLDER}
            ${EI_SDK_FOLDER}/porting
            ${EI_SDK_FOLDER}/porting/espressif/esp-dsp/modules/common/include
            ${EI_SDK_FOLDER}/porting/espressif/esp-dsp/modules/fft/include
            ${EI_SDK_FOLDER}/porting/espressif/ESP-NN
            ${EI_SDK_FOLDER}/porting/espressif/ESP-NN/include
        )

        zephyr_include_directories(${EI_ESP_INCLUDES})
        
        RECURSIVE_FIND_FILE_EXCLUDE_DIR(CPP_EI_SOURCE_FILES "${EI_SDK_FOLDER}" "CMSIS" "*.cpp")
        RECURSIVE_FIND_FILE_EXCLUDE_DIR(CC_EI_SOURCE_FILES "${EI_SDK_FOLDER}" "CMSIS" "*.cc")
        RECURSIVE_FIND_FILE_EXCLUDE_DIR(C_EI_SOURCE_FILES "${EI_SDK_FOLDER}" "CMSIS" "*.c")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/porting/zephyr" "*.cpp")

        list(APPEND EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/StatisticsFunctions/arm_rms_f32.c")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/porting/espressif/ESP-NN" "*.s")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/porting/espressif/esp-dsp/modules/fft" "*.s")

        list(APPEND EI_SOURCE_FILES ${CPP_EI_SOURCE_FILES})
        list(APPEND EI_SOURCE_FILES ${CC_EI_SOURCE_FILES})
        list(APPEND EI_SOURCE_FILES ${C_EI_SOURCE_FILES})
    elseif("${ARCH}" STREQUAL "arm")
        zephyr_compile_definitions(
                        EI_CLASSIFIER_TFLITE_ENABLE_CMSIS_NN=1
                        ARM_MATH_LOOPUNROLL
                        )
        zephyr_include_directories(${EI_SDK_FOLDER})

        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}" "*.cpp")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}" "*.cc")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}" "*.s")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions" "*.c")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/CommonTables" "*.c")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/BasicMathFunctions" "*.c")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/ComplexMathFunctions" "*.c")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/FastMathFunctions" "*.c")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/SupportFunctions" "*.c")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/MatrixFunctions" "*.c")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/StatisticsFunctions" "*.c")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/NN/Source" "*.c")
        LIST(APPEND EI_SOURCE_FILES "${EI_SDK_FOLDER}/tensorflow/lite/c/common.c")
    else()
        zephyr_include_directories(${EI_SDK_FOLDER})

        RECURSIVE_FIND_FILE_EXCLUDE_DIR(CPP_EI_SOURCE_FILES "${EI_SDK_FOLDER}" "CMSIS" "*.cpp")
        RECURSIVE_FIND_FILE_EXCLUDE_DIR(CC_EI_SOURCE_FILES "${EI_SDK_FOLDER}" "CMSIS" "*.cc")
        RECURSIVE_FIND_FILE_EXCLUDE_DIR(C_EI_SOURCE_FILES "${EI_SDK_FOLDER}" "CMSIS" "*.c")
        RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/porting/zephyr" "*.cpp")

        list(APPEND EI_SOURCE_FILES ${CPP_EI_SOURCE_FILES})
        list(APPEND EI_SOURCE_FILES ${CC_EI_SOURCE_FILES})
        list(APPEND EI_SOURCE_FILES ${C_EI_SOURCE_FILES})
    endif()

    zephyr_library_sources(${EI_SOURCE_FILES})
else()  # use edge-impulse-sdk as a c++ to be intergated directly into the project
    if(NOT TARGET app)
        message(FATAL_ERROR "Please create a target named 'app' (ex: add_executable(app)) before adding this file")
    endif()
    target_include_directories(app PRIVATE
        ${EI_SDK_FOLDER}
    )

    RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}" "*.cpp")
    RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}" "*.cc")
    RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}" "*.s")
    RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions" "*.c")
    RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/CommonTables" "*.c")
    RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/BasicMathFunctions" "*.c")
    RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/ComplexMathFunctions" "*.c")
    RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/FastMathFunctions" "*.c")
    RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/SupportFunctions" "*.c")
    RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/MatrixFunctions" "*.c")
    RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/StatisticsFunctions" "*.c")
    RECURSIVE_FIND_FILE_APPEND(EI_SOURCE_FILES "${EI_SDK_FOLDER}/CMSIS/NN/Source" "*.c")
    LIST(APPEND EI_SOURCE_FILES "${EI_SDK_FOLDER}/tensorflow/lite/c/common.c")

    target_sources(app PRIVATE ${EI_SOURCE_FILES})
endif(DEFINED CONFIG_EDGE_IMPULSE_SDK)


