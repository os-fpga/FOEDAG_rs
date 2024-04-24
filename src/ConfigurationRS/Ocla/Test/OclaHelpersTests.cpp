#include <gtest/gtest.h>

#include <vector>

#include "OclaHelpers.h"

class ConvertOclaModeToStringParamTest
    : public ::testing::TestWithParam<
          std::pair<ocla_trigger_mode, std::string>> {};

TEST_P(ConvertOclaModeToStringParamTest, ConvertOclaMode) {
  const auto& param = GetParam();
  const ocla_trigger_mode mode = param.first;
  const std::string& expected_result = param.second;

  std::string result = convert_ocla_trigger_mode_to_string(mode);
  EXPECT_EQ(result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    Default, ConvertOclaModeToStringParamTest,
    ::testing::Values(std::make_pair(CONTINUOUS, "DISABLE"),
                      std::make_pair(PRE, "PRE-TRIGGER"),
                      std::make_pair(POST, "POST-TRIGGER"),
                      std::make_pair(CENTER, "CENTER-TRIGGER"),
                      std::make_pair(CONTINUOUS, "DISABLE")));

class ConvertTriggerConditionToStringParamTest
    : public ::testing::TestWithParam<
          std::pair<ocla_trigger_condition, std::string>> {};

TEST_P(ConvertTriggerConditionToStringParamTest, ConvertTriggerCondition) {
  const auto& param = GetParam();
  const ocla_trigger_condition condition = param.first;
  const std::string& expected_result = param.second;

  std::string result = convert_trigger_condition_to_string(condition);
  EXPECT_EQ(result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    Default, ConvertTriggerConditionToStringParamTest,
    ::testing::Values(std::make_pair(ocla_trigger_condition::OR, "OR"),
                      std::make_pair(ocla_trigger_condition::AND, "AND"),
                      std::make_pair(ocla_trigger_condition::DEFAULT, "OR"),
                      std::make_pair(ocla_trigger_condition(99), "(unknown)"),
                      std::make_pair(ocla_trigger_condition::XOR, "XOR")));

TEST(convert_trigger_condition_to_string, DefaultModeString) {
  // Test converting with a default value
  auto result =
      convert_trigger_condition_to_string((ocla_trigger_condition)10, "AND");
  EXPECT_EQ(result, "AND");
}

class ConvertTriggerTypeToStringTest
    : public ::testing::TestWithParam<
          std::pair<ocla_trigger_type, std::string>> {};

TEST_P(ConvertTriggerTypeToStringTest, ConvertTriggerType) {
  const auto& param = GetParam();
  const ocla_trigger_type type = param.first;
  const std::string& expected_result = param.second;

  std::string result = convert_trigger_type_to_string(type);
  EXPECT_EQ(result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    Default, ConvertTriggerTypeToStringTest,
    ::testing::Values(
        std::make_pair(ocla_trigger_type::TRIGGER_NONE, "disable"),
        std::make_pair(ocla_trigger_type::EDGE, "edge"),
        std::make_pair(ocla_trigger_type::LEVEL, "level"),
        std::make_pair(ocla_trigger_type::VALUE_COMPARE, "value_compare"),
        std::make_pair(ocla_trigger_type(99), "(unknown)")));

class ConvertTriggerEventToStringTest
    : public ::testing::TestWithParam<
          std::pair<ocla_trigger_event, std::string>> {};

TEST_P(ConvertTriggerEventToStringTest, ConvertTriggerType) {
  const auto& param = GetParam();
  const ocla_trigger_event type = param.first;
  const std::string& expected_result = param.second;

  std::string result = convert_trigger_event_to_string(type);
  EXPECT_EQ(result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    Default, ConvertTriggerEventToStringTest,
    ::testing::Values(
        std::make_pair(ocla_trigger_event::EDGE_NONE, "edge_none"),
        std::make_pair(ocla_trigger_event::RISING, "rising"),
        std::make_pair(ocla_trigger_event::FALLING, "falling"),
        std::make_pair(ocla_trigger_event::EITHER, "either"),
        std::make_pair(ocla_trigger_event::LOW, "low"),
        std::make_pair(ocla_trigger_event::HIGH, "high"),
        std::make_pair(ocla_trigger_event::VALUE_NONE, "value_none"),
        std::make_pair(ocla_trigger_event::EQUAL, "equal"),
        std::make_pair(ocla_trigger_event::LESSER, "lesser"),
        std::make_pair(ocla_trigger_event::GREATER, "greater"),
        std::make_pair(ocla_trigger_event(99), "(unknown)")));

class ConvertOclaModeParamTest
    : public ::testing::TestWithParam<
          std::pair<std::string, ocla_trigger_mode>> {};

TEST_P(ConvertOclaModeParamTest, ConvertModeString) {
  const auto& param = GetParam();
  const std::string& mode_string = param.first;
  ocla_trigger_mode expected_result = param.second;

  ocla_trigger_mode result = convert_ocla_trigger_mode(mode_string);
  EXPECT_EQ(result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    Default, ConvertOclaModeParamTest,
    ::testing::Values(std::make_pair("pre-trigger", PRE),
                      std::make_pair("post-trigger", POST),
                      std::make_pair("disable", CONTINUOUS),
                      std::make_pair("post-trigger", POST),
                      std::make_pair("invalid-mode", CONTINUOUS)));

TEST(ConvertOclaModeTest, DefaultModeString) {
  // Test converting with a default value
  ocla_trigger_mode result = convert_ocla_trigger_mode("invalid-mode", POST);
  EXPECT_EQ(result, POST);
}

struct TestParam {
  std::string condition_string;
  ocla_trigger_condition expected_result;
};

class ConvertTriggerConditionTest : public ::testing::TestWithParam<TestParam> {
};

TEST_P(ConvertTriggerConditionTest, ConvertTriggerCondition) {
  const auto& param = GetParam();
  const std::string condition = param.condition_string;
  const ocla_trigger_condition expected_result = param.expected_result;

  ocla_trigger_condition result = convert_trigger_condition(condition);
  EXPECT_EQ(result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    Default, ConvertTriggerConditionTest,
    ::testing::Values(TestParam{"OR", ocla_trigger_condition::DEFAULT},
                      TestParam{"AND", ocla_trigger_condition::AND},
                      TestParam{"XOR", ocla_trigger_condition::XOR}));

TEST(ConvertTriggerTypeTest, ValidTriggerType) {
  std::vector<std::pair<ocla_trigger_type, std::string>> testParam{
      {TRIGGER_NONE, "disable"},
      {EDGE, "edge"},
      {LEVEL, "level"},
      {VALUE_COMPARE, "value_compare"}};
  for (const auto& [expected, param] : testParam) {
    std::string type_string = param;
    ocla_trigger_type result = convert_trigger_type(type_string);
    EXPECT_EQ(result, expected);
  }
}

TEST(ConvertTriggerTypeTest, InvalidTriggerType) {
  std::string type_string = "dummyTestString";
  ocla_trigger_type expected = TRIGGER_NONE;
  ocla_trigger_type result = convert_trigger_type(type_string);
  EXPECT_EQ(result, expected);
}

TEST(ConvertTriggerTypeTest, EmptyTriggerType) {
  std::string type_string = "";
  ocla_trigger_type expected = TRIGGER_NONE;
  ocla_trigger_type result = convert_trigger_type(type_string);
  EXPECT_EQ(result, expected);
}

TEST(ConvertTriggerTypeTest, defaultTriggerType) {
  std::string type_string = "my default";
  ocla_trigger_type expected = VALUE_COMPARE;
  ocla_trigger_type result = convert_trigger_type(type_string, VALUE_COMPARE);
  EXPECT_EQ(result, expected);
}

TEST(ConvertTriggerEventTest, ValidTriggerType) {
  std::vector<std::pair<ocla_trigger_event, std::string>> testParam{
      {EDGE_NONE, "edge_none"},   {RISING, "rising"}, {FALLING, "falling"},
      {EITHER, "either"},         {LOW, "low"},       {HIGH, "high"},
      {VALUE_NONE, "value_none"}, {EQUAL, "equal"},   {LESSER, "lesser"},
      {GREATER, "greater"}};
  for (const auto& [expected, param] : testParam) {
    std::string type_string = param;
    ocla_trigger_event result = convert_trigger_event(type_string);
    EXPECT_EQ(result, expected);
  }
}

TEST(ConvertTriggerEventTest, InvalidTriggerType) {
  std::string type_string = "dummyTestString";
  ocla_trigger_event expected = NO_EVENT;
  ocla_trigger_event result = convert_trigger_event(type_string);
  EXPECT_EQ(result, expected);
}

TEST(ConvertTriggerEventTest, EmptyTriggerType) {
  std::string type_string = "";
  ocla_trigger_event expected = NO_EVENT;
  ocla_trigger_event result = convert_trigger_event(type_string);
  EXPECT_EQ(result, expected);
}

TEST(ConvertTriggerEventTest, defaultTriggerType) {
  std::string type_string = "my default";
  ocla_trigger_event expected = NO_EVENT;
  ocla_trigger_event result = convert_trigger_event(type_string, NO_EVENT);
  EXPECT_EQ(result, expected);
}

TEST(CFG_toupperTest, CFG_toupper_test) {
  std::string test_string = "helloworld!!!";
  auto result = CFG_toupper(test_string);
  EXPECT_EQ(result, "HELLOWORLD!!!");
}

TEST(ReadWriteBitsVectorUint32Test, CFG_read_bit_vec32_test) {
  std::vector<uint32_t> test_vector{0xa5a55a5a, 0xdeadbeef, 0xffff0000, 0xabcd1234, 0x76543210};
  std::vector<uint32_t> vector(5, 0);

  for (int i = 0; i < (32 * int(test_vector.size())); i++) {
    uint32_t bit = CFG_read_bit_vec32(test_vector.data(), i);
    if (bit) {
      vector[i / 32] |= (1u << (i % 32));
    }
  }

  EXPECT_EQ(vector, test_vector);
}

TEST(ReadWriteBitsVectorUint32Test, CFG_write_bit_vec32) {
  std::vector<uint32_t> test_vector{0xa5a55a5a, 0xdeadbeef, 0xffff0000, 0xabcd1234, 0x76543210};
  std::vector<uint32_t> vector(5, -1);

  for (int i = 0; i < (32 * int(test_vector.size())); i++) {
    CFG_write_bit_vec32(vector.data(), i, test_vector[i / 32] & (1u << (i % 32)));
  }

  EXPECT_EQ(vector, test_vector);
}

TEST(ReadWriteBitsVectorUint32Test, CFG_copy_bits_vec32) {
  std::vector<uint32_t> test_vector{
    0x3A8FBCD2, 0x7E2D9F54, 0x9C0A6B71, 0x5F184E3D, 0xA39215E8,
    0x1B6C8D9A, 0xE71FAB2F, 0x4D0E62F6, 0x82F573C9};
  std::vector<uint32_t> expected_vector{
    0x3A8FBCFF, 0x7E2D9F54, 0x9C0A6B71, 0x5F184E3D, 0xA39215E8,
    0x1B6C8D9A, 0xE71FAB2F, 0x4D0E62F6, 0xFFF573C9};
  std::vector<uint32_t> vector(9, 0xffffffff);

  for (int i = 7; i < (32 * int(test_vector.size())) - 8; i += 7) {
    CFG_copy_bits_vec32(test_vector.data(), i, vector.data(), i, 7);
  }

  EXPECT_EQ(vector, expected_vector);
}

class ReverseByteOrderU32Test : public ::testing::TestWithParam<std::tuple<uint32_t, uint32_t>> {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

INSTANTIATE_TEST_SUITE_P(Default, ReverseByteOrderU32Test,
                        ::testing::Values(std::make_tuple(0x01020304, 0x04030201),
                                          std::make_tuple(0x12345678, 0x78563412),
                                          std::make_tuple(0x1111FFFF, 0xFFFF1111)));

TEST_P(ReverseByteOrderU32Test, CFG_reverse_byte_order_u32_test) {
  uint32_t input = std::get<0>(GetParam());
  uint32_t expected_output = std::get<1>(GetParam());
  auto output = CFG_reverse_byte_order_u32(input);
  EXPECT_EQ(output, expected_output);
}

class SetBitfieldU32Test : public ::testing::TestWithParam<std::tuple<uint32_t, uint8_t, uint8_t, uint32_t, uint32_t>> {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

INSTANTIATE_TEST_SUITE_P(Default, SetBitfieldU32Test,
                        ::testing::Values(
                                          std::make_tuple(0x00000000, 7, 5, 31, 0x00000f80),
                                          std::make_tuple(0xffffffff, 21, 7, 0, 0xf01fffff),
                                          std::make_tuple(0x00000000, 0, 32, 0xdeadbeef, 0xdeadbeef),
                                          std::make_tuple(0x00000001, 1, 31, 0xdeadbeef, 0xbd5b7ddf)
                                          ));

TEST_P(SetBitfieldU32Test, CFG_set_bitfield_u32_test) {
  uint32_t value = std::get<0>(GetParam());
  uint32_t pos = std::get<1>(GetParam());
  uint32_t width = std::get<2>(GetParam());
  uint32_t data = std::get<3>(GetParam());
  uint32_t expected_value = std::get<4>(GetParam());
  CFG_set_bitfield_u32(value, pos, width, data);
  EXPECT_EQ(value, expected_value);
}

class ParseSignalTest : public ::testing::TestWithParam<std::tuple<std::string, std::string, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>> {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

INSTANTIATE_TEST_SUITE_P(Default, ParseSignalTest,
                        ::testing::Values(std::make_tuple("counter[31:1]", "counter", 1, 31, 0, 0, OCLA_SIGNAL_PATTERN_1),
                                          std::make_tuple("8'10011001", "8'10011001", 0, 0, 8, 153, OCLA_SIGNAL_PATTERN_2),
                                          std::make_tuple("addr[5]", "addr", 5, 5, 0, 0, OCLA_SIGNAL_PATTERN_3),
                                          std::make_tuple("3", "3", 0, 0, 0, 3, OCLA_SIGNAL_PATTERN_4),
                                          std::make_tuple("abc", "abc", 0, 0, 0, 0, OCLA_SIGNAL_PATTERN_5),
                                          std::make_tuple("#123ABC", "", 0, 0, 0, 0, 0)));

TEST_P(ParseSignalTest, CFG_parse_signal_test) {
  std::string signal_str = std::get<0>(GetParam());
  std::string expected_name = std::get<1>(GetParam());
  uint32_t expected_start = std::get<2>(GetParam());
  uint32_t expected_end = std::get<3>(GetParam());
  uint32_t expected_width = std::get<4>(GetParam());
  uint32_t expected_value = std::get<5>(GetParam());
  uint32_t expected_result = std::get<6>(GetParam());
  uint32_t start = 0;
  uint32_t end = 0;
  uint32_t width = 0;
  uint32_t value = 0;
  std::string name = "";
  auto result = CFG_parse_signal(signal_str, name, start, end, width, value);
  EXPECT_EQ(result, expected_result);
  EXPECT_EQ(name, expected_name);
  EXPECT_EQ(start, expected_start);
  EXPECT_EQ(end, expected_end);
  EXPECT_EQ(width, expected_width);
  EXPECT_EQ(value, expected_value);
}
