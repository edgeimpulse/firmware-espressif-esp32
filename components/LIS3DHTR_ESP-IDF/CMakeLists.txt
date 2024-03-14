if(IDF_TARGET STREQUAL "esp32" OR IDF_TARGET STREQUAL "esp8266" OR IDF_TARGET STREQUAL "esp32s3")
  set(COMPONENT_SRCS
    src/LIS3DHTR.cpp
    )

  set(COMPONENT_ADD_INCLUDEDIRS
    src/include
    )

  set(COMPONENT_PRIV_REQUIRES driver)

  register_component()
endif()
