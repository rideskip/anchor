SONAR_BASE_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

SONAR_C_SOURCES := \
	$(SONAR_BASE_DIR)/src/client.c \
	$(SONAR_BASE_DIR)/src/server.c \
	$(SONAR_BASE_DIR)/src/common/buffer_chain.c \
	$(SONAR_BASE_DIR)/src/common/crc16.c \
	$(SONAR_BASE_DIR)/src/link_layer/link_layer.c \
	$(SONAR_BASE_DIR)/src/link_layer/receive.c \
	$(SONAR_BASE_DIR)/src/link_layer/transmit.c \
	$(SONAR_BASE_DIR)/src/application_layer/application_layer.c \
	$(SONAR_BASE_DIR)/src/attribute/attribute_server.c \
	$(SONAR_BASE_DIR)/src/attribute/attribute_client.c
