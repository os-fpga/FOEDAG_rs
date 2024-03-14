#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <tuple>

#include "OclaIP.h"
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

class OclaIPTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  NiceMock<MockOclaJtagAdapter> mockAdapter;
};

TEST_F(OclaIPTest, getStatusTest_DataAvailable) {
  ON_CALL(mockAdapter, read(OCSR)).WillByDefault(Return(0x12340000 + 1));
  OclaIP oclaIP(&mockAdapter, 0);
  auto result = oclaIP.get_status();
  ASSERT_EQ(result, DATA_AVAILABLE);
}

TEST_F(OclaIPTest, getStatusTest_DataNotAvailable) {
  ON_CALL(mockAdapter, read(OCSR)).WillByDefault(Return(0x12340000 + 0));
  OclaIP oclaIP(&mockAdapter, 0);
  auto result = oclaIP.get_status();
  ASSERT_EQ(result, NA);
}

TEST_F(OclaIPTest, getNumberOfProbesTest) {
  ON_CALL(mockAdapter, read(UIDP1)).WillByDefault(Return(0xffff0000 + 333));
  OclaIP oclaIP(&mockAdapter, 0);
  auto result = oclaIP.get_number_of_probes();
  ASSERT_EQ(333, result);
}

TEST_F(OclaIPTest, getMemoryDepthTest) {
  ON_CALL(mockAdapter, read(UIDP0)).WillByDefault(Return(0xffff0000 + 444));
  OclaIP oclaIP(&mockAdapter, 0);
  auto result = oclaIP.get_memory_depth();
  ASSERT_EQ(444, result);
}

TEST_F(OclaIPTest, getTriggerCountTest) {
  ON_CALL(mockAdapter, read(OCSR)).WillByDefault(Return(((15u << 24) + 0x123)));
  OclaIP oclaIP(&mockAdapter, 0);
  auto result = oclaIP.get_trigger_count();
  ASSERT_EQ(16, result);
}

TEST_F(OclaIPTest, getMaxCompareValueSizeTest) {
  ON_CALL(mockAdapter, read(OCSR))
      .WillByDefault(Return(((1u << 29) + 0x34123)));
  OclaIP oclaIP(&mockAdapter, 0);
  auto result = oclaIP.get_max_compare_value_size();
  ASSERT_EQ(64, result);
}

TEST_F(OclaIPTest, getIdTest) {
  ON_CALL(mockAdapter, read(IP_ID)).WillByDefault(Return(1234));
  OclaIP oclaIP(&mockAdapter, 0);
  auto result = oclaIP.get_id();
  ASSERT_EQ(1234, result);
}

TEST_F(OclaIPTest, getTypeTest) {
  ON_CALL(mockAdapter, read(IP_TYPE)).WillByDefault(Return(0x41424344));
  OclaIP oclaIP(&mockAdapter, 0);
  auto result = oclaIP.get_type();
  ASSERT_EQ("ABCD", result);
}

TEST_F(OclaIPTest, getVersionTest) {
  ON_CALL(mockAdapter, read(IP_VERSION)).WillByDefault(Return(123456789));
  OclaIP oclaIP(&mockAdapter, 0);
  auto result = oclaIP.get_version();
  ASSERT_EQ(123456789, result);
}

TEST_F(OclaIPTest, configureTest) {
  ocla_config cfg;
  cfg.condition = ocla_trigger_condition::XOR;
  cfg.mode = ocla_trigger_mode::PRE;
  cfg.enable_fix_sample_size = true;
  cfg.sample_size = 1234;
  EXPECT_CALL(mockAdapter, write(TMTR, 0x4d101d));
  OclaIP oclaIP(&mockAdapter, 0);
  oclaIP.configure(cfg);
}

TEST_F(OclaIPTest, configureTest_FNS_Disable) {
  ocla_config cfg;
  cfg.condition = ocla_trigger_condition::XOR;
  cfg.mode = ocla_trigger_mode::PRE;
  cfg.enable_fix_sample_size = false;
  cfg.sample_size = 1234;
  EXPECT_CALL(mockAdapter, write(TMTR, 0x0d));
  OclaIP oclaIP(&mockAdapter, 0);
  oclaIP.configure(cfg);
}

TEST_F(OclaIPTest, configureChannelTest_Edge) {
  ON_CALL(mockAdapter, read(OCSR)).WillByDefault(Return(3 << 24));
  ON_CALL(mockAdapter, read(TSSR + 0x90)).WillByDefault(Return(0x3ff));
  ON_CALL(mockAdapter, read(TCUR + 0x90)).WillByDefault(Return(0xffffffff));
  ON_CALL(mockAdapter, read(TDCR + 0x90)).WillByDefault(Return(0xffffffff));

  ocla_trigger_config cfg;

  cfg.type = ocla_trigger_type::EDGE;
  cfg.event = ocla_trigger_event::RISING;
  cfg.probe_num = 7;
  cfg.compare_width = 0;
  cfg.value = 0;

  EXPECT_CALL(mockAdapter, write(TSSR + 0x90, 7)).Times(1);
  EXPECT_CALL(mockAdapter, write(TCUR + 0x90, 0xfffffff5)).Times(1);
  EXPECT_CALL(mockAdapter, write(TDCR + 0x90, 0xffffffff)).Times(1);

  OclaIP oclaIP(&mockAdapter, 0);
  oclaIP.configure_channel(3, cfg);
}

TEST_F(OclaIPTest, configureChannelTest_Level) {
  ON_CALL(mockAdapter, read(OCSR)).WillByDefault(Return(3 << 24));
  ON_CALL(mockAdapter, read(TSSR + 0x30)).WillByDefault(Return(0x3ff));
  ON_CALL(mockAdapter, read(TCUR + 0x30)).WillByDefault(Return(0xffffffff));
  ON_CALL(mockAdapter, read(TDCR + 0x30)).WillByDefault(Return(0xffffffff));

  ocla_trigger_config cfg;

  cfg.type = ocla_trigger_type::LEVEL;
  cfg.event = ocla_trigger_event::LOW;
  cfg.probe_num = 999;
  cfg.compare_width = 0;
  cfg.value = 0;

  EXPECT_CALL(mockAdapter, write(TSSR + 0x30, 999)).Times(1);
  EXPECT_CALL(mockAdapter, write(TCUR + 0x30, 0xffffffce)).Times(1);
  EXPECT_CALL(mockAdapter, write(TDCR + 0x30, 0xffffffff)).Times(1);

  OclaIP oclaIP(&mockAdapter, 0);
  oclaIP.configure_channel(1, cfg);
}

TEST_F(OclaIPTest, configureChannelTest_ValueCompare) {
  ON_CALL(mockAdapter, read(OCSR)).WillByDefault(Return(3 << 24));
  ON_CALL(mockAdapter, read(TSSR + 0x60)).WillByDefault(Return(0x3ff));
  ON_CALL(mockAdapter, read(TCUR + 0x60)).WillByDefault(Return(0xffffffff));
  ON_CALL(mockAdapter, read(TDCR + 0x60)).WillByDefault(Return(0xffffffff));

  ocla_trigger_config cfg;

  cfg.type = ocla_trigger_type::VALUE_COMPARE;
  cfg.event = ocla_trigger_event::EQUAL;
  cfg.probe_num = 21;
  cfg.compare_width = 12;
  cfg.value = 1234;

  EXPECT_CALL(mockAdapter, write(TSSR + 0x60, (11u << 24) + 21)).Times(1);
  EXPECT_CALL(mockAdapter, write(TCUR + 0x60, 0xffffff7f)).Times(1);
  EXPECT_CALL(mockAdapter, write(TDCR + 0x60, 1234)).Times(1);

  OclaIP oclaIP(&mockAdapter, 0);
  oclaIP.configure_channel(2, cfg);
}

TEST_F(OclaIPTest, startTest) {
  EXPECT_CALL(mockAdapter, write(OCCR, 0x1));
  OclaIP oclaIP(&mockAdapter, 0);
  oclaIP.start();
}

TEST_F(OclaIPTest, resetTest) {
  EXPECT_CALL(mockAdapter, write(OCCR, 0x2));
  OclaIP oclaIP(&mockAdapter, 0);
  oclaIP.reset();
}

TEST_F(OclaIPTest, getDataTest_FixSampleSize) {
  ON_CALL(mockAdapter, read(TMTR))
      .WillByDefault(Return((127u << 12) + (1u << 4)));
  ON_CALL(mockAdapter, read(UIDP0)).WillByDefault(Return(1000));
  ON_CALL(mockAdapter, read(UIDP1)).WillByDefault(Return(33));
  EXPECT_CALL(mockAdapter, read(TBDR, 256, 0))
      .Times(1)
      .WillOnce(Return(std::vector<jtag_read_result>(256, {0, 0, 0})));

  OclaIP oclaIP(&mockAdapter, 0);
  auto result = oclaIP.get_data();
  EXPECT_EQ(128, result.depth);
  EXPECT_EQ(33, result.width);
  EXPECT_EQ(2, result.num_reads);
  EXPECT_EQ(256, result.values.size());
}

TEST_F(OclaIPTest, getDataTest_End2End) {
  ON_CALL(mockAdapter, read(UIDP0)).WillByDefault(Return(1000));
  ON_CALL(mockAdapter, read(UIDP1)).WillByDefault(Return(65));

  ocla_config cfg;

  cfg.condition = ocla_trigger_condition::OR;
  cfg.mode = ocla_trigger_mode::POST;
  cfg.enable_fix_sample_size = true;
  cfg.sample_size = 988;

  EXPECT_CALL(mockAdapter, read(TBDR, 988 * 3, 0))
      .Times(1)
      .WillOnce(Return(std::vector<jtag_read_result>{988 * 3, {0, 0, 0}}));

  OclaIP oclaIP(&mockAdapter, 0);
  oclaIP.configure(cfg);
  auto result = oclaIP.get_data();
  EXPECT_EQ(988, result.depth);
  EXPECT_EQ(65, result.width);
  EXPECT_EQ(3, result.num_reads);
  EXPECT_EQ(988 * 3, result.values.size());
}

TEST_F(OclaIPTest, getDataTest_ConfigReadback) {
  ON_CALL(mockAdapter, read(UIDP0)).WillByDefault(Return(1000));
  ON_CALL(mockAdapter, read(UIDP1)).WillByDefault(Return(65));
  ON_CALL(mockAdapter, read(OCSR)).WillByDefault(Return((5u << 24)));

  ocla_trigger_config cfg;

  cfg.type = ocla_trigger_type::VALUE_COMPARE;
  cfg.event = ocla_trigger_event::EQUAL;
  cfg.probe_num = 17;
  cfg.value = 123;
  cfg.compare_width = 12;

  OclaIP oclaIP(&mockAdapter, 0);
  oclaIP.configure_channel(5, cfg);
  auto result = oclaIP.get_channel_config(5);
  EXPECT_EQ(cfg.type, result.type);
  EXPECT_EQ(cfg.event, result.event);
  EXPECT_EQ(cfg.probe_num, result.probe_num);
  EXPECT_EQ(cfg.value, result.value);
  EXPECT_EQ(cfg.compare_width, result.compare_width);
}

TEST_F(OclaIPTest, getConfigTest_default) {
  OclaIP oclaIP(&mockAdapter, 0);
  ocla_config configData = oclaIP.get_config();
  EXPECT_EQ(ocla_trigger_condition::DEFAULT, configData.condition);
  EXPECT_EQ(ocla_trigger_mode::CONTINUOUS, configData.mode);
  EXPECT_EQ(false, configData.enable_fix_sample_size);
  EXPECT_EQ(1, configData.sample_size);
}

TEST_F(OclaIPTest, getConfigTest) {
  ON_CALL(mockAdapter, read(TMTR)).WillByDefault(Return(0x59ac5599));
  OclaIP oclaIP(&mockAdapter, 0);
  ocla_config configData = oclaIP.get_config();
  EXPECT_EQ(ocla_trigger_condition::OR, configData.condition);
  EXPECT_EQ(ocla_trigger_mode::PRE, configData.mode);
  EXPECT_EQ(true, configData.enable_fix_sample_size);
  EXPECT_EQ(367302, configData.sample_size);
}

TEST_F(OclaIPTest, getChannelConfigTest_Edge) {
  ON_CALL(mockAdapter, read(OCSR)).WillByDefault(Return(0x3000000));
  ON_CALL(mockAdapter, read(TSSR + 0x90)).WillByDefault(Return(0x3e7));
  ON_CALL(mockAdapter, read(TCUR + 0x90))
      .WillByDefault(Return(0xfffffff0 + 0x9));
  OclaIP oclaIP(&mockAdapter, 0);
  ocla_trigger_config configData = oclaIP.get_channel_config(3);
  EXPECT_EQ(ocla_trigger_type::EDGE, configData.type);
  EXPECT_EQ(ocla_trigger_event::FALLING, configData.event);
  EXPECT_EQ(999, configData.probe_num);
  EXPECT_EQ(0, configData.value);
  EXPECT_EQ(0, configData.compare_width);
}

TEST_F(OclaIPTest, getChannelConfigTest_Level) {
  ON_CALL(mockAdapter, read(OCSR)).WillByDefault(Return(0x3000000));
  ON_CALL(mockAdapter, read(TSSR + 0x90)).WillByDefault(Return(123));
  ON_CALL(mockAdapter, read(TCUR + 0x90))
      .WillByDefault(Return(0xffffff00 + 0x1e));
  OclaIP oclaIP(&mockAdapter, 0);
  ocla_trigger_config configData = oclaIP.get_channel_config(3);
  EXPECT_EQ(ocla_trigger_type::LEVEL, configData.type);
  EXPECT_EQ(ocla_trigger_event::HIGH, configData.event);
  EXPECT_EQ(123, configData.probe_num);
  EXPECT_EQ(0, configData.value);
  EXPECT_EQ(0, configData.compare_width);
}

TEST_F(OclaIPTest, getChannelConfigTest_ValueCompare) {
  ON_CALL(mockAdapter, read(OCSR)).WillByDefault(Return(0x3000000));
  ON_CALL(mockAdapter, read(TSSR + 0x90))
      .WillByDefault(Return((7 << 24) + 123));
  ON_CALL(mockAdapter, read(TCUR + 0x90))
      .WillByDefault(Return(0xffffff00 + 0x7f));
  ON_CALL(mockAdapter, read(TDCR + 0x90)).WillByDefault(Return(99));
  OclaIP oclaIP(&mockAdapter, 0);
  ocla_trigger_config configData = oclaIP.get_channel_config(3);
  EXPECT_EQ(ocla_trigger_type::VALUE_COMPARE, configData.type);
  EXPECT_EQ(ocla_trigger_event::EQUAL, configData.event);
  EXPECT_EQ(123, configData.probe_num);
  EXPECT_EQ(99, configData.value);
  EXPECT_EQ(8, configData.compare_width);
}

TEST_F(OclaIPTest, getChannelConfigTest_default) {
  OclaIP oclaIP(&mockAdapter, 0);
  ocla_trigger_config configData = oclaIP.get_channel_config(0);
  EXPECT_EQ(ocla_trigger_type::TRIGGER_NONE, configData.type);
  EXPECT_EQ(ocla_trigger_event::NONE, configData.event);
  EXPECT_EQ(0, configData.probe_num);
  EXPECT_EQ(0, configData.value);
  EXPECT_EQ(0, configData.compare_width);
}
