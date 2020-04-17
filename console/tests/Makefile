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

CFLAGS := $(CXX_INCLUDES) -g3 -Werror $(addprefix -D,$(C_DEFS))
CPP_FLAGS :=
LDFLAGS := -lgtest -lpthread

ifeq ($(OS),Windows_NT)
$(error "Windows is not currently supported")
else
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
# Likely using clang
CFLAGS += -Wno-extern-c-compat
CPP_FLAGS += -std=c++14
else
CPP_FLAGS += -D_Static_assert=static_assert
endif
endif

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	@echo "Compiling $(notdir $@)"
	@$(CC) -c $(CFLAGS) -MD -MF"$(@:%.o=%.d)" $< -o $@

$(BUILD_DIR)/%.o: %.cpp Makefile | $(BUILD_DIR)
	@echo "Compiling $(notdir $@)"
	@$(CXX) -c $(CFLAGS) $(CPP_FLAGS) -MD -MF"$(@:%.o=%.d)" $< -o $@

$(BUILD_DIR)/$(TARGET): $(OBJECTS) Makefile | $(BUILD_DIR)
	@echo "Linking $(notdir $@)"
	@$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

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
