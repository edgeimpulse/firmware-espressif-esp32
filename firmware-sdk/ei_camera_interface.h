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

#ifndef EI_CAMERA_INTERFACE_H
#define EI_CAMERA_INTERFACE_H

#include <cstdint>

typedef struct {
    uint16_t width;
    uint16_t height;
} ei_device_snapshot_resolutions_t;

class EiCamera {
public:
    /**
     * @brief Call to driver to return an image encoded in RGB88
     * Format should be Big Endian, or in other words, if your image
     * pointer is indexed as a char*, then image[0] is R, image[1] is G
     * image[2] is B, and image[3] is R again (no padding / word alignment)
     *
     * @param image Point to output buffer for image.  32 bit for word alignment on some platforms
     * @param image_size Size of buffer allocated ( should be 3 * width * height )
     * @return true If successful
     * @return false If not successful
     */
    virtual bool ei_camera_capture_rgb888_packed_big_endian(
        uint8_t *image,
        uint32_t image_size)
        {
            // virtual. You must provide an implementation - if your camera supports color
            return true;
        }

    /**
     * @brief Call to driver to return an image encoded in Grayscale
     * Format should be Big Endian, or in other words, if your image
     * pointer is indexed as a char*, then image[0] is R, image[1] is G
     * image[2] is B, and image[3] is R again (no padding / word alignment)
     *
     * @param image Point to output buffer for image.  32 bit for word alignment on some platforms
     * @param image_size Size of buffer allocated ( should be 1 * width * height )
     * @return true If successful
     * @return false If not successful
     */
    virtual bool ei_camera_capture_grayscale_packed_big_endian(
        uint8_t *image,
        uint32_t image_size)
        {
            // virtual. You must provide an implementation - if your camera supports grayscale
            return false;
        }

    /**
     * @brief Get the min resolution supported by camera
     *
     * @return ei_device_snapshot_resolutions_t
     */
    virtual ei_device_snapshot_resolutions_t get_min_resolution(void) = 0;

    /**
     * @brief Get the list of supported resolutions, ie. not requiring
     * any software processing like crop or resize
     *
     * @param res pointer to store the list of resolutions
     * @param res_num pointer to a variable that will contain size of the res list
     */
    virtual void get_resolutions(ei_device_snapshot_resolutions_t **res, uint8_t *res_num) = 0;

    /**
     * @brief Set the camera resolution to desired width and height
     *
     * @param res struct with desired width and height of the snapshot
     * @return true if resolution set successfully
     * @return false if something went wrong
     */
    virtual bool set_resolution(const ei_device_snapshot_resolutions_t res) = 0;

    /**
     * @brief Try to set the camera resolution to required width and height.
     * The method is looking for best possible resolution, applies it to the camera and returns
     * (from the list of natively supported)
     * Usually required resolutions are smaller or the same as min camera resolution, because
     * many cameras support much bigger resolutions that required in TinyML models.
     *
     * @param required_width required width of snapshot
     * @param required_height required height of snapshot
     * @return ei_device_snapshot_resolutions_t returns
     * the best match of sensor supported resolutions
     * to user specified resolution
     */
    virtual ei_device_snapshot_resolutions_t search_resolution(uint32_t required_width, uint32_t required_height)
    {
        ei_device_snapshot_resolutions_t *list;
        ei_device_snapshot_resolutions_t res;
        uint8_t list_size;

        get_resolutions(&list, &list_size);

        res = get_min_resolution();
        // if required res is smaller than the smallest supported,
        // it's easy and just return the min supported resolution
        if(required_width <= res.width && required_height <= res.height) {
            return res;
        }

        // Assuming resolutions list is ordered from smallest to biggest
        // we have to find the smallest resolution in which the required ons is fitting
        for (uint8_t ix = 0; ix < list_size; ix++) {
            if ((required_width <= list[ix].width) && (required_height <= list[ix].height)) {
                return list[ix];
            }
        }

        // Ooops! There is no resolution big enough to cover the required one, return max
        res = list[list_size - 1];
        return res;
    }

    /**
     * @brief Call to driver to initialize camera
     * to capture images in required resolution
     *
     * @param width image width size, in pixels
     * @param height image height size, in pixels
     * @return true if successful
     * @return false if not successful
     */

    virtual bool init(uint16_t width, uint16_t height)
    {
        return true;
    }

    /**
     * @brief Call to driver to deinitialize camera
     * and release all resources (fb, etc).
     *
     * @return true if successful
     * @return false if not successful
     */

    virtual bool deinit()
    {
        return true;
    }

    /**
     * @brief Provides pointer to a framebuffer
     * if camera framebuffer is not available
     * a dedicated framebuffer needs to be
     * created manually
     *
     * @return EiCamera*
     */
    virtual bool get_fb_ptr(uint8_t** fb_ptr)
    {
        return false;
    }

    /**
     * @brief Implementation must provide a singleton getter
     *
     * @return EiCamera*
     */
    static EiCamera *get_camera();

};
#endif /* EI_CAMERA_INTERFACE_H */
