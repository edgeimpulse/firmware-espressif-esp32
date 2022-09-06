# Changelog

All notable changes to `firmware-sdk` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- 
- Added `CMakeLists.txt` for CMake based build systems (#4459)
- `ei_device_info_lib`: new `init_device_id` method to force developers to implement such a functionality (#4459)
- `ei_device_info_lib`: now device has a default `device_id` value (#4459)
- `EiDeviceMemory`: new `flush_data` method (#4152)
- `at_base64_lib`: new API allowing for chunked data to be encoded and processed by UART (#4678)
- `jpeg`: new API to encode and send in the base64 images from RAW RGB888, RGB565 or Grayscale buffers (#3579)

### Changed
- Global define of `EI_SENSOR_AQ_STREAM=FILE` is not needed anymore (#4459)
- Adding all `QCBOR` directories to include path is not needed anymore (#4459)
- extended `set_*` methods of the `EiDeviceInfo` allowing to not save config after changeing value (#4543)
- remove all references to old `ei_config_t` struct from `ei_fusion` module and use a new `EiDeviceInfo` interface (#4426)
- Removed `const` qualifier from some of `EiDeviceMemory` fields (#4459)
- Small fixes and code clean-up
