#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <tuple>
#include <vector>

#include "EioIP.h"
#include "OclaJtagAdapter.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class MockOclaJtagAdapter : public OclaJtagAdapter {
 public:
  MOCK_METHOD(void, write, (uint32_t addr, uint32_t data), (override));
  MOCK_METHOD(uint32_t, read, (uint32_t addr), (override));
  MOCK_METHOD(std::vector<jtag_read_result>, read,
              (uint32_t base_addr, uint32_t num_reads, uint32_t increase_by),
              (override));
  MOCK_METHOD(void, set_target_device,
              (FOEDAG::Device device, std::vector<FOEDAG::Tap> taps),
              (override));
};

class EioIPTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
  NiceMock<MockOclaJtagAdapter> mockAdapter;
};

TEST_F(EioIPTest, read_zero_test) {
  EioIP eio(&mockAdapter, 0);
  EXPECT_THROW(eio.read_input_bits(0), std::exception);
}

TEST_F(EioIPTest, read_three_test) {
  EioIP eio(&mockAdapter, 0);
  EXPECT_THROW(eio.read_input_bits(3), std::exception);
}

TEST_F(EioIPTest, readback_output_zero_test) {
  EioIP eio(&mockAdapter, 0);
  EXPECT_THROW(eio.readback_output_bits(0), std::exception);
}

TEST_F(EioIPTest, readback_output_three_test) {
  EioIP eio(&mockAdapter, 0);
  EXPECT_THROW(eio.readback_output_bits(3), std::exception);
}

TEST_F(EioIPTest, read_one_test) {
  ON_CALL(mockAdapter, read(EIO_AXI_DAT_IN, 1, 4))
      .WillByDefault(Return(std::vector<jtag_read_result>{{0, 0x12345678, 0}}));
  ON_CALL(mockAdapter, read(EIO_CTRL)).WillByDefault(Return(0x1));
  EXPECT_CALL(mockAdapter, write(EIO_CTRL, 0x0));
  EioIP eio(&mockAdapter, 0);
  auto result = eio.read_input_bits(1);
  ASSERT_EQ(result.size(), 1);
  ASSERT_EQ(result[0], 0x12345678);
}

TEST_F(EioIPTest, read_two_test) {
  ON_CALL(mockAdapter, read(EIO_AXI_DAT_IN, 2, 4))
      .WillByDefault(Return(std::vector<jtag_read_result>{{0, 0x87654321, 0},
                                                          {0, 0x12345678, 0}}));
  ON_CALL(mockAdapter, read(EIO_CTRL)).WillByDefault(Return(0x0));
  EXPECT_CALL(mockAdapter, write(EIO_CTRL, _)).Times(0);
  EioIP eio(&mockAdapter, 0);
  auto result = eio.read_input_bits(2);
  ASSERT_EQ(result.size(), 2);
  ASSERT_EQ(result[0], 0x87654321);
  ASSERT_EQ(result[1], 0x12345678);
}

TEST_F(EioIPTest, write_zero_test) {
  EioIP eio(&mockAdapter, 0);
  EXPECT_THROW(eio.write_output_bits({}, 0), std::exception);
}

TEST_F(EioIPTest, write_three_test) {
  EioIP eio(&mockAdapter, 0);
  EXPECT_THROW(eio.write_output_bits({1, 2, 3}, 3), std::exception);
}

TEST_F(EioIPTest, write_input_mismatch_test) {
  EioIP eio(&mockAdapter, 0);
  EXPECT_THROW(eio.write_output_bits({1}, 2), std::exception);
}

TEST_F(EioIPTest, write_one_test) {
  EXPECT_CALL(mockAdapter, write(EIO_AXI_DAT_IN, 0xa55aa5a5));
  EioIP eio(&mockAdapter, 0);
  eio.write_output_bits({0xa55aa5a5}, 1);
}

TEST_F(EioIPTest, write_two_test) {
  EXPECT_CALL(mockAdapter, write(EIO_AXI_DAT_IN, 0xdeadbeef));
  EXPECT_CALL(mockAdapter, write(EIO_AXI_DAT_IN + 4, 0xa55aa5a5));
  EioIP eio(&mockAdapter, 0);
  eio.write_output_bits({0xdeadbeef, 0xa55aa5a5}, 2);
}

TEST_F(EioIPTest, get_type_test) {
  ON_CALL(mockAdapter, read(EIO_IP_TYPE)).WillByDefault(Return(0x0045494f));
  EioIP eio(&mockAdapter, 0);
  auto result = eio.get_type();
  ASSERT_EQ(result, "EIO");
}

TEST_F(EioIPTest, get_version_test) {
  ON_CALL(mockAdapter, read(EIO_IP_VERSION)).WillByDefault(Return(0x1));
  EioIP eio(&mockAdapter, 0);
  auto result = eio.get_version();
  ASSERT_EQ(result, 0x1);
}

TEST_F(EioIPTest, get_id_test) {
  ON_CALL(mockAdapter, read(EIO_IP_ID)).WillByDefault(Return(0x012343210));
  EioIP eio(&mockAdapter, 0);
  auto result = eio.get_id();
  ASSERT_EQ(result, 0x012343210);
}

TEST_F(EioIPTest, get_prs_mode_probe_out_test) {
  ON_CALL(mockAdapter, read(0x1234 + EIO_CTRL)).WillByDefault(Return(0x1));
  EioIP eio(&mockAdapter, 0x1234);
  auto result = eio.get_prs_mode();
  ASSERT_EQ(result, eio_prs_mode::PROBE_OUT);
}

TEST_F(EioIPTest, get_prs_mode_probe_in_test) {
  ON_CALL(mockAdapter, read(0x1234 + EIO_CTRL)).WillByDefault(Return(0x0));
  EioIP eio(&mockAdapter, 0x1234);
  auto result = eio.get_prs_mode();
  ASSERT_EQ(result, eio_prs_mode::PROBE_IN);
}

TEST_F(EioIPTest, set_prs_mode_probe_out_test) {
  ON_CALL(mockAdapter, read(0x4321 + EIO_CTRL)).WillByDefault(Return(0x0));
  EXPECT_CALL(mockAdapter, write(0x4321 + EIO_CTRL, 0x1));
  EioIP eio(&mockAdapter, 0x4321);
  eio.set_prs_mode(eio_prs_mode::PROBE_OUT);
  auto result = eio.get_prs_mode();
  ASSERT_EQ(result, eio_prs_mode::PROBE_OUT);
}

TEST_F(EioIPTest, set_prs_mode_probe_in_test) {
  ON_CALL(mockAdapter, read(0x4321 + EIO_CTRL)).WillByDefault(Return(0x1));
  EXPECT_CALL(mockAdapter, write(0x4321 + EIO_CTRL, 0x0));
  EioIP eio(&mockAdapter, 0x4321);
  eio.set_prs_mode(eio_prs_mode::PROBE_IN);
  auto result = eio.get_prs_mode();
  ASSERT_EQ(result, eio_prs_mode::PROBE_IN);
}

TEST_F(EioIPTest, readback_output_one_test) {
  ON_CALL(mockAdapter, read(EIO_AXI_DAT_OUT, 1, 4))
      .WillByDefault(Return(std::vector<jtag_read_result>{{0, 0x12345678, 0}}));
  ON_CALL(mockAdapter, read(EIO_CTRL)).WillByDefault(Return(0x1));
  EXPECT_CALL(mockAdapter, write(EIO_CTRL, _)).Times(0);
  EioIP eio(&mockAdapter, 0);
  auto result = eio.readback_output_bits(1);
  ASSERT_EQ(result.size(), 1);
  ASSERT_EQ(result[0], 0x12345678);
}

TEST_F(EioIPTest, readback_output_two_test) {
  ON_CALL(mockAdapter, read(EIO_AXI_DAT_OUT, 2, 4))
      .WillByDefault(Return(std::vector<jtag_read_result>{{0, 0x87654321, 0},
                                                          {0, 0x12345678, 0}}));
  ON_CALL(mockAdapter, read(EIO_CTRL)).WillByDefault(Return(0x0));
  EXPECT_CALL(mockAdapter, write(EIO_CTRL, 0x1));
  EioIP eio(&mockAdapter, 0);
  auto result = eio.readback_output_bits(2);
  ASSERT_EQ(result.size(), 2);
  ASSERT_EQ(result[0], 0x87654321);
  ASSERT_EQ(result[1], 0x12345678);
}
