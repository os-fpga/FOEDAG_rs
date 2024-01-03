#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>
#include <memory>

#include "OclaIP.h"
#include "OclaJtagAdapter.h"

using ::testing::_;
using ::testing::Return;

class MockOclaJtagAdapter : public OclaJtagAdapter {
 public:
  MOCK_METHOD(void, write, (uint32_t addr, uint32_t data), (override));
  MOCK_METHOD(uint32_t, read, (uint32_t addr), (override));
  MOCK_METHOD(std::vector<uint32_t>, read,
              (uint32_t base_addr, uint32_t num_reads, uint32_t increase_by),
              (override));
  MOCK_METHOD(void, set_target_device,
              (FOEDAG::Device device, std::vector<FOEDAG::Tap> taps),
              (override));
};

class OclaIPTest : public ::testing::Test {
 protected:
  void SetUp() override {
    baseAddr = 0;
    oclaIP = std::make_shared<OclaIP>(&mockAdapter, baseAddr);
  }

  void TearDown() override {}

  std::shared_ptr<OclaIP> oclaIP;
  uint32_t baseAddr;
  MockOclaJtagAdapter mockAdapter;
};

TEST_F(OclaIPTest, getStatusTest) {
  EXPECT_CALL(mockAdapter, read(baseAddr + OCSR)).WillOnce(Return(1));
  auto result = oclaIP->get_status();
  ASSERT_EQ(result, DATA_AVAILABLE);
}

TEST_F(OclaIPTest, getNumberOfProbesTest) {
  const uint32_t rawNumberOfProbes = 33;
  EXPECT_CALL(mockAdapter, read(baseAddr + OCSR))
      .WillOnce(Return(rawNumberOfProbes));
  auto expectedNumProbes = (rawNumberOfProbes >> 1) & 0x3ff;
  auto result = oclaIP->get_number_of_probes();
  ASSERT_EQ(expectedNumProbes, result);
}

TEST_F(OclaIPTest, getMemoryDepthTest) {
  const uint32_t memoryDepth = 0xffff0000;
  EXPECT_CALL(mockAdapter, read(baseAddr + OCSR)).WillOnce(Return(memoryDepth));
  const uint32_t expectedMemoryDepth = (memoryDepth >> 11) & 0x3ff;
  auto result = oclaIP->get_memory_depth();
  ASSERT_EQ(expectedMemoryDepth, result);
}

TEST_F(OclaIPTest, getIdTest) {
  const uint32_t expectedId = 42;
  EXPECT_CALL(mockAdapter, read(baseAddr + IP_ID)).WillOnce(Return(expectedId));
  auto result = oclaIP->get_id();
  ASSERT_EQ(expectedId, result);
}

TEST_F(OclaIPTest, getTypeTest) {
  const uint32_t expectedTypeUint32 = 0x6F636C61;
  const std::string expectedType = "ocla";
  EXPECT_CALL(mockAdapter, read(baseAddr + IP_TYPE))
      .WillOnce(Return(expectedTypeUint32));
  auto result = oclaIP->get_type();
  ASSERT_EQ(expectedType, result);
}

TEST_F(OclaIPTest, getVersionTest) {
  const uint32_t expectedVersion = 1234;
  EXPECT_CALL(mockAdapter, read(baseAddr + IP_VERSION))
      .WillOnce(Return(expectedVersion));
  auto result = oclaIP->get_version();
  ASSERT_EQ(expectedVersion, result);
}

TEST_F(OclaIPTest, configureTest) {
  ocla_config cfg;
  cfg.fns = ocla_fns::ENABLED;
  cfg.ns = 1000;
  cfg.mode = ocla_mode::PRE;
  cfg.condition = ocla_trigger_condition::DEFAULT;

  uint32_t tmtr = 0x3E89;
  EXPECT_CALL(mockAdapter, write(baseAddr + TMTR, tmtr));

  for (auto &reg : {TCUR0, TCUR1}) {
    uint32_t tcur = 0x2000;
    EXPECT_CALL(mockAdapter, read(baseAddr + reg)).WillOnce(Return(tcur));
    EXPECT_CALL(mockAdapter, write(baseAddr + reg, tcur));
  }
  oclaIP->configure(cfg);
}

TEST_F(OclaIPTest, configureTriggerTriggerNodeTest) {
  uint32_t channel = 1;  // channel 1 to 4
  ocla_trigger_config trig_cfg;
  trig_cfg.type = ocla_trigger_type::TRIGGER_NONE;
  trig_cfg.event = ocla_trigger_event::EQUAL;
  trig_cfg.value = 333;
  trig_cfg.probe_num = 1;

  EXPECT_CALL(mockAdapter, read(baseAddr + TCUR0)).WillOnce(Return(1));
  EXPECT_CALL(mockAdapter, write(baseAddr + TCUR0, _));
  oclaIP->configure_channel(channel, trig_cfg);
}

TEST_F(OclaIPTest, configureTriggerValueCompareTest) {
  uint32_t channel = 3;  // channel 1 to 4
  ocla_trigger_config trig_cfg;
  trig_cfg.type = ocla_trigger_type::VALUE_COMPARE;
  trig_cfg.event = ocla_trigger_event::EQUAL;
  trig_cfg.value = 444;
  trig_cfg.probe_num = 1;

  EXPECT_CALL(mockAdapter, read(baseAddr + TCUR1)).WillOnce(Return(1));
  EXPECT_CALL(mockAdapter, write(baseAddr + TDCR, trig_cfg.value));
  EXPECT_CALL(mockAdapter, write(baseAddr + TCUR1, _));
  oclaIP->configure_channel(channel, trig_cfg);
}

TEST_F(OclaIPTest, startTest) {
  EXPECT_CALL(mockAdapter, read(baseAddr + TCUR0)).WillOnce(Return(1));
  EXPECT_CALL(mockAdapter, write(baseAddr + TCUR0, 1));
  uint32_t tmtr = 0x20;
  EXPECT_CALL(mockAdapter, read(baseAddr + TMTR)).WillOnce(Return(tmtr));
  tmtr |= (1 << 2);
  EXPECT_CALL(mockAdapter, write(baseAddr + TMTR, tmtr));
  oclaIP->start();
}

TEST_F(OclaIPTest, getDataTest) {
  const uint32_t ocsr = 0xdeadbeef;
  const uint32_t tmtr = 0xAABBCCDD;
  EXPECT_CALL(mockAdapter, read(baseAddr + OCSR)).WillOnce(Return(ocsr));
  EXPECT_CALL(mockAdapter, read(baseAddr + TMTR)).WillOnce(Return(tmtr));

  ocla_data data;
  if ((tmtr >> 3) & 0x1) {
    data.depth = (tmtr >> 4) & 0x7ff;
  } else {
    data.depth = (ocsr >> 11) & 0x3ff;
  }

  data.width = (ocsr >> 1) & 0x3ff;
  data.num_reads = ((data.width - 1) / 32) + 1;
  const uint32_t count = data.depth * data.num_reads;
  std::vector<uint32_t> expectedValues = data.values;
  EXPECT_CALL(mockAdapter, read(baseAddr + TBDR, count, 0))
      .WillOnce(Return(expectedValues));

  auto result = oclaIP->get_data();
  EXPECT_EQ(data.depth, result.depth);
  EXPECT_EQ(data.width, result.width);
  EXPECT_EQ(data.num_reads, result.num_reads);
  EXPECT_EQ(data.values.size(), result.values.size());
}

TEST_F(OclaIPTest, getConfigTest) {
  uint32_t tmtr = 0x12345678;
  uint32_t tcur0 = 0x8002;
  uint32_t tcur1 = 0x7001;
  ocla_config cfg;
  cfg.mode = (ocla_mode)(tmtr & 0x3);
  cfg.fns = (ocla_fns)((tmtr >> 3) & 0x1);
  cfg.ns = ((tmtr >> 4) & 0x7ff);
  cfg.st = ((tmtr >> 2) & 1);

  EXPECT_CALL(mockAdapter, read(baseAddr + TMTR)).WillOnce(Return(tmtr));
  EXPECT_CALL(mockAdapter, read(baseAddr + TCUR0)).WillOnce(Return(tcur0));
  EXPECT_CALL(mockAdapter, read(baseAddr + TCUR1)).WillOnce(Return(tcur1));

  tcur0 = ((tcur0 >> 15) & 3);
  tcur1 = ((tcur1 >> 15) & 3);
  cfg.condition = tcur0 == tcur1 ? (ocla_trigger_condition)tcur0 : DEFAULT;

  ocla_config configData = oclaIP->get_config();
  EXPECT_EQ(cfg.condition, configData.condition);
  EXPECT_EQ(cfg.mode, configData.mode);
  EXPECT_EQ(cfg.fns, configData.fns);
  EXPECT_EQ(cfg.ns, configData.ns);
  EXPECT_EQ(cfg.st, configData.st);
}

TEST_F(OclaIPTest, getChannelConfigTest) {
  uint32_t channel = 1;
  uint32_t tcur = 0x10a1aabb;
  EXPECT_CALL(mockAdapter, read(baseAddr + (channel < 2 ? TCUR0 : TCUR1)))
      .WillOnce(::testing::Return(tcur));

  ocla_trigger_config result = oclaIP->get_channel_config(channel);

  ocla_trigger_config expected;
  expected.probe_num = 16;
  expected.type = LEVEL;
  expected.event = ocla_trigger_event::LOW;

  EXPECT_EQ(result.probe_num, expected.probe_num);
  EXPECT_EQ(result.type, expected.type);
  EXPECT_EQ(result.event, expected.event);
}
