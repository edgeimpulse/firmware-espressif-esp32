/* Edge Impulse inferencing library
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

#ifndef HR_PPG_HPP
#define HR_PPG_HPP

#include "edge-impulse-sdk/dsp/numpy.hpp"
#include "edge-impulse-sdk/dsp/ei_dsp_handle.h"
#include "edge-impulse-enterprise/hr/hr_ppg.hpp"
#include "edge-impulse-enterprise/hr/hrv.hpp"
#include "edge-impulse-sdk/dsp/memory.hpp"

// Need a wrapper to get ei_malloc used
// cppyy didn't like this override for some reason
class hrv_wrap : public ei::hrv::beats_to_hrv {
public:
    // Boilerplate below here
    void* operator new(size_t size) {
        // Custom memory allocation logic here
        return ei_malloc(size);
    }

    void operator delete(void* ptr) {
        // Custom memory deallocation logic here
        ei_free(ptr);
    }
    // end boilerplate

    // Use the same constructor as the parent
    using ei::hrv::beats_to_hrv::beats_to_hrv;
};

class hr_class : public DspHandle {
public:
    int print() override {
        ei_printf("Last HR: %f\n", ppg._res.hr);
        return ei::EIDSP_OK;
    }

    int extract(ei::signal_t *signal, ei::matrix_t *output_matrix, void *config_ptr, const float frequency) override {
        using namespace ei;

        // Using reference avoids a copy
        ei_dsp_config_hr_t& config = *((ei_dsp_config_hr_t*)config_ptr);
        size_t floats_per_inc = ppg.win_inc_samples * ppg.axes;
        size_t hrv_inc_samples = config.hrv_update_interval_s * frequency * ppg.axes;
        // Greater than b/c can have multiple increments (HRs) per window
        assert(signal->total_length >= floats_per_inc);

        int out_idx = 0;
        size_t hrv_count = 0;
        for (size_t i = 0; i <= signal->total_length - floats_per_inc; i += floats_per_inc) {
            // TODO ask for smaller increments and bp them into place
            // Copy into the end of the buffer
            matrix_t temp(ppg.win_inc_samples, ppg.axes);
            signal->get_data(i, floats_per_inc, temp.buffer);
            auto hr = ppg.stream(&temp);
            if(!hrv || (hrv && config.include_hr)) {
                output_matrix->buffer[out_idx++] = hr;
            }
            if(hrv) {
                auto peaks = ppg.get_peaks();
                hrv->add_streaming_beats(peaks);
                hrv_count += floats_per_inc;
                if(hrv_count >= hrv_inc_samples) {
                    fvec features = hrv->get_hrv_features(0);
                    for(size_t j = 0; j < features.size(); j++) {
                        output_matrix->buffer[out_idx++] = features[j];
                    }
                    hrv_count = 0;
                }
            }
        }
        return EIDSP_OK;
    }

    hr_class(ei_dsp_config_hr_t* config, float frequency)
        : ppg(frequency,
              config->axes,
              int(frequency * config->hr_win_size_s),
              int(frequency * 2), // TODO variable overlap
              config->filter_preset,
              config->acc_resting_std,
              config->sensitivity,
              true), hrv(nullptr)
    {
        auto table = config->named_axes;
        for( size_t i = 0; i < config->named_axes_size; i++ ) {
            ppg.set_offset_table(i, table[i].axis);
        }
        // if not "none"
        if(strcmp(config->hrv_features,"none") != 0) {
            // new is overloaded to use ei_malloc
            hrv = new hrv_wrap(
                frequency,
                config->hrv_features,
                config->hrv_update_interval_s,
                config->hrv_win_size_s,
                2); // TODO variable window?
        }
    }

    ~hr_class() {
        if(hrv) {
            // delete is overloaded to use ei_free
            delete hrv;
        }
    }

    float get_last_hr() {
        return ppg._res.hr;
    }

    // Boilerplate below here
    static DspHandle* create(void* config, float frequency);

    void* operator new(size_t size) {
        // Custom memory allocation logic here
        return ei_malloc(size);
    }

    void operator delete(void* ptr) {
        // Custom memory deallocation logic here
        ei_free(ptr);
    }
    // end boilerplate
private:
    ei::hr_ppg ppg;
    hrv_wrap* hrv; // pointer b/c we don't always need it
};

DspHandle* hr_class::create(void* config_in, float frequency) { // NOLINT def in header is OK at EI
    auto config = reinterpret_cast<ei_dsp_config_hr_t*>(config_in);
    return new hr_class(config, frequency);
};

/*
NOTE, contact EI sales for license and source to use EI heart rate and heart rate variance functions in deployment
*/

#endif