/*
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS
 * IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
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
