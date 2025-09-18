#include <unity.h>
#include <Arduino.h>
#include "string_utils.h"
#include "datetime_utils.h"
#include "errors.h"
#include "battery.h"

using namespace photo_frame;

// String Utils Tests
void test_build_string_two_components() {
    String s1 = "Hello";
    String s2 = " World";
    String result = string_utils::build_string(s1, s2);
    TEST_ASSERT_EQUAL_STRING("Hello World", result.c_str());
}

void test_build_string_three_components() {
    String s1 = "Hello";
    String s2 = " ";
    String s3 = "World";
    String result = string_utils::build_string(s1, s2, s3);
    TEST_ASSERT_EQUAL_STRING("Hello World", result.c_str());
}

void test_build_string_four_components() {
    String s1 = "A";
    String s2 = "B";
    String s3 = "C";
    String s4 = "D";
    String result = string_utils::build_string(s1, s2, s3, s4);
    TEST_ASSERT_EQUAL_STRING("ABCD", result.c_str());
}

void test_build_string_with_empty_components() {
    String s1 = "Hello";
    String s2 = "";
    String s3 = "World";
    String result = string_utils::build_string(s1, s2, s3);
    TEST_ASSERT_EQUAL_STRING("HelloWorld", result.c_str());
}

void test_build_path_three_components() {
    String s1 = "root";
    String s2 = "file.txt";
    String s3 = ".png";
    String result = string_utils::build_path(s1, s2, s3);
    TEST_ASSERT_EQUAL_STRING("root/file.txt.png", result.c_str());
}

void test_build_path_with_trailing_slash() {
    String s1 = "folder/";
    String s2 = "file.txt";
    String result = string_utils::build_path(s1, s2);
    TEST_ASSERT_EQUAL_STRING("folder/file.txt", result.c_str());
}

void test_build_http_request_line() {
    String method = "GET";
    String path = "/api/test";
    String result = string_utils::build_http_request_line(method, path);
    TEST_ASSERT_EQUAL_STRING("GET /api/test HTTP/1.1\r\n", result.c_str());
}

void test_build_http_header() {
    String name = "Content-Type";
    String value = "application/json";
    String result = string_utils::build_http_header(name, value);
    TEST_ASSERT_EQUAL_STRING("Content-Type: application/json\r\n", result.c_str());
}

void test_seconds_to_human_seconds() {
    char buffer[32];
    string_utils::seconds_to_human(buffer, sizeof(buffer), 45);
    TEST_ASSERT_EQUAL_STRING("45s", buffer);
}

void test_seconds_to_human_minutes() {
    char buffer[32];
    string_utils::seconds_to_human(buffer, sizeof(buffer), 125);
    TEST_ASSERT_EQUAL_STRING("2m 5s", buffer);
}

void test_seconds_to_human_hours() {
    char buffer[32];
    string_utils::seconds_to_human(buffer, sizeof(buffer), 3665);
    TEST_ASSERT_EQUAL_STRING("1h 1m 5s", buffer);
}

void test_seconds_to_human_zero() {
    char buffer[32];
    string_utils::seconds_to_human(buffer, sizeof(buffer), 0);
    TEST_ASSERT_EQUAL_STRING("0s", buffer);
}

// Memory check test
void test_heap_health_check() {
    bool result = string_utils::check_heap_health("test context", 1000);
    TEST_ASSERT_TRUE(result); // Should have plenty of heap available
}

// Error Tests
void test_error_default_constructor() {
    photo_frame_error error;
    TEST_ASSERT_EQUAL(ERROR_SEVERITY_INFO, error.severity);
    TEST_ASSERT_EQUAL(ERROR_CATEGORY_GENERAL, error.category);
    TEST_ASSERT_EQUAL(0, error.code);
}

void test_error_message_constructor() {
    const char* message = "Test error";
    photo_frame_error error(message, 123);
    TEST_ASSERT_EQUAL(ERROR_SEVERITY_ERROR, error.severity);
    TEST_ASSERT_EQUAL(ERROR_CATEGORY_GENERAL, error.category);
    TEST_ASSERT_EQUAL_STRING(message, error.message);
    TEST_ASSERT_EQUAL(123, error.code);
}

void test_error_full_constructor() {
    const char* message = "Network timeout";
    photo_frame_error error(message, 456, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK);
    TEST_ASSERT_EQUAL(ERROR_SEVERITY_WARNING, error.severity);
    TEST_ASSERT_EQUAL(ERROR_CATEGORY_NETWORK, error.category);
    TEST_ASSERT_EQUAL_STRING(message, error.message);
    TEST_ASSERT_EQUAL(456, error.code);
}

void test_error_is_critical() {
    photo_frame_error error("Critical message", 789, ERROR_SEVERITY_CRITICAL, ERROR_CATEGORY_BATTERY);
    TEST_ASSERT_TRUE(error.is_critical());
}

void test_error_is_not_critical() {
    photo_frame_error error("Warning message", 101, ERROR_SEVERITY_WARNING, ERROR_CATEGORY_NETWORK);
    TEST_ASSERT_FALSE(error.is_critical());
}

void test_error_equality() {
    photo_frame_error error1("Error 1", 100);
    photo_frame_error error2("Error 2", 100);
    photo_frame_error error3("Error 3", 200);
    TEST_ASSERT_TRUE(error1 == error2);  // Same code
    TEST_ASSERT_FALSE(error1 == error3); // Different code
}

// Battery Tests - Only test inline/constexpr functions
void test_battery_step_constructor() {
    battery_step_t step(50, 3700);
    TEST_ASSERT_EQUAL(50, step.percent);
    TEST_ASSERT_EQUAL(3700, step.voltage);
}

void test_battery_info_constructors() {
    battery_info info1;
    TEST_ASSERT_EQUAL(0, info1.percent);

    battery_info info2 = battery_info::full();
    TEST_ASSERT_EQUAL(100, info2.percent);

    battery_info info3 = battery_info::empty();
    TEST_ASSERT_EQUAL(0, info3.percent);
}

void setup() {
    delay(2000);
    UNITY_BEGIN();

    // String Utils Tests
    RUN_TEST(test_build_string_two_components);
    RUN_TEST(test_build_string_three_components);
    RUN_TEST(test_build_string_four_components);
    RUN_TEST(test_build_string_with_empty_components);
    RUN_TEST(test_build_path_three_components);
    RUN_TEST(test_build_path_with_trailing_slash);
    RUN_TEST(test_build_http_request_line);
    RUN_TEST(test_build_http_header);
    RUN_TEST(test_seconds_to_human_seconds);
    RUN_TEST(test_seconds_to_human_minutes);
    RUN_TEST(test_seconds_to_human_hours);
    RUN_TEST(test_seconds_to_human_zero);

    // Memory check test
    RUN_TEST(test_heap_health_check);

    // Error Tests
    RUN_TEST(test_error_default_constructor);
    RUN_TEST(test_error_message_constructor);
    RUN_TEST(test_error_full_constructor);
    RUN_TEST(test_error_is_critical);
    RUN_TEST(test_error_is_not_critical);
    RUN_TEST(test_error_equality);

    // Battery Tests
    RUN_TEST(test_battery_step_constructor);
    RUN_TEST(test_battery_info_constructors);

    UNITY_END();
}

void loop() {
    // Empty - tests run once in setup()
}