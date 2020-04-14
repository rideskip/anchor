TARGET := unit_test
BUILD_DIR := build/

include ../sonar.mk

C_SOURCES := \
	$(SONAR_C_SOURCES) \
	../../logging/src/logging.c

CXX_SOURCES := \
	main.cpp \
	test_buffer_chain.cpp \
	test_crc16.cpp \
	test_link_layer_receive.cpp \
	test_link_layer_transmit.cpp \
	test_link_layer.cpp \
	test_application_layer.cpp \
	test_attribute_server.cpp \
	test_attribute_client.cpp \
	test_client.cpp \
	test_server.cpp

CXX_INCLUDES := \
	-I.. \
	-I../include \
	-I../../logging/include

OPT :=

CC := gcc
CXX := g++
AR := ar

OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(CXX_SOURCES:.cpp=.o))) $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.cpp $(sort $(dir $(CXX_SOURCES)))

CFLAGS := $(CXX_INCLUDES) -g3 -Wno-extern-c-compat -Werror
LDFLAGS := -lgtest

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	@echo "Compiling $(notdir $@)"
	@$(CC) -c -DFILENAME=\"$(notdir $<)\" $(CFLAGS) -MD -MF"$(@:%.o=%.d)" $< -o $@

$(BUILD_DIR)/%.o: %.cpp Makefile | $(BUILD_DIR)
	@echo "Compiling $(notdir $@)"
	@$(CXX) -c -DFILENAME=\"$(notdir $<)\" $(CFLAGS) -std=c++14 -MD -MF"$(@:%.o=%.d)" $< -o $@

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
