#include "CFGCommonRS.h"

void test_case_compression(uint32_t& index, std::vector<uint8_t> input) {
  CFG_POST_MSG("********************** Test Case #%d **********************",
               index);
  std::vector<uint8_t> output;
  std::vector<uint8_t> output_output;
  size_t header_size = 0;
  CFG_compress(&input[0], input.size(), output, &header_size, true, false);
  CFG_ASSERT(header_size < output.size());
  CFG_decompress(&output[0], output.size(), output_output, true);
  CFG_ASSERT(input.size() == output_output.size());
  CFG_ASSERT(memcmp(&input[0], &output_output[0], input.size()) == 0);
  CFG_POST_MSG("!!! Result: Input (%ld) vs Output (%ld) [Header Size: %ld]",
               input.size(), output.size() - header_size, header_size);
  CFG_POST_MSG("***********************************************************");
  index++;
}

void test_compression() {
  CFG_POST_MSG("Compression Test");
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

void test_crc() {
  CFG_POST_MSG("CRC Test");
  uint16_t expected_crc16 = 0xA161;
  const uint8_t* data = (uint8_t*)(const_cast<char*>(
      "\x31\x50\x4F\x42\x01\x00\x01\x00\x01\x00\x00\x00\x80\x4A\x01\x00\x00\x00"
      "\x01\x80\x00\x00\x01\x80\x40\x00\x00\x00\xD0\x4C\x01\x00\x02\x00\x00\x00"
      "\x04\x01\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00"));
  uint16_t crc16 = CFG_bop_A001_crc16(data, 62);
  CFG_ASSERT(crc16 == expected_crc16);
}

int main(int argc, const char** argv) {
  CFG_POST_MSG("This is CFGCommon unit test");
  test_compression();
  test_crc();
  return 0;
}
