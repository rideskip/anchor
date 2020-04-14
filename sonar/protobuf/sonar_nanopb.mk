SONAR_NANOPB_BUILD_DIR ?= build
PROTOC ?= protoc

ifeq (SONAR_PROTO_SOURCES,)
$(error "Must set SONAR_PROTO_SOURCES before including sonar_nanopb.mk")
endif
ifeq (NANOPB_ROOT_DIR,)
$(error "Must set NANOPB_ROOT_DIR before including sonar_nanopb.mk")
endif

SONAR_PROTO_NANOPB_MAKEFILE_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
SONAR_NANOPB_PROTO_FLAGS := \
	--plugin=protoc-gen-nanopb=$(NANOPB_ROOT_DIR)/generator/protoc-gen-nanopb \
	--plugin=protoc-gen-sonar=$(abspath $(SONAR_PROTO_NANOPB_MAKEFILE_DIR)/sonar_proto_gen.py) \
	$(addprefix -I,$(abspath $(dir $(SONAR_PROTO_SOURCES))))

vpath %.proto $(sort $(dir $(SONAR_PROTO_SOURCES)))
vpath %.pb.c $(SONAR_NANOPB_BUILD_DIR)

# Rule for building the generated file which nanopb depends on
NANOPB_DEP := $(NANOPB_ROOT_DIR)/generator/proto/nanopb_pb2.py
$(NANOPB_DEP): $(NANOPB_ROOT_DIR)/generator/proto/nanopb.proto $(NANOPB_ROOT_DIR)/generator/proto/plugin.proto
	@$(MAKE) -C $(NANOPB_ROOT_DIR)/generator/proto

# Rule for building the .pb.c file from .proto files
$(SONAR_NANOPB_BUILD_DIR)/%.pb.c: %.proto $(SONAR_NANOPB_PROTO_SOURCES) $(NANOPB_DEP) Makefile | $(SONAR_NANOPB_BUILD_DIR)
	@echo "Compiling $(notdir $@)"
	@NANOPB_ROOT_DIR=$(NANOPB_ROOT_DIR) $(PROTOC) --nanopb_out=$(dir $@) --sonar_out=c:$(dir $@) $(SONAR_NANOPB_PROTO_FLAGS) $(abspath $<)

# Create a no-op rule for the generated nanopb headers since they are generated along with the C files
$(SONAR_NANOPB_BUILD_DIR)/%.pb.h: $(SONAR_NANOPB_BUILD_DIR)/%.pb.c
