PROJECT_NAME := clox
VERSION := 0.0.0

SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
LIB_DIR := lib
INC_DIR := src/include
TEST_DIR := tests
COVERAGE_DIR := coverage-report

CC := gcc
LD := $(CC)
AR := ar
STRIP := strip
SIZE := size

CFLAGS := -Wall -Wextra -Wpedantic -std=c11
CFLAGS += -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes
CFLAGS += -Wconversion -Wsign-conversion -Wdouble-promotion
CFLAGS += -I$(INC_DIR) -I$(SRC_DIR)

LDFLAGS := -L$(LIB_DIR)
LDLIBS := -lm

COVERAGE_FLAGS := --coverage

OPTIMIZATION := -O2
DEBUG_FLAGS := -g -DDEBUG
RELEASE_FLAGS := -DNDEBUG -UDEBUG

BUILD_TYPE ?= debug

ifeq ($(BUILD_TYPE),debug)
    CFLAGS += $(DEBUG_FLAGS) -O0
    BUILD_DIR := $(OBJ_DIR)/debug
else ifeq ($(BUILD_TYPE),release)
    CFLAGS += $(RELEASE_FLAGS) $(OPTIMIZATION)
    BUILD_DIR := $(OBJ_DIR)/release
else ifeq ($(BUILD_TYPE),coverage)
    CFLAGS += $(DEBUG_FLAGS) -O0 $(COVERAGE_FLAGS)
    LDFLAGS += $(COVERAGE_FLAGS)
    BUILD_DIR := $(OBJ_DIR)/coverage
endif

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

TARGET := $(BIN_DIR)/$(PROJECT_NAME)

STATIC_LIB := $(LIB_DIR)/lib$(PROJECT_NAME).a

SRC_FILES = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/**/*.c)
HDR_FILES = $(wildcard $(INC_DIR)/*.h) $(wildcard $(INC_DIR)/**/*.h)

.DEFAULT_GOAL := all

.PHONY: all
all: $(TARGET) size

.PHONY: static
static: $(STATIC_LIB)

$(TARGET): $(OBJS) | $(BIN_DIR)
	@echo "Linking $@"
	$(LD) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)
	@echo "Build completed: $@"

$(STATIC_LIB): $(OBJS) | $(LIB_DIR)
	@echo "Creating static library $@"
	$(AR) rcs $@ $(OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(@D)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

$(BIN_DIR) $(LIB_DIR) $(BUILD_DIR):
	@mkdir -p $@

.PHONY: run
run: $(TARGET)
	@echo "Running $(TARGET)"
	./$(TARGET)

.PHONY: size
size: $(TARGET)
	@echo "Size of executable:"
	$(SIZE) $(TARGET)

.PHONY: clean
clean:
	@echo "Cleaning build artifacts"
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(LIB_DIR) $(COVERAGE_DIR)
	
.PHONY: lint
lint:
	clang-tidy $(SRCS) -- $(CFLAGS)

.PHONY: format
format:
	@echo "Formatting source files with clang-format"
	clang-format -i $(SRC_FILES) $(HDR_FILES)

.PHONY: coverage
coverage: clean
	@echo "Building testing executable"
	$(MAKE) BUILD_TYPE=coverage
	@echo "Testing build completed"
	tests/tester.sh $(TARGET) $(TEST_DIR)
	@echo "Collecting coverage data..."
	mkdir -p $(COVERAGE_DIR)
	lcov --capture --directory . --output-file $(COVERAGE_DIR)/coverage.info
	lcov --extract $(COVERAGE_DIR)/coverage.info "$(pwd)/src/*" --output-file $(COVERAGE_DIR)/coverage.info
	genhtml $(COVERAGE_DIR)/coverage.info --output-directory $(COVERAGE_DIR)
	@echo "Coverage report generated at $(COVERAGE_DIR)/index.html"

.PHONY: test
test: $(TARGET)
	@echo "Running tests..."
	tests/tester.sh $(TARGET) $(TEST_DIR)

.PHONY: help
help:
	@echo "Production Makefile for $(PROJECT_NAME)"
	@echo ""
	@echo "Usage: make [target] [BUILD_TYPE=debug|release]"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build the project (default)"
	@echo "  static       - Build static library"
	@echo "  run          - Build and run the executable"
	@echo "  size         - Show size of executable"
	@echo "  clean        - Remove build artifacts"
	@echo "  lint    	  - Run clang-tidy on source files"
	@echo "  help         - Show this help message"
	@echo "  test         - Run tests"
	@echo "  coverage     - Generate code coverage report"
	@echo ""
	@echo "Build types:"
	@echo "  BUILD_TYPE=debug   - Build with debug symbols (default)"
	@echo "  BUILD_TYPE=release - Build optimized version"
	@echo "  BUILD_TYPE=coverage - Build with coverage flags"
	@echo ""