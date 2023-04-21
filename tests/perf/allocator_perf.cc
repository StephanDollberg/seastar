/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright (C) 2023 ScyllaDB
 */

#include <seastar/testing/perf_tests.hh>

#include <seastar/core/memory.hh>
#include <sys/mman.h>

struct allocator_test {
    std::size_t _allocs;
    void** _ptr_storage;

    allocator_test() {
        _allocs = 10;
        // use mmap to not hit the seastar allocator in any unneeded fashion
        _ptr_storage = (void**) mmap(0, sizeof(void*) * _allocs, 
            PROT_READ | PROT_WRITE, 
            MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE, -1, 0);
    }

    ~allocator_test() {
        munmap(_ptr_storage, _allocs);
    }
};

PERF_TEST(allocator, single_alloc_and_free_small)
{
    auto ptr = malloc(10);
    perf_tests::do_not_optimize(ptr);
    free(ptr);
}

// this test doesn't serve much value. It should take about 10 times as the
// single alloc test above. If not, something is wrong.
PERF_TEST_F(allocator_test, single_alloc_and_free_small_many)
{
    const std::size_t allocs = 10;

    for (std::size_t i = 0; i < allocs; ++i)
    {
        auto ptr = malloc(10);
        perf_tests::do_not_optimize(ptr);
        _ptr_storage[i] = ptr;
    }

    for (std::size_t i = 0; i < allocs; ++i)
    {
        free(_ptr_storage[i]);
    }
}

// Allocate from more than one page. Should also not suffer a perf penalty 
// As we should have at least min free of 50 objects (hard coded internally)
PERF_TEST_F(allocator_test, single_alloc_and_free_small_many_cross_page)
{
    const std::size_t alloc_size = 1024;
    const std::size_t allocs = (seastar::memory::page_size / alloc_size) + 1;

    for (std::size_t i = 0; i < allocs; ++i)
    {
        auto ptr = malloc(alloc_size);
        perf_tests::do_not_optimize(ptr);
        _ptr_storage[i] = ptr;
    }

    for (std::size_t i = 0; i < allocs; ++i)
    {
        free(_ptr_storage[i]);
    }
}

// Include an allocation in the benchmark that will require going to the large pool
// for more data for the small pool
PERF_TEST_F(allocator_test, single_alloc_and_free_small_many_cross_page_alloc_more)
{
    const std::size_t alloc_size = 1024;
    const std::size_t allocs = 101; // at 1024 alloc size we will have a _max_free of 100 objects

    for (std::size_t i = 0; i < allocs; ++i)
    {
        auto ptr = malloc(alloc_size);
        perf_tests::do_not_optimize(ptr);
        _ptr_storage[i] = ptr;
    }

    for (std::size_t i = 0; i < allocs; ++i)
    {
        free(_ptr_storage[i]);
    }
}

PERF_TEST(allocator, single_alloc_and_free_large)
{
    auto ptr = malloc(32000);
    perf_tests::do_not_optimize(ptr);
    free(ptr);
}
