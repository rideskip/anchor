#include "gtest/gtest.h"

extern "C" {
#include "anchor/logging/logging.h"
}

#include <stdio.h>
#include <time.h>

static void logging_write_function(const char* str) {
  fputs(str, stdout);
}

static uint32_t logging_time_ms_function(void) {
  return time(NULL);
}

int main(int argc, char **argv) {
  const logging_init_t init_logging = {
    .write_function = logging_write_function,
    .time_ms_function = logging_time_ms_function,
    .default_level = LOGGING_LEVEL_DEBUG,
  };
  logging_init(&init_logging);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
