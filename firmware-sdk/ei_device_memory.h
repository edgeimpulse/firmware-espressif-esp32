/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EI_DEVICE_MEMORY_H
#define EI_DEVICE_MEMORY_H

#include <cstdint>
#include <cstring>

/**
 * @brief Interface class for all memory type storages in Edge Impulse compatible devices.
 * The memory should be organized in blocks because all EI sensor drivers depend on block organization.
 * Additionally, at least one or two blocks of memory at the beginning is used for device configuration
 * (see ei_device_info_lib.h)
 */
class EiDeviceMemory {
protected:
    /**
     * @brief Direct read from memory should be implemented per device/memory type.
     *
     * @param data pointer to buffer for data to be read
     * @param address absolute address in the memory (format and value depending on memory chip/type)
     * @param num_bytes number of bytes to be read @refitem data should be at least num_bytes long
     * @return uint32_t number of bytes read. If it differs from num_bytes, then some error occurs.
     */
    virtual uint32_t read_data(uint8_t *data, uint32_t address, uint32_t num_bytes) = 0;

    /**
     * @brief Direct write to memory should be implemented per device/memory type.
     *
     * @param data pointer to buffer with data to write
     * @param address absolute address in the memory (format and value depending on memory chip/type)
     * @param num_bytes number of bytes to write
     * @return uint32_t number of bytes that have been written, if differs from num_bytes some error occurred.
     */
    virtual uint32_t write_data(const uint8_t *data, uint32_t address, uint32_t num_bytes) = 0;

    /**
     * @brief Erase memory region
     *
     * @param address absolute address in memory where the erase should begin. Typically block aligned.
     * @param num_bytes number of bytes to be erased, typically multiple of block size.
     * @return uint32_t number of bytes that have been erased, if differs from num_bytes, then some error occurred.
     */
    virtual uint32_t erase_data(uint32_t address, uint32_t num_bytes) = 0;

    /**
     * @brief number of blocks occupied by config. Typically 1, but depending on memory
     * type and config size: it can be multiple blocks.
     *
     */
    uint32_t used_blocks;
    /**
     * @brief total number of blocks in the memory
     *
     */
    uint32_t memory_blocks;
    /**
     * @brief total size of memory, typically integer multiply of blocks
     *
     */
    uint32_t memory_size;

public:
    /**
     * @brief size of the memory block in bytes
     */
    uint32_t block_size;
    /**
     * @brief Erase time of a single block/sector/page in ms (milliseconds).
     * For RAM, it can be set to 0 or 1.
     * For Flash memories, take this value from the datasheet or measure.
     */
    uint32_t block_erase_time;

    /**
     * @brief Construct a new Ei Device Memory object and make sure to pass all necessary data
     * from the derived class. Usually, the derived class's constructor needs to get this data
     * from user code or read from the chip.
     *
     * @param config_size see property description
     * @param erase_time see property description
     * @param memory_size see property description
     * @param block_size see property description
     */
    EiDeviceMemory(
        uint32_t config_size,
        uint32_t erase_time,
        uint32_t memory_size,
        uint32_t block_size)
        : memory_blocks(block_size == 0 ? 0 : memory_size / block_size)
        , memory_size(memory_size)
        , block_size(block_size)
        , block_erase_time(erase_time)
    {
        if(config_size == 0) {
            // this means we are not storing the config
            used_blocks = 0;
        }
        else if(config_size < block_size) {
            used_blocks = 1;
        }
        else {
            if(block_size == 0) {
                used_blocks = 0;
            }
            else {
                used_blocks = (config_size % block_size == 0) ? (config_size / block_size) : (config_size / block_size + 1);
            }
        }
    }

    virtual uint32_t get_available_sample_blocks(void)
    {
        return memory_blocks - used_blocks;
    }

    virtual uint32_t get_available_sample_bytes(void)
    {
        return (memory_blocks - used_blocks) * block_size;
    }

    virtual bool save_config(const uint8_t *config, uint32_t config_size)
    {
        uint32_t used_bytes = used_blocks * block_size;

        // this means we have no space for config
        if(used_bytes == 0) {
            return false;
        }

        if (erase_data(0, used_bytes) != used_bytes) {
            return false;
        }

        if (write_data(config, 0, config_size) != config_size) {
            return false;
        }

        return true;
    }

    virtual bool load_config(uint8_t *config, uint32_t config_size)
    {
        if (read_data(config, 0, config_size) != config_size) {
            return false;
        }
        return true;
    }

    virtual uint32_t
    read_sample_data(uint8_t *sample_data, uint32_t address, uint32_t sample_data_size)
    {
        uint32_t offset = used_blocks * block_size;

        return read_data(sample_data, offset + address, sample_data_size);
    }

    virtual uint32_t
    write_sample_data(const uint8_t *sample_data, uint32_t address, uint32_t sample_data_size)
    {
        uint32_t offset = used_blocks * block_size;

        return write_data(sample_data, offset + address, sample_data_size);
    }

    virtual uint32_t erase_sample_data(uint32_t address, uint32_t num_bytes)
    {
        uint32_t offset = used_blocks * block_size;

        return erase_data(offset + address, num_bytes);
    }


    /**
     * @brief Necessary for targets, such as RP2040, which have large Flash page size (256 bytes)
     * For the targets, that doesn't require it, a default dummy implementation is provided
     * to reduce boilerplate code in the target flash implementation file.
     */
    virtual uint32_t flush_data(void)
    {
        return 0;
    }

    /**
     * @brief Set the up sampling, required for SD card device
     * 
     */
    virtual bool setup_sampling(const char* sensor_name, const char* lable_name)
    {
        return true;
    }

    /**
     * @brief 
     * 
     */
    virtual void finalize_samplig(void)
    {
        
    }
};

template <int BLOCK_SIZE = 1024, int MEMORY_BLOCKS = 4> class EiDeviceRAM : public EiDeviceMemory {

protected:
    uint8_t ram_memory[MEMORY_BLOCKS * BLOCK_SIZE];

    uint32_t read_data(uint8_t *data, uint32_t address, uint32_t num_bytes) override
    {
        if (num_bytes > memory_size - address) {
            num_bytes = memory_size - address;
        }

        memcpy(data, &ram_memory[address], num_bytes);

        return num_bytes;
    }

    uint32_t write_data(const uint8_t *data, uint32_t address, uint32_t num_bytes) override
    {
        if (num_bytes > memory_size - address) {
            num_bytes = memory_size - address;
        }

        memcpy(&ram_memory[address], data, num_bytes);

        return num_bytes;
    }

    uint32_t erase_data(uint32_t address, uint32_t num_bytes) override
    {
        if (num_bytes > memory_size - address) {
            num_bytes = memory_size - address;
        }

        memset(&ram_memory[address], 0, num_bytes);

        return num_bytes;
    }

public:
    EiDeviceRAM(uint32_t config_size)
        : EiDeviceMemory(config_size, 0, BLOCK_SIZE * MEMORY_BLOCKS, BLOCK_SIZE)
    {
    }
};

#endif /* EI_DEVICE_MEMORY_H */
