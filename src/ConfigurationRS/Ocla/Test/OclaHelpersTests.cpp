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
    ::testing::Values(std::make_pair(CONTINUOUS, "disable"),
                      std::make_pair(PRE, "pre-trigger"),
                      std::make_pair(POST, "post-trigger"),
                      std::make_pair(CENTER, "center-trigger"),
                      std::make_pair(CONTINUOUS, "disable")));

class ConvertTriggerConditionToStringParamTest
    : public ::testing::TestWithParam<
          std::pair<ocla_trigger_bool_comp, std::string>> {};

TEST_P(ConvertTriggerConditionToStringParamTest, ConvertTriggerCondition) {
  const auto& param = GetParam();
  const ocla_trigger_bool_comp condition = param.first;
  const std::string& expected_result = param.second;

  std::string result = convert_trigger_bool_comp_to_string(condition);
  EXPECT_EQ(result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    Default, ConvertTriggerConditionToStringParamTest,
    ::testing::Values(std::make_pair(ocla_trigger_bool_comp::OR, "OR"),
                      std::make_pair(ocla_trigger_bool_comp::AND, "AND"),
                      std::make_pair(ocla_trigger_bool_comp::DEFAULT, "OR"),
                      std::make_pair(ocla_trigger_bool_comp(99), "(unknown)"),
                      std::make_pair(ocla_trigger_bool_comp::XOR, "XOR")));

TEST(convert_trigger_bool_comp_to_string, DefaultModeString) {
  // Test converting with a default value
  auto result =
      convert_trigger_bool_comp_to_string((ocla_trigger_bool_comp)10, "AND");
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
  ocla_trigger_bool_comp expected_result;
};

class ConvertTriggerConditionTest : public ::testing::TestWithParam<TestParam> {
};

TEST_P(ConvertTriggerConditionTest, ConvertTriggerCondition) {
  const auto& param = GetParam();
  const std::string condition = param.condition_string;
  const ocla_trigger_bool_comp expected_result = param.expected_result;

  ocla_trigger_bool_comp result = convert_trigger_bool_comp(condition);
  EXPECT_EQ(result, expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    Default, ConvertTriggerConditionTest,
    ::testing::Values(TestParam{"OR", ocla_trigger_bool_comp::DEFAULT},
                      TestParam{"AND", ocla_trigger_bool_comp::AND},
                      TestParam{"XOR", ocla_trigger_bool_comp::XOR}));

TEST(ConvertTriggerTypeTest, ValidTriggerType) {
  std::vector<std::pair<ocla_trigger_type, std::string>> testParam{
      {TRIGGER_NONE, "disable"},
      {EDGE, "edge"},
      {LEVEL, "level"},
      {VALUE_COMPARE, "value_compare"}};
  // ocla_trigger_type expected = ocla_trigger_type::TRIGGER_NONE;
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
  // ocla_trigger_type expected = ocla_trigger_type::TRIGGER_NONE;
  for (const auto& [expected, param] : testParam) {
    std::string type_string = param;
    ocla_trigger_event result = convert_trigger_event(type_string);
    EXPECT_EQ(result, expected);
  }
}

TEST(ConvertTriggerEventTest, InvalidTriggerType) {
  std::string type_string = "dummyTestString";
  ocla_trigger_event expected = NONE;
  ocla_trigger_event result = convert_trigger_event(type_string);
  EXPECT_EQ(result, expected);
}

TEST(ConvertTriggerEventTest, EmptyTriggerType) {
  std::string type_string = "";
  ocla_trigger_event expected = NONE;
  ocla_trigger_event result = convert_trigger_event(type_string);
  EXPECT_EQ(result, expected);
}

TEST(ConvertTriggerEventTest, defaultTriggerType) {
  std::string type_string = "my default";
  ocla_trigger_event expected = NONE;
  ocla_trigger_event result = convert_trigger_event(type_string, NONE);
  EXPECT_EQ(result, expected);
}

TEST(GenerateSignalDescriptorTest, ValidWidth) {
  uint32_t width = 8;
  std::vector<signal_info> expected = {{"s0", 1}, {"s1", 1}, {"s2", 1},
                                       {"s3", 1}, {"s4", 1}, {"s5", 1},
                                       {"s6", 1}, {"s7", 1}};
  auto result = generate_signal_descriptor(width);
  for (size_t i = 0; i < width; i++) {
    EXPECT_EQ(result[i].name, expected[i].name);
    EXPECT_EQ(result[i].bitwidth, expected[i].bitwidth);
  }
}

TEST(GenerateSignalDescriptorTest, ZeroWidth) {
  uint32_t width = 0;
  std::vector<signal_info> expected = {};
  std::vector<signal_info> result = generate_signal_descriptor(width);
  EXPECT_EQ(result.size(), expected.size());
}
