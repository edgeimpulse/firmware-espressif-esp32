/* Edge Impulse ingestion SDK
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Include ----------------------------------------------------------------- */
#include "flash_memory.h"

static const char *TAG = "FlashDriver";

/** Align addres to given sector size */
#define SECTOR_ALIGN(a, sec_size) ((a & (sec_size - 1)) ? (a & ~(sec_size - 1)) + sec_size : a)

uint32_t EiFlashMemory::read_data(uint8_t *data, uint32_t address, uint32_t num_bytes)
{

    esp_err_t ret = esp_partition_read(partition, address, (uint8_t *)data, num_bytes);

    if (ret != ESP_OK) num_bytes = 0;

    return num_bytes;
}

uint32_t EiFlashMemory::write_data(const uint8_t *data, uint32_t address, uint32_t num_bytes)
{

    esp_err_t ret = esp_partition_write(partition, address, data, num_bytes);

    if (ret != ESP_OK) num_bytes = 0;

    return num_bytes;
}

uint32_t EiFlashMemory::erase_data(uint32_t address, uint32_t num_bytes)
{
    /**
     * Address can point to the middle of sector, but num_bytes may be reaching
     *  part of the last sector
     * +-------+-------+-------+-------+
     * |       |       |       |       |
     * +-------+-------+-------+-------+
     *     ^
     *     address
     *     <-----num_bytes--------->
     */

    esp_err_t ret = esp_partition_erase_range(partition, address, SECTOR_ALIGN(num_bytes, SPI_FLASH_SEC_SIZE));

    if (ret != ESP_OK) num_bytes = 0;

    return num_bytes;
}

EiFlashMemory::EiFlashMemory(uint32_t config_size):
    EiDeviceMemory(config_size, ESP32_FS_BLOCK_ERASE_TIME_MS, 0, SPI_FLASH_SEC_SIZE)
{

    // Find the partition map in the partition table
    partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "storage");
    assert(partition != NULL);

    ESP_LOGI(TAG, "Found partition '%s' at offset 0x%x with size 0x%x\n", partition->label, partition->address, partition->size);

    memory_size = partition->size; 

    ESP_LOGI(TAG, "memory_size %d used_blocks %d\n", memory_size, used_blocks);

}
