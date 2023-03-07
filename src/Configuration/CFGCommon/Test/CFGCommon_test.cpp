/*
Copyright 2021 The Foedag team

GPL License

Copyright (c) 2021 The Open-Source FPGA Foundation

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>

#include "CFGHelper.h"
#include "CFGMessager.h"

void test_case_compression(uint32_t& index, std::vector<uint8_t> input) {
  printf("********************** Test Case #%d **********************\n",
         index);
  std::vector<uint8_t> output;
  std::vector<uint8_t> output_output;
  size_t header_size = 0;
  CFG_compress(&input[0], input.size(), output, &header_size, true, false);
  CFG_ASSERT(header_size < output.size());
  CFG_decompress(&output[0], output.size(), output_output, true);
  CFG_ASSERT(input.size() == output_output.size());
  CFG_ASSERT(memcmp(&input[0], &output_output[0], input.size()) == 0);
  printf("!!! Result: Input (%ld) vs Output (%ld) [Header Size: %ld]\n",
         input.size(), output.size() - header_size, header_size);
  printf("***********************************************************\n");
  index++;
}

void test_compression() {
  printf("Compression Test\n");
  uint32_t index = 0;
  test_case_compression(index, {0, 0, 0, 1, 2, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6});

  test_case_compression(index, {1, 2});

  test_case_compression(index, {1, 2, 3});

  test_case_compression(index, {1, 2, 3, 3});

  test_case_compression(index, {1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3});

  test_case_compression(index, {0, 0, 3, 0, 0, 3, 0, 0, 3, 0, 0, 3});

  test_case_compression(index, {1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0});

  test_case_compression(index,
                        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 1});

  test_case_compression(index,
                        {1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});

  test_case_compression(
      index, {1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 2, 3, 4});

  test_case_compression(
      index, {1, 2, 3, 3, 4, 5, 6, 0, 0, 1, 2, 3, 4, 4, 5, 6, 6, 0, 0});
}
int main(int argc, char** argv) {
  printf("This is CFGCommon unit test\n");
  test_compression();
}
