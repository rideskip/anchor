TARGET := test
BUILD_DIR := build/

C_SOURCES := \
	../src/console.c

C_DEFS :=

CXX_SOURCES := \
	main.cpp \
	test_console.cpp

CXX_INCLUDES := \
	-I../include

CC := gcc
CXX := g++

OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(CXX_SOURCES:.cpp=.o))) $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.cpp $(sort $(dir $(CXX_SOURCES)))

CFLAGS := $(CXX_INCLUDES) -g3 -Wno-extern-c-compat -Werror $(addprefix -D,$(C_DEFS))
LDFLAGS := -lgtest


$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	@echo "Compiling $(notdir $@)"
	@$(CC) -c $(CFLAGS) -MD -MF"$(@:%.o=%.d)" $< -o $@

$(BUILD_DIR)/%.o: %.cpp Makefile | $(BUILD_DIR)
	@echo "Compiling $(notdir $@)"
	@$(CXX) -c $(CFLAGS) -std=c++14 -MD -MF"$(@:%.o=%.d)" $< -o $@

$(BUILD_DIR)/$(TARGET): $(OBJECTS) Makefile | $(BUILD_DIR)
	@echo "Linking $(notdir $@)"
	@$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR):
	@mkdir -p $@

build: $(BUILD_DIR)/$(TARGET)

test: $(BUILD_DIR)/$(TARGET)
	@$<

clean:
	@echo "Deleting build folder"
	@rm -fR $(BUILD_DIR)


-include $(wildcard $(BUILD_DIR)/*.d)
.PHONY: test clean build
.DEFAULT_GOAL := test
