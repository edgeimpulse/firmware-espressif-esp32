/*
 * Copyright (c) 2020-2022 Arm Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#define DEFAULT_ARENA_SIZE_OF_MB 16
#define ETHOSU_PMU_EVENT_MAX 4

/*
 *The following undef are necessary to avoid clash with macros in GNU C Library
 * if removed the following warning/error are produced:
 *
 *  In the GNU C Library, "major" ("minor") is defined
 *  by <sys/sysmacros.h>. For historical compatibility, it is
 *  currently defined by <sys/types.h> as well, but we plan to
 *  remove this soon. To use "major" ("minor"), include <sys/sysmacros.h>
 *  directly. If you did not intend to use a system-defined macro
 *  "major" ("minor"), you should undefine it after including <sys/types.h>.
 */
#undef major
#undef minor

namespace EthosU {

class Exception : public std::exception {
public:
    Exception(const char *msg);
    virtual ~Exception() throw();
    virtual const char *what() const throw();

private:
    std::string msg;
};

/**
 * Sematic Version : major.minor.patch
 */
class SemanticVersion {
public:
    SemanticVersion(uint32_t _major = 0, uint32_t _minor = 0, uint32_t _patch = 0) :
        major(_major), minor(_minor), patch(_patch){};

    bool operator==(const SemanticVersion &other);
    bool operator<(const SemanticVersion &other);
    bool operator<=(const SemanticVersion &other);
    bool operator!=(const SemanticVersion &other);
    bool operator>(const SemanticVersion &other);
    bool operator>=(const SemanticVersion &other);

    uint32_t major;
    uint32_t minor;
    uint32_t patch;
};

std::ostream &operator<<(std::ostream &out, const SemanticVersion &v);

/*
 * Hardware Identifier
 * @versionStatus:             Version status
 * @version:                   Version revision
 * @product:                   Product revision
 * @architecture:              Architecture revison
 */
struct HardwareId {
public:
    HardwareId(uint32_t _versionStatus,
               const SemanticVersion &_version,
               const SemanticVersion &_product,
               const SemanticVersion &_architecture) :
        versionStatus(_versionStatus),
        version(_version), product(_product), architecture(_architecture) {}

    uint32_t versionStatus;
    SemanticVersion version;
    SemanticVersion product;
    SemanticVersion architecture;
};

/*
 * Hardware Configuration
 * @macsPerClockCycle:         MACs per clock cycle
 * @cmdStreamVersion:          NPU command stream version
 * @customDma:                 Custom DMA enabled
 */
struct HardwareConfiguration {
public:
    HardwareConfiguration(uint32_t _macsPerClockCycle, uint32_t _cmdStreamVersion, bool _customDma) :
        macsPerClockCycle(_macsPerClockCycle), cmdStreamVersion(_cmdStreamVersion), customDma(_customDma) {}

    uint32_t macsPerClockCycle;
    uint32_t cmdStreamVersion;
    bool customDma;
};

/**
 * Device capabilities
 * @hwId:                      Hardware
 * @driver:                    Driver revision
 * @hwCfg                      Hardware configuration
 */
class Capabilities {
public:
    Capabilities(const HardwareId &_hwId, const HardwareConfiguration &_hwCfg, const SemanticVersion &_driver) :
        hwId(_hwId), hwCfg(_hwCfg), driver(_driver) {}

    HardwareId hwId;
    HardwareConfiguration hwCfg;
    SemanticVersion driver;
};

class Device {
public:
    Device(const char *device = "/dev/ethosu0");
    virtual ~Device() noexcept(false);

    int ioctl(unsigned long cmd, void *data = nullptr) const;
    Capabilities capabilities() const;

private:
    int fd;
};

class Buffer {
public:
    Buffer(const Device &device, const size_t capacity);
    virtual ~Buffer() noexcept(false);

    size_t capacity() const;
    void clear() const;
    char *data() const;
    void resize(size_t size, size_t offset = 0) const;
    size_t offset() const;
    size_t size() const;

    int getFd() const;

private:
    int fd;
    char *dataPtr;
    const size_t dataCapacity;
};

class Network {
public:
    Network(const Device &device, std::shared_ptr<Buffer> &buffer);
    Network(const Device &device, const unsigned index);
    virtual ~Network() noexcept(false);

    int ioctl(unsigned long cmd, void *data = nullptr);
    std::shared_ptr<Buffer> getBuffer();
    const std::vector<size_t> &getIfmDims() const;
    size_t getIfmSize() const;
    const std::vector<size_t> &getOfmDims() const;
    size_t getOfmSize() const;

    size_t getInputCount() const;
    size_t getOutputCount() const;
    int32_t getInputDataOffset(int index);
    int32_t getOutputDataOffset(int index);
    const std::vector<std::vector<size_t>> &getIfmShapes() const;
    const std::vector<std::vector<size_t>> &getOfmShapes() const;
    const std::vector<int> &getIfmTypes() const;
    const std::vector<int> &getOfmTypes() const;
    const Device &getDevice() const;
    bool isVelaModel() const;

private:
    void collectNetworkInfo();

    int fd;
    std::shared_ptr<Buffer> buffer;
    std::vector<size_t> ifmDims;
    std::vector<size_t> ofmDims;
    std::vector<int32_t> ifmDataOffset;
    std::vector<int32_t> ofmDataOffset;
    std::vector<std::vector<size_t>> ifmShapes;
    std::vector<std::vector<size_t>> ofmShapes;
    std::vector<int> ifmTypes;
    std::vector<int> ofmTypes;
    const Device &device;
    bool _isVelaModel;
};

enum class InferenceStatus {
    OK,
    ERROR,
    RUNNING,
    REJECTED,
    ABORTED,
    ABORTING,
};

std::ostream &operator<<(std::ostream &out, const InferenceStatus &v);

class Inference {
public:
    template <typename T>
    Inference(const std::shared_ptr<Network> &network,
              const T &ifmBegin,
              const T &ifmEnd,
              const T &ofmBegin,
              const T &ofmEnd) :
        network(network) {
        std::copy(ifmBegin, ifmEnd, std::back_inserter(ifmBuffers));
        std::copy(ofmBegin, ofmEnd, std::back_inserter(ofmBuffers));
        std::vector<uint32_t> counterConfigs = initializeCounterConfig();

        // Init tensor arena buffer
        size_t arena_buffer_size = DEFAULT_ARENA_SIZE_OF_MB << 20;
        arenaBuffer = std::make_shared<Buffer>(network->getDevice(), arena_buffer_size);
        arenaBuffer->resize(arena_buffer_size);

        create(counterConfigs, false);
    }
    template <typename T, typename U>
    Inference(const std::shared_ptr<Network> &network,
              const T &ifmBegin,
              const T &ifmEnd,
              const T &ofmBegin,
              const T &ofmEnd,
              const U &counters,
              bool enableCycleCounter) :
        network(network) {
        std::copy(ifmBegin, ifmEnd, std::back_inserter(ifmBuffers));
        std::copy(ofmBegin, ofmEnd, std::back_inserter(ofmBuffers));
        std::vector<uint32_t> counterConfigs = initializeCounterConfig();

        if (counters.size() > counterConfigs.size())
            throw EthosU::Exception("PMU Counters argument to large.");

        // Init tensor arena buffer
        size_t arena_buffer_size = DEFAULT_ARENA_SIZE_OF_MB << 20;
        arenaBuffer = std::make_shared<Buffer>(network->getDevice(), arena_buffer_size);
        arenaBuffer->resize(arena_buffer_size);

        std::copy(counters.begin(), counters.end(), counterConfigs.begin());
        create(counterConfigs, enableCycleCounter);
    }

    template <typename T, typename U>
    Inference(const std::shared_ptr<Network> &network,
              const T &arenaBuffer,
              const U &counters,
              bool enableCycleCounter) :
        network(network), arenaBuffer(arenaBuffer) {

        std::vector<uint32_t> counterConfigs = initializeCounterConfig();

        if (counters.size() > counterConfigs.size())
            throw EthosU::Exception("PMU Counters argument to large.");

        std::copy(counters.begin(), counters.end(), counterConfigs.begin());
        create(counterConfigs, enableCycleCounter);
    }


    virtual ~Inference() noexcept(false);

    bool wait(int64_t timeoutNanos = -1) const;
    const std::vector<uint32_t> getPmuCounters() const;
    uint64_t getCycleCounter() const;
    bool cancel() const;
    InferenceStatus status() const;
    int getFd() const;
    const std::shared_ptr<Network> getNetwork() const;
    std::vector<std::shared_ptr<Buffer>> &getIfmBuffers();
    std::vector<std::shared_ptr<Buffer>> &getOfmBuffers();

    static uint32_t getMaxPmuEventCounters();

    char* getInputData(int index = 0);
    char* getOutputData(int index = 0);

private:
    void create(std::vector<uint32_t> &counterConfigs, bool enableCycleCounter);
    std::vector<uint32_t> initializeCounterConfig();

    int fd;
    const std::shared_ptr<Network> network;
    std::vector<std::shared_ptr<Buffer>> ifmBuffers;
    std::vector<std::shared_ptr<Buffer>> ofmBuffers;
    std::shared_ptr<Buffer> arenaBuffer;
};

struct TensorInfo{
    int type;
    std::vector<size_t> shape;
};

//Define tflite::TensorType here
enum TensorType{
  TensorType_FLOAT32 = 0,
  TensorType_FLOAT16 = 1,
  TensorType_INT32 = 2,
  TensorType_UINT8 = 3,
  TensorType_INT64 = 4,
  TensorType_STRING = 5,
  TensorType_BOOL = 6,
  TensorType_INT16 = 7,
  TensorType_COMPLEX64 = 8,
  TensorType_INT8 = 9,
  TensorType_FLOAT64 = 10,
  TensorType_MIN = TensorType_FLOAT32,
  TensorType_MAX = TensorType_FLOAT64

};

class Interpreter {
public:
    Interpreter(const char *model, const char *device = "/dev/ethosu0",
                int64_t arenaSizeOfMB = DEFAULT_ARENA_SIZE_OF_MB);
    Interpreter(const std::string &model) : Interpreter(model.c_str()) {}

    void SetPmuCycleCounters(std::vector<uint8_t> counters, bool enableCycleCounter = true);
    std::vector<uint32_t> GetPmuCounters();
    uint64_t GetCycleCounter();

    void Invoke(int64_t timeoutNanos = 60000000000);

    template <typename T>
    T* typed_input_buffer(int index) {
        int32_t offset = network->getInputDataOffset(index);
        return (T*)(arenaBuffer->data() + offset);
    }

    template <typename T>
    T* typed_output_buffer(int index) {
        int32_t offset = network->getOutputDataOffset(index);
        return (T*)(arenaBuffer->data() + offset);
    }

    std::vector<TensorInfo> GetInputInfo();
    std::vector<TensorInfo> GetOutputInfo();

private:
    Device device;
    std::shared_ptr<Buffer> networkBuffer;
    std::shared_ptr<Buffer> arenaBuffer;
    std::shared_ptr<Network> network;
    std::shared_ptr<Inference> inference;

    int64_t arenaSizeOfMB;
    std::vector<uint8_t> pmuCounters;
    bool enableCycleCounter;
};

} // namespace EthosU
