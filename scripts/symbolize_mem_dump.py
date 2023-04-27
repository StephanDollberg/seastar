#!/usr/bin/env python3
#
# This file is open source software, licensed to you under the terms
# of the Apache License, Version 2.0 (the "License").  See the NOTICE file
# distributed with this work for additional information regarding copyright
# ownership.  You may not use this file except in compliance with the License.
#
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
# Copyright (C) 2023 ScyllaDB

import addr2line
import fileinput
import sys

# Expects N lines of format:
# size count backtrace
# For example:
# 12344 2 0x1234 0x1456 0x34667

# To be invoked like:
# something_producing_output_as_above | symbolize_mem_dump.py /path/to/binary | flamegraph.pl - > flamegraph.svg

def filter_line(unfiltered_line):
    res = []
    # check for inlined info
    split_lines = unfiltered_line.split('\n')
    # print(split_lines)

    for line in split_lines:
        at_location_index = line.find(' at ')
        if at_location_index != -1:
            # print(at_location_index)
            line = line[:at_location_index]
        
        # print(line)

        if line == '' or line == '\n':
            continue

        if line.startswith('??'):
            continue

        line = line.strip()

        inline_prefix = '(inlined by) '
        if line.startswith(inline_prefix):
            line = line[len(inline_prefix):]

        if 'current_backtrace_tasklocal' in line:
            continue

        if line == 'seastar::memory::get_backtrace()':
            continue

        if line == 'operator()':
            continue

        res.append(line)

    return res 


def filter_lines(symbolized_lines):
    res = []
    for line in symbolized_lines:
        lines = filter_line(line)
        if lines is not None:
            res.extend(lines)
    return res 


if __name__ == '__main__':
    with addr2line.BacktraceResolver(executable=sys.argv[1]) as resolve:
        for line in fileinput.input(files=('-'), encoding='utf-8'):
            # print(line)
            (size, count, stacks) = line.split(' ', 2)
            # print(stacks)
            addresses = stacks.strip().split(' ')
            # print(addresses)
            resolved = [resolve.resolve_address(address) for address in addresses]
            filtered = filter_lines(resolved)
            print('{} {}'.format(';'.join(filtered), size))



