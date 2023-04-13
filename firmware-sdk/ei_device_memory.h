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

#include <cmath>
#include <cstdint>
#include <cstring>

/**
 * @brief Interface class for all memory type storages in Edge Impulse compatible devices.
 * The memory should be organized in a blocks, because all EI sensor drivers depends on block oranization.
 * Additionaly, at least one or two block of memory at the beginning is used for device configuration
 * (see ei_device_info_lib.h)
 */
class EiDeviceMemory {
protected:
    /**
     * @brief Direct read from memory, should be implemented per device/memory type.
     * 
     * @param data pointer to buffer for data to be read
     * @param address absolute address in the memory (format and value depending on memory chip/type)
     * @param num_bytes number of bytes to be read @refitem data should be at leas num_bytes long
     * @return uint32_t number of bytes actually read. If differs from num_bytes, then some error occured.
     */
    virtual uint32_t read_data(uint8_t *data, uint32_t address, uint32_t num_bytes) = 0;

    /**
     * @brief Direct write to memory, should be implemented per device/memory type.
     * 
     * @param data pointer to bufer with data to write
     * @param address absolute address in the memory (format and value depending on memory chip/type)
     * @param num_bytes number of bytes to write
     * @return uint32_t number of bytes that has been written, if differs from num_bytes some error occured.
     */
    virtual uint32_t write_data(const uint8_t *data, uint32_t address, uint32_t num_bytes) = 0;

    /**
     * @brief Erase memory region
     * 
     * @param address absolute address in memory whwere the erase should begin. Typically block aligned.
     * @param num_bytes number of bytes to be erased, typically multiple of block size.
     * @return uint32_t numer of bytes that has been erased, if differes from num_bytes, then some oerror occured.
     */
    virtual uint32_t erase_data(uint32_t address, uint32_t num_bytes) = 0;

    /**
     * @brief size of device configuration block, device or even firmware specific.
     * 
     */
    uint32_t config_size;
    /**
     * @brief number of blocks occupied by config. Typically 1, but depending on memory
     * type and config size, it can be multiple blocks.
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
     * @brief Erase time of a single block/sector/page in ms (miliseconds).
     * For RAM it can be set to 0 or 1.
     * For Flash memories take this value from datasheet or measure.
     */
    uint32_t block_erase_time;

    /**
     * @brief Construct a new Ei Device Memory object, make sure to pass all necessary data
     * from derived class. Usually constructor of the derived class needs to get this data 
     * from user code or read from chip.
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
        : config_size(config_size)
        , used_blocks((config_size < block_size) ? 1 : ceil(float(config_size) / block_size))
        , memory_blocks(memory_size / block_size)
        , memory_size(memory_size)
        , block_size(block_size)
        , block_erase_time(erase_time)
    {
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
     * For the targets, that don't require it, a default dummy implementation is provided
     * to reduce boilerplate code in target flash implementation file.
     */
    virtual uint32_t flush_data(void)
    {
        return 0;
    }
};

template <int BLOCK_SIZE = 512, int MEMORY_BLOCKS = 8> class EiDeviceRAM : public EiDeviceMemory {

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

    /**
     * @brief for RAM memory, we don't need to care about the blocks and
     * pack data one after another for better memory utilization.
     * 
     */
    uint32_t
    read_sample_data(uint8_t *sample_data, uint32_t address, uint32_t sample_data_size) override
    {
        return this->read_data(sample_data, config_size + address, sample_data_size);
    }

    uint32_t write_sample_data(
        const uint8_t *sample_data,
        uint32_t address,
        uint32_t sample_data_size) override
    {
        return this->write_data(sample_data, config_size + address, sample_data_size);
    }

    uint32_t erase_sample_data(uint32_t address, uint32_t num_bytes) override
    {
        return this->erase_data(config_size + address, num_bytes);
    }
};

#endif /* EI_DEVICE_MEMORY_H */
