#include "gtest/gtest.h"

extern "C" {

#include "anchor/console/console.h"

};

#include <cstring>

extern std::vector<char> g_console_write_buffer;

#define EXPECT_WRITE_BUFFER(str) do { \
    g_console_write_buffer.push_back('\0'); \
    const char* write_buffer_str = g_console_write_buffer.data(); \
    EXPECT_STREQ(write_buffer_str, str); \
    g_console_write_buffer.clear(); \
  } while (0)

static void process_line(const char* str) {
  console_process((const uint8_t*)str, (uint32_t)strlen(str));
}

TEST(ConsoleTest, TestInvalidCommand) {
  process_line("\n");
  EXPECT_WRITE_BUFFER("\n> ");

  process_line("invalid\n");
  EXPECT_WRITE_BUFFER(
    "invalid\n"
    "ERROR: Command not found (invalid)\n> ");

  process_line("2 add\n");
  EXPECT_WRITE_BUFFER(
    "2 add\n"
    "ERROR: Command not found (2)\n> ");

  process_line("\a\n");
  EXPECT_WRITE_BUFFER("\n> ");
}

TEST(ConsoleTest, TestValidHiCommand) {
  process_line("say_hi\n");
  EXPECT_WRITE_BUFFER(
    "say_hi\n"
    "hi\n> ");
}

TEST(ConsoleTest, TestInvalidHiCommand) {
  process_line("say_hi there\n");
  EXPECT_WRITE_BUFFER(
    "say_hi there\n"
    "ERROR: Too many arguments\n> ");

  process_line("say_hi \n");
  EXPECT_WRITE_BUFFER(
    "say_hi \n"
    "ERROR: Extra whitespace between arguments\n> ");
}

TEST(ConsoleTest, TestValidAddCommand) {
  process_line("add 1 2\n");
  EXPECT_WRITE_BUFFER(
    "add 1 2\n"
    "1 + 2 = 3\n> ");

  process_line("add 1 2 3\n");
  EXPECT_WRITE_BUFFER(
    "add 1 2 3\n"
    "1 + 2 + 3 = 6\n> ");

  process_line("add -5 10\n");
  EXPECT_WRITE_BUFFER(
    "add -5 10\n"
    "-5 + 10 = 5\n> ");

  process_line("add 0xff 0x100\n");
  EXPECT_WRITE_BUFFER(
    "add 0xff 0x100\n"
    "255 + 256 = 511\n> ");
}

TEST(ConsoleTest, TestInvalidAddCommand) {
  process_line("add\n");
  EXPECT_WRITE_BUFFER(
    "add\n"
    "ERROR: Too few arguments\n> ");

  process_line("add 2\n");
  EXPECT_WRITE_BUFFER(
    "add 2\n"
    "ERROR: Too few arguments\n> ");

  process_line("add 2 3 4 5\n");
  EXPECT_WRITE_BUFFER(
    "add 2 3 4 5\n"
    "ERROR: Too many arguments\n> ");

  process_line("add 2  3\n");
  EXPECT_WRITE_BUFFER(
    "add 2  3\n"
    "ERROR: Extra whitespace between arguments\n> ");

  process_line("add 2 x\n");
  EXPECT_WRITE_BUFFER(
    "add 2 x\n"
    "ERROR: Invalid value for 'num2' (x)\n> ");

  process_line("add 2 0xZZ\n");
  EXPECT_WRITE_BUFFER(
    "add 2 0xZZ\n"
    "ERROR: Invalid value for 'num2' (0xZZ)\n> ");

  process_line("add 0xZZ 0xYY\n");
  EXPECT_WRITE_BUFFER(
    "add 0xZZ 0xYY\n"
    "ERROR: Invalid value for 'num1' (0xZZ)\n> ");
}

TEST(ConsoleTest, TestValidStroffCommand) {
  process_line("stroff myteststr 0\n");
  EXPECT_WRITE_BUFFER(
    "stroff myteststr 0\n"
    "myteststr\n> ");

  process_line("stroff myteststr 2\n");
  EXPECT_WRITE_BUFFER(
    "stroff myteststr 2\n"
    "teststr\n> ");

  process_line("stroff myteststr 9\n");
  EXPECT_WRITE_BUFFER(
    "stroff myteststr 9\n"
    "\n> ");

  process_line("stroff 22222 1\n");
  EXPECT_WRITE_BUFFER(
    "stroff 22222 1\n"
    "2222\n> ");
}

TEST(ConsoleTest, TestInvalidStroffCommand) {
  process_line("stroff\n");
  EXPECT_WRITE_BUFFER(
    "stroff\n"
    "ERROR: Too few arguments\n> ");

  process_line("stroff 2\n");
  EXPECT_WRITE_BUFFER(
    "stroff 2\n"
    "ERROR: Too few arguments\n> ");

  process_line("stroff 2 2 1\n");
  EXPECT_WRITE_BUFFER(
    "stroff 2 2 1\n"
    "ERROR: Too many arguments\n> ");

  process_line("stroff 2 test\n");
  EXPECT_WRITE_BUFFER(
    "stroff 2 test\n"
    "ERROR: Invalid value for 'offset' (test)\n> ");
}

TEST(ConsoleTest, TestHelpCommand) {
  process_line("help\n");
  EXPECT_WRITE_BUFFER("help\n"
    "Available commands:\n"
    "  help    - List all commands, or give details about a specific command\n"
    "  say_hi  - Says hi\n"
    "  say_bye - Says bye\n"
    "  minimal\n"
    "  add     - Add two numbers\n"
    "  stroff  - Prints a string starting from an offset\n"
    "> ");

  process_line("help add\n");
  EXPECT_WRITE_BUFFER(
    "help add\n"
    "Add two numbers\n"
    "Usage: add num1 num2 [num3]\n"
    "  num1 - First number\n"
    "  num2 - Second number\n"
    "  num3 - Third (optional) number\n"
    "> ");

  process_line("help minimal\n");
  EXPECT_WRITE_BUFFER(
    "help minimal\n"
    "Usage: minimal\n"
    "> ");
}

TEST(ConsoleTest, TestTabComplete) {
  process_line("sa\t");
  EXPECT_WRITE_BUFFER("say_");

  process_line("\t");
  EXPECT_WRITE_BUFFER("\nsay_hi say_bye\n> say_");

  process_line("b\t");
  EXPECT_WRITE_BUFFER("bye");

  process_line("\t");
  EXPECT_WRITE_BUFFER("");
}
