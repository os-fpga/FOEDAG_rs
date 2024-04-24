#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <tuple>

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

TEST_F(EioIPTest, read_lower_test) {
  ON_CALL(mockAdapter, read(EIO_AXI_DAT_IN)).WillByDefault(Return(0x12345678));
  EioIP eio(&mockAdapter, 0);
  auto result = eio.read(eio_probe_register_t::LOWER);
  ASSERT_EQ(result, 0x12345678);
}

TEST_F(EioIPTest, read_upper_test) {
  ON_CALL(mockAdapter, read(EIO_AXI_DAT_IN + 4))
      .WillByDefault(Return(0x87654321));
  EioIP eio(&mockAdapter, 0);
  auto result = eio.read(eio_probe_register_t::UPPER);
  ASSERT_EQ(result, 0x87654321);
}

TEST_F(EioIPTest, write_lower_test) {
  EXPECT_CALL(mockAdapter, write(EIO_AXI_DAT_IN, 0xa55aa5a5));
  EioIP eio(&mockAdapter, 0);
  eio.write(0xa55aa5a5, eio_probe_register_t::LOWER);
}

TEST_F(EioIPTest, write_upper_test) {
  EXPECT_CALL(mockAdapter, write(EIO_AXI_DAT_IN + 4, 0xdeadbeef));
  EioIP eio(&mockAdapter, 0);
  eio.write(0xdeadbeef, eio_probe_register_t::UPPER);
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
