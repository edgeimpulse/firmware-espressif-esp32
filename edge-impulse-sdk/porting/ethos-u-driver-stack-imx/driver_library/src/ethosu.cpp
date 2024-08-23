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

#ifdef EI_ETHOS_LINUX
#include "../include/ethosu.hpp"
#include "../../kernel_driver/include/uapi/ethosu.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

using namespace std;

namespace {

enum class Severity { Error, Warning, Info, Debug };

class Log {
public:
    Log(const Severity _severity = Severity::Error) : severity(_severity) {}

    ~Log() = default;

    template <typename T>
    const Log &operator<<(const T &d) const {
        if (level >= severity) {
            cout << d;
        }

        return *this;
    }

    const Log &operator<<(ostream &(*manip)(ostream &)) const {
        if (level >= severity) {
            manip(cout);
        }

        return *this;
    }

private:
    static Severity getLogLevel() {
        if (const char *e = getenv("ETHOSU_LOG_LEVEL")) {
            const string env(e);

            if (env == "Error") {
                return Severity::Error;
            } else if (env == "Warning") {
                return Severity::Warning;
            } else if (env == "Info") {
                return Severity::Info;
            } else if (env == "Debug") {
                return Severity::Debug;
            } else {
                cerr << "Unsupported log level '" << env << "'" << endl;
            }
        }

        return Severity::Warning;
    }

    static const Severity level;
    const Severity severity;
};

const Severity Log::level = Log::getLogLevel();

} // namespace

namespace EthosU {
__attribute__((weak)) int eioctl(int fd, unsigned long cmd, void *data = nullptr) {
    int ret = ::ioctl(fd, cmd, data);
    if (ret < 0) {
        throw EthosU::Exception("IOCTL failed");
    }

    Log(Severity::Debug) << "ioctl. fd=" << fd << ", cmd=" << setw(8) << setfill('0') << hex << cmd << ", ret=" << ret
                         << endl;

    return ret;
}

__attribute__((weak)) int eopen(const char *pathname, int flags) {
    int fd = ::open(pathname, flags);
    if (fd < 0) {
        throw Exception("Failed to open device");
    }

    Log(Severity::Debug) << "open. fd=" << fd << ", path='" << pathname << "', flags=" << flags << endl;

    return fd;
}

__attribute__((weak)) int
eppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *tmo_p, const sigset_t *sigmask) {
    int result = ::ppoll(fds, nfds, tmo_p, sigmask);
    if (result < 0) {
        throw Exception("Failed to wait for ppoll event or signal");
    }

    return result;
}

__attribute__((weak)) int eclose(int fd) {
    Log(Severity::Debug) << "close. fd=" << fd << endl;

    int result = ::close(fd);
    if (result < 0) {
        throw Exception("Failed to close file");
    }

    return result;
}
__attribute((weak)) void *emmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    void *ptr = ::mmap(addr, length, prot, flags, fd, offset);
    if (ptr == MAP_FAILED) {
        throw Exception("Failed to mmap file");
    }

    Log(Severity::Debug) << "map. fd=" << fd << ", addr=" << setfill('0') << addr << ", length=" << dec << length
                         << ", ptr=" << hex << ptr << endl;

    return ptr;
}

__attribute__((weak)) int emunmap(void *addr, size_t length) {
    Log(Severity::Debug) << "unmap. addr=" << setfill('0') << addr << ", length=" << dec << length << endl;

    int result = ::munmap(addr, length);
    if (result < 0) {
        throw Exception("Failed to munmap file");
    }

    return result;
}

} // namespace EthosU

namespace EthosU {

/****************************************************************************
 * Exception
 ****************************************************************************/

Exception::Exception(const char *msg) : msg(msg) {}

Exception::~Exception() throw() {}

const char *Exception::what() const throw() {
    return msg.c_str();
}

/****************************************************************************
 * Semantic Version
 ****************************************************************************/

bool SemanticVersion::operator==(const SemanticVersion &other) {
    return other.major == major && other.minor == minor && other.patch == patch;
}

bool SemanticVersion::operator<(const SemanticVersion &other) {
    if (other.major > major)
        return true;
    if (other.minor > minor)
        return true;
    return other.patch > patch;
}

bool SemanticVersion::operator<=(const SemanticVersion &other) {
    return *this < other || *this == other;
}

bool SemanticVersion::operator!=(const SemanticVersion &other) {
    return !(*this == other);
}

bool SemanticVersion::operator>(const SemanticVersion &other) {
    return !(*this <= other);
}

bool SemanticVersion::operator>=(const SemanticVersion &other) {
    return !(*this < other);
}

ostream &operator<<(ostream &out, const SemanticVersion &v) {
    return out << "{ major=" << unsigned(v.major) << ", minor=" << unsigned(v.minor) << ", patch=" << unsigned(v.patch)
               << " }";
}

/****************************************************************************
 * Device
 ****************************************************************************/
Device::Device(const char *device) {
    fd = eopen(device, O_RDWR | O_NONBLOCK);
    Log(Severity::Info) << "Device(\"" << device << "\"). this=" << this << ", fd=" << fd << endl;
}

Device::~Device() noexcept(false) {
    eclose(fd);
    Log(Severity::Info) << "~Device(). this=" << this << endl;
}

int Device::ioctl(unsigned long cmd, void *data) const {
    return eioctl(fd, cmd, data);
}

Capabilities Device::capabilities() const {
    ethosu_uapi_device_capabilities uapi;
    (void)eioctl(fd, ETHOSU_IOCTL_CAPABILITIES_REQ, static_cast<void *>(&uapi));

    Capabilities capabilities(
        HardwareId(uapi.hw_id.version_status,
                   SemanticVersion(uapi.hw_id.version_major, uapi.hw_id.version_minor),
                   SemanticVersion(uapi.hw_id.product_major),
                   SemanticVersion(uapi.hw_id.arch_major_rev, uapi.hw_id.arch_minor_rev, uapi.hw_id.arch_patch_rev)),
        HardwareConfiguration(uapi.hw_cfg.macs_per_cc, uapi.hw_cfg.cmd_stream_version, bool(uapi.hw_cfg.custom_dma)),
        SemanticVersion(uapi.driver_major_rev, uapi.driver_minor_rev, uapi.driver_patch_rev));
    return capabilities;
}

/****************************************************************************
 * Buffer
 ****************************************************************************/

Buffer::Buffer(const Device &device, const size_t capacity) : fd(-1), dataPtr(nullptr), dataCapacity(capacity) {
    ethosu_uapi_buffer_create uapi = {static_cast<uint32_t>(dataCapacity)};
    fd                             = device.ioctl(ETHOSU_IOCTL_BUFFER_CREATE, static_cast<void *>(&uapi));

    void *d;
    try {
        d = emmap(nullptr, dataCapacity, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    } catch (std::exception &e) {
        try {
            eclose(fd);
        } catch (...) { std::throw_with_nested(e); }
        throw;
    }

    dataPtr = reinterpret_cast<char *>(d);

    Log(Severity::Info) << "Buffer(" << &device << ", " << dec << capacity << "), this=" << this << ", fd=" << fd
                        << ", dataPtr=" << static_cast<void *>(dataPtr) << endl;
}

Buffer::~Buffer() noexcept(false) {
    try {
        emunmap(dataPtr, dataCapacity);
    } catch (std::exception &e) {
        try {
            eclose(fd);
        } catch (...) { std::throw_with_nested(e); }
        throw;
    }

    eclose(fd);

    Log(Severity::Info) << "~Buffer(). this=" << this << endl;
}

size_t Buffer::capacity() const {
    return dataCapacity;
}

void Buffer::clear() const {
    resize(0, 0);
}

char *Buffer::data() const {
    return dataPtr + offset();
}

void Buffer::resize(size_t size, size_t offset) const {
    ethosu_uapi_buffer uapi;
    uapi.offset = offset;
    uapi.size   = size;
    eioctl(fd, ETHOSU_IOCTL_BUFFER_SET, static_cast<void *>(&uapi));
}

size_t Buffer::offset() const {
    ethosu_uapi_buffer uapi;
    eioctl(fd, ETHOSU_IOCTL_BUFFER_GET, static_cast<void *>(&uapi));
    return uapi.offset;
}

size_t Buffer::size() const {
    ethosu_uapi_buffer uapi;
    eioctl(fd, ETHOSU_IOCTL_BUFFER_GET, static_cast<void *>(&uapi));
    return uapi.size;
}

int Buffer::getFd() const {
    return fd;
}

/****************************************************************************
 * Network
 ****************************************************************************/

Network::Network(const Device &device, shared_ptr<Buffer> &buffer) : device(device), fd(-1), buffer(buffer) {
    // Create buffer handle
    ethosu_uapi_network_create uapi;
    uapi.type = ETHOSU_UAPI_NETWORK_BUFFER;
    uapi.fd   = buffer->getFd();
    fd        = device.ioctl(ETHOSU_IOCTL_NETWORK_CREATE, static_cast<void *>(&uapi));
    try {
        collectNetworkInfo();
    } catch (std::exception &e) {
        try {
            eclose(fd);
        } catch (...) { std::throw_with_nested(e); }
        throw;
    }

    Log(Severity::Info) << "Network(" << &device << ", " << &*buffer << "), this=" << this << ", fd=" << fd << endl;
}

Network::Network(const Device &device, const unsigned index) : device(device), fd(-1) {
    // Create buffer handle
    ethosu_uapi_network_create uapi;
    uapi.type  = ETHOSU_UAPI_NETWORK_INDEX;
    uapi.index = index;
    fd         = device.ioctl(ETHOSU_IOCTL_NETWORK_CREATE, static_cast<void *>(&uapi));
    try {
        collectNetworkInfo();
    } catch (std::exception &e) {
        try {
            eclose(fd);
        } catch (...) { std::throw_with_nested(e); }
        throw;
    }

    Log(Severity::Info) << "Network(" << &device << ", " << index << "), this=" << this << ", fd=" << fd << endl;
}

void Network::collectNetworkInfo() {
    ethosu_uapi_network_info info;
    ioctl(ETHOSU_IOCTL_NETWORK_INFO, static_cast<void *>(&info));

    _isVelaModel = info.is_vela;

    for (uint32_t i = 0; i < info.ifm_count; i++) {
        ifmDims.push_back(info.ifm_size[i]);
        ifmTypes.push_back(info.ifm_types[i]);
        ifmDataOffset.push_back(info.ifm_offset[i]);

        std::vector<size_t> shape;
        for (uint32_t j = 0; j < info.ifm_dims[i]; j++) {
            shape.push_back(info.ifm_shapes[i][j]);
        }
        ifmShapes.push_back(shape);
    }

    for (uint32_t i = 0; i < info.ofm_count; i++) {
        ofmDims.push_back(info.ofm_size[i]);
        ofmTypes.push_back(info.ofm_types[i]);
        ofmDataOffset.push_back(info.ofm_offset[i]);

        std::vector<size_t> shape;
        for (uint32_t j = 0; j < info.ofm_dims[i]; j++) {
            shape.push_back(info.ofm_shapes[i][j]);
        }
        ofmShapes.push_back(shape);
    }
}

Network::~Network() noexcept(false) {
    eclose(fd);
    Log(Severity::Info) << "~Network(). this=" << this << endl;
}

int Network::ioctl(unsigned long cmd, void *data) {
    return eioctl(fd, cmd, data);
}

shared_ptr<Buffer> Network::getBuffer() {
    return buffer;
}

const std::vector<size_t> &Network::getIfmDims() const {
    return ifmDims;
}

size_t Network::getIfmSize() const {
    size_t size = 0;

    for (auto s : ifmDims) {
        size += s;
    }

    return size;
}

const std::vector<size_t> &Network::getOfmDims() const {
    return ofmDims;
}

size_t Network::getOfmSize() const {
    size_t size = 0;

    for (auto s : ofmDims) {
        size += s;
    }

    return size;
}

size_t Network::getInputCount() const {
    return ifmTypes.size();
}

size_t Network::getOutputCount() const {
    return ofmTypes.size();
}

int32_t Network::getInputDataOffset(int index){
    if (index >= ifmDataOffset.size()){
        throw Exception("Invalid input index or non vela model");
    }
    return ifmDataOffset[index];
}

int32_t Network::getOutputDataOffset(int index){
    if (index >= ofmDataOffset.size()){
        throw Exception("Invalid output index or non vela model");
    }
    return ofmDataOffset[index];
}

const std::vector<std::vector<size_t>> &Network::getIfmShapes() const {
    return ifmShapes;
}

const std::vector<std::vector<size_t>> &Network::getOfmShapes() const {
    return ofmShapes;
}

const std::vector<int> &Network::getIfmTypes() const {
    return ifmTypes;
}

const std::vector<int> &Network::getOfmTypes() const {
    return ofmTypes;
}

const Device &Network::getDevice() const {
    return device;
}

bool Network::isVelaModel() const {
    return _isVelaModel;
}

/****************************************************************************
 * Inference
 ****************************************************************************/

ostream &operator<<(ostream &out, const InferenceStatus &status) {
    switch (status) {
    case InferenceStatus::OK:
        return out << "ok";
    case InferenceStatus::ERROR:
        return out << "error";
    case InferenceStatus::RUNNING:
        return out << "running";
    case InferenceStatus::REJECTED:
        return out << "rejected";
    case InferenceStatus::ABORTED:
        return out << "aborted";
    case InferenceStatus::ABORTING:
        return out << "aborting";
    }
    throw Exception("Unknown inference status");
}

Inference::~Inference() noexcept(false) {
    eclose(fd);
    Log(Severity::Info) << "~Inference(). this=" << this << endl;
}

void Inference::create(std::vector<uint32_t> &counterConfigs, bool cycleCounterEnable = false) {
    ethosu_uapi_inference_create uapi;

    if (ifmBuffers.size() > ETHOSU_FD_MAX) {
        throw Exception("IFM buffer overflow");
    }

    if (ofmBuffers.size() > ETHOSU_FD_MAX) {
        throw Exception("OFM buffer overflow");
    }

    if (counterConfigs.size() != ETHOSU_PMU_EVENT_MAX) {
        throw Exception("Wrong size of counter configurations");
    }

    uapi.ifm_count = 0;
    uapi.ifm_fd[uapi.ifm_count++] = arenaBuffer->getFd();
    for (auto it : ifmBuffers) {
        uapi.ifm_fd[uapi.ifm_count++] = it->getFd();
    }

    uapi.ofm_count = 0;
    for (auto it : ofmBuffers) {
        uapi.ofm_fd[uapi.ofm_count++] = it->getFd();
    }

    for (int i = 0; i < ETHOSU_PMU_EVENT_MAX; i++) {
        uapi.pmu_config.events[i] = counterConfigs[i];
    }

    uapi.pmu_config.cycle_count = cycleCounterEnable;

    fd = network->ioctl(ETHOSU_IOCTL_INFERENCE_CREATE, static_cast<void *>(&uapi));

    Log(Severity::Info) << "Inference(" << &*network << "), this=" << this << ", fd=" << fd << endl;
}

std::vector<uint32_t> Inference::initializeCounterConfig() {
    return std::vector<uint32_t>(ETHOSU_PMU_EVENT_MAX, 0);
}

uint32_t Inference::getMaxPmuEventCounters() {
    return ETHOSU_PMU_EVENT_MAX;
}

bool Inference::wait(int64_t timeoutNanos) const {
    struct pollfd pfd;
    pfd.fd      = fd;
    pfd.events  = POLLIN | POLLERR;
    pfd.revents = 0;

    // if timeout negative wait forever
    if (timeoutNanos < 0) {
        return eppoll(&pfd, 1, NULL, NULL);
    }

    struct timespec tmo_p;
    int64_t nanosec = 1000000000;
    tmo_p.tv_sec    = timeoutNanos / nanosec;
    tmo_p.tv_nsec   = timeoutNanos % nanosec;

    return eppoll(&pfd, 1, &tmo_p, NULL) == 0;
}

bool Inference::cancel() const {
    ethosu_uapi_cancel_inference_status uapi;
    eioctl(fd, ETHOSU_IOCTL_INFERENCE_CANCEL, static_cast<void *>(&uapi));
    return uapi.status == ETHOSU_UAPI_STATUS_OK;
}

InferenceStatus Inference::status() const {
    ethosu_uapi_result_status uapi;

    eioctl(fd, ETHOSU_IOCTL_INFERENCE_STATUS, static_cast<void *>(&uapi));

    switch (uapi.status) {
    case ETHOSU_UAPI_STATUS_OK:
        return InferenceStatus::OK;
    case ETHOSU_UAPI_STATUS_ERROR:
        return InferenceStatus::ERROR;
    case ETHOSU_UAPI_STATUS_RUNNING:
        return InferenceStatus::RUNNING;
    case ETHOSU_UAPI_STATUS_REJECTED:
        return InferenceStatus::REJECTED;
    case ETHOSU_UAPI_STATUS_ABORTED:
        return InferenceStatus::ABORTED;
    case ETHOSU_UAPI_STATUS_ABORTING:
        return InferenceStatus::ABORTING;
    }

    throw Exception("Unknown inference status");
}

const std::vector<uint32_t> Inference::getPmuCounters() const {
    ethosu_uapi_result_status uapi;
    std::vector<uint32_t> counterValues = std::vector<uint32_t>(ETHOSU_PMU_EVENT_MAX, 0);

    eioctl(fd, ETHOSU_IOCTL_INFERENCE_STATUS, static_cast<void *>(&uapi));

    for (int i = 0; i < ETHOSU_PMU_EVENT_MAX; i++) {
        if (uapi.pmu_config.events[i]) {
            counterValues.at(i) = uapi.pmu_count.events[i];
        }
    }

    return counterValues;
}

uint64_t Inference::getCycleCounter() const {
    ethosu_uapi_result_status uapi;

    eioctl(fd, ETHOSU_IOCTL_INFERENCE_STATUS, static_cast<void *>(&uapi));

    return uapi.pmu_count.cycle_count;
}

int Inference::getFd() const {
    return fd;
}

const shared_ptr<Network> Inference::getNetwork() const {
    return network;
}

vector<shared_ptr<Buffer>> &Inference::getIfmBuffers() {
    return ifmBuffers;
}

vector<shared_ptr<Buffer>> &Inference::getOfmBuffers() {
    return ofmBuffers;
}

/****************************************************************************
 * Interpreter
 ****************************************************************************/
Interpreter::Interpreter(const char *model, const char *_device, int64_t _arenaSizeOfMB):
             device(_device), arenaSizeOfMB(_arenaSizeOfMB){
    //Send capabilities request
    Capabilities capabilities = device.capabilities();

    cout << "Capabilities:" << endl
         << "\tversion_status:" << unsigned(capabilities.hwId.versionStatus) << endl
         << "\tversion:" << capabilities.hwId.version << endl
         << "\tproduct:" << capabilities.hwId.product << endl
         << "\tarchitecture:" << capabilities.hwId.architecture << endl
         << "\tdriver:" << capabilities.driver << endl
         << "\tmacs_per_cc:" << unsigned(capabilities.hwCfg.macsPerClockCycle) << endl
         << "\tcmd_stream_version:" << unsigned(capabilities.hwCfg.cmdStreamVersion) << endl
         << "\tcustom_dma:" << std::boolalpha << capabilities.hwCfg.customDma << endl;

    // Init network
    ifstream stream(model, ios::binary);
    if (!stream.is_open()) {
         throw Exception("Failed to open model file");
    }

    stream.seekg(0, ios_base::end);
    size_t size = stream.tellg();
    stream.seekg(0, ios_base::beg);

    networkBuffer = make_shared<Buffer>(device, size);
    networkBuffer->resize(size);
    stream.read(networkBuffer->data(), size);
    network = make_shared<Network>(device, networkBuffer);
    if (!network->isVelaModel()) {
         throw Exception("Only support models compiled by vela.");
    }

    // Init tensor arena buffer
    size_t arena_buffer_size = arenaSizeOfMB << 20;
    arenaBuffer = make_shared<Buffer>(device, arena_buffer_size);
    arenaBuffer->resize(arena_buffer_size);
}

void Interpreter::SetPmuCycleCounters(vector<uint8_t> counters, bool cycleCounter) {
    if (counters.size() != ETHOSU_PMU_EVENT_MAX){
        throw Exception("PMU event count is invalid.");
    }

    pmuCounters = counters;
    enableCycleCounter = cycleCounter;
}

void Interpreter::Invoke(int64_t timeoutNanos) {
    inference = make_shared<Inference>(network, arenaBuffer,
                                pmuCounters, enableCycleCounter);
    inference->wait(timeoutNanos);

    if (inference->status() != InferenceStatus::OK) {
        throw Exception("Failed to invoke.");
    }
}

std::vector<uint32_t> Interpreter::GetPmuCounters() {
    return inference->getPmuCounters();
}

uint64_t Interpreter::GetCycleCounter() {
    return inference->getCycleCounter();
}

std::vector<TensorInfo> Interpreter::GetInputInfo() {
    std::vector<TensorInfo> ret;
    auto types = network->getIfmTypes();
    auto shapes = network->getIfmShapes();

    for (int i = 0; i < network->getInputCount(); i ++) {
        ret.push_back(TensorInfo{types[i], shapes[i]});
    }

    return ret;
}

std::vector<TensorInfo> Interpreter::GetOutputInfo(){
    std::vector<TensorInfo> ret;
    auto types = network->getOfmTypes();
    auto shapes = network->getOfmShapes();

    for (int i = 0; i < network->getOutputCount(); i ++) {
        ret.push_back(TensorInfo{types[i], shapes[i]});
    }

    return ret;
}

} // namespace EthosU

#endif // EI_ETHOS_LINUX
