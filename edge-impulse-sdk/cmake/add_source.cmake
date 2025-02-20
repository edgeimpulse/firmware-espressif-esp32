# Module containing predefined source lists that can be toggled and applied to targets
set(EI_SDK_FOLDER ${CMAKE_CURRENT_LIST_DIR}/..)
# Define all source lists here

include(${CMAKE_CURRENT_LIST_DIR}/utils.cmake)

# Stub in case we want them later
set(_BASE_SOURCES
    CACHE INTERNAL "Base source files"
)

set(_DSP_SOURCES
    ${EI_SDK_FOLDER}/dsp/memory.cpp
    ${EI_SDK_FOLDER}/dsp/kissfft/kiss_fftr.cpp
    ${EI_SDK_FOLDER}/dsp/kissfft/kiss_fft.cpp
    CACHE INTERNAL "DSP Source"
)

set(_DSP_CMSIS_SOURCES
    CACHE INTERNAL "DSP CMSIS Source"
)

list(APPEND _DSP_CMSIS_SOURCES
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_cfft_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_cfft_init_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_cfft_radix2_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_cfft_radix2_init_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_cfft_radix4_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_cfft_radix4_init_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_cfft_radix8_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_dct4_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_dct4_init_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_mfcc_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_mfcc_init_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_rfft_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_rfft_fast_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_rfft_fast_init_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_rfft_init_f32.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_bitreversal2.c
    ${EI_SDK_FOLDER}/CMSIS/DSP/Source/TransformFunctions/arm_bitreversal.c
)


RECURSIVE_FIND_FILE_APPEND(_DSP_CMSIS_SOURCES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/CommonTables" "*.c")
RECURSIVE_FIND_FILE_APPEND(_DSP_CMSIS_SOURCES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/BasicMathFunctions" "*.c")
RECURSIVE_FIND_FILE_APPEND(_DSP_CMSIS_SOURCES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/ComplexMathFunctions" "*.c")
RECURSIVE_FIND_FILE_APPEND(_DSP_CMSIS_SOURCES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/FastMathFunctions" "*.c")
RECURSIVE_FIND_FILE_APPEND(_DSP_CMSIS_SOURCES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/SupportFunctions" "*.c")
RECURSIVE_FIND_FILE_APPEND(_DSP_CMSIS_SOURCES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/MatrixFunctions" "*.c")
RECURSIVE_FIND_FILE_APPEND(_DSP_CMSIS_SOURCES "${EI_SDK_FOLDER}/CMSIS/DSP/Source/StatisticsFunctions" "*.c")

# Stub in case we want them later
set(_EXPERIMENTAL_SOURCES
    CACHE INTERNAL "Experimental source files"
)

# Function to apply collected sources to a target based on toggles
function(apply_predefined_sources TARGET_NAME)
    cmake_parse_arguments(
        PARSE_ARGV
        1
        ARG
        "USE_DSP;USE_EXPERIMENTAL;NO_DSP_CMSIS"
        ""
        ""
    )

    if(NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "Target ${TARGET_NAME} does not exist")
    endif()

    # Enable vector conversions, for M55
    target_compile_options(${TARGET_NAME} PRIVATE -flax-vector-conversions)

    # Always add base sources
    if(_BASE_SOURCES)
        target_sources(${TARGET_NAME} PRIVATE ${_BASE_SOURCES})
    endif()

    # Add optional sources if enabled
    if(ARG_USE_DSP AND _DSP_SOURCES)
        target_sources(${TARGET_NAME} PRIVATE ${_DSP_SOURCES})
        if(NOT ARG_NO_DSP_CMSIS)
            target_sources(${TARGET_NAME} PRIVATE ${_DSP_CMSIS_SOURCES})
        endif()
    endif()

    # Add experimental sources if enabled
    if(ARG_USE_EXPERIMENTAL AND _EXPERIMENTAL_SOURCES)
        target_sources(${TARGET_NAME} PRIVATE ${_EXPERIMENTAL_SOURCES})
    endif()

    # Create source groups for better IDE organization
    foreach(source ${_BASE_SOURCES} ${_DSP_SOURCES} ${_EXPERIMENTAL_SOURCES})
        get_filename_component(source_path "${source}" PATH)
        string(REPLACE "/" "\\" source_path_msvc "${source_path}")
        source_group("Source Files\\${source_path_msvc}" FILES "${source}")
    endforeach()
endfunction()