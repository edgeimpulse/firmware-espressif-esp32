/* Edge Impulse firmware SDK
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
        uint32_t image_size) = 0; //pure virtual.  You must provide an implementation

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
     * @brief Implementation must provide a singleton getter
     * 
     * @return EiCamera* 
     */
    static EiCamera *get_camera();
};
#endif /* EI_CAMERA_INTERFACE_H */
