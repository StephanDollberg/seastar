/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This file has been originally been imported from:
// https://cs.android.com/android/platform/superproject/+/013901367630d3ec71c9f2bb3f3077bd11585301:external/perfetto/src/profiling/memory/sampler.h

//
// The code has been modified as follows:
//
//  - Integrated into seastar and adapted to coding style
//  - Right now we don't account for samples multiple times (in case we have
//  multiple loops of drawing from the exp distribution). The reason is that
//  in our memory sampler we would have to store the weight in addition to the
//  alloation site ptr as on free we need to know how much a sample accounted
//  for. Hence, for now we simply always use the sampling interval.
//  - Sampler can be turned "off" with a 0 sampling rate
//  - The fast path is more optimized (as a consequence of the first point)
//
// Changes Copyright (C) 2023 ScyllaDB

#pragma once

#include <random>

// See also: https://perfetto.dev/docs/design-docs/heapprofd-sampling for more
// background of how the sampler works

class sampler {
public:
    sampler() : random_gen(rd_device()) {
        set_sampling_interval(0);
    }
    /// Sets the sampling interval in bytes. Setting it to 0 means to never sample
    void set_sampling_interval(uint64_t sampling_interval) {
        sampling_interval_ = sampling_interval;
        if (sampling_interval_ == 0) {
            // Set the interval very large. This means in practice we will
            // likely never get this below zero and hence it's unlikely we will
            // ever have to run the reset path with sampling off
            interval_to_next_sample_ = std::numeric_limits<int64_t>::max();
            return;
        }
        sampling_rate_ = 1.0 / static_cast<double>(sampling_interval_);
        interval_to_next_sample_ = next_sampling_interval();
    }
    /// Returns true if this allocation of size `alloc_size` should be sampled
    bool should_sample(size_t alloc_size) {
        // We need to check the sampling_interval as 0 means off. Also we need
        // to check whether this allocation has brought the interval
        // below/to 0. We chose to check the interval_to_next_sample first. If
        // heap profiling is compiled in it's likely we are using it as well.
        // Hence, lets do the interval decrement and check first and only check
        // whether sampling is off after.
        interval_to_next_sample_ -= alloc_size;
        if (interval_to_next_sample_ > 0) {
            return false;
        }
        reset_interval_to_next_sample(alloc_size);
        return sampling_interval_ != 0;
    }

    uint64_t sampling_interval() const { return sampling_interval_; }

    /// How much should an allocation of size `allocation_size` count for
    size_t sample_size(size_t allocation_size) const {
        return std::max(allocation_size, sampling_interval_);
    }

private:
    /// Resets interval_to_next_sample_ by repeatedly drawing from the
    /// exponential distribution given an allocation of size `alloc_size`
    /// breached the current interval
    void reset_interval_to_next_sample(size_t alloc_size)
    {
        if (sampling_interval_ == 0) { // sampling is off
            interval_to_next_sample_ = std::numeric_limits<int64_t>::max();
        }
        else {
            // Large allocations we will just consider in whole. This avoids
            // having to sample the distribution too many times if a large alloc
            // took us very negative we just add the alloc size back on
            if (alloc_size > sampling_interval_) {
                interval_to_next_sample_ += alloc_size;
            }
            else {
                while (interval_to_next_sample_ <= 0) {
                    interval_to_next_sample_ += next_sampling_interval();
                }
            }
        }
    }

    int64_t next_sampling_interval() {
        std::exponential_distribution<double> dist(sampling_rate_);
        int64_t next = static_cast<int64_t>(dist(random_gen));
        // We approximate the geometric distribution using an exponential
        // distribution.
        // We need to add 1 because that gives us the number of failures before
        // the next success, while our interval includes the next success.
        return next + 1;
    }

    uint64_t sampling_interval_; // Sample every N bytes ; 0 means off
    double sampling_rate_; // 1 / sampling_interval_ ; used by the exp distribution
    int64_t interval_to_next_sample_; // Next time we are going to take a sample
    std::random_device rd_device;
    std::mt19937_64 random_gen;
};