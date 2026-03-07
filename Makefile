.PHONY: all build debug release test clean reconfigure format format-check install-hooks help

# Default build directory and type
BUILD_DIR ?= build
CMAKE_BUILD_TYPE ?= Debug

help:
	@echo "gthreads build system - available targets:"
	@echo ""
	@echo "Build & Test:"
	@echo "  make build           Build in debug mode (default)"
	@echo "  make debug           Build in debug mode (explicit)"
	@echo "  make release         Build in release mode"
	@echo "  make rebuild         Clean and rebuild"
	@echo "  make test            Run all tests"
	@echo "  make test-verbose    Run tests with verbose output"
	@echo ""
	@echo "Code Quality:"
	@echo "  make format          Format source code with clang-format"
	@echo "  make format-check    Check formatting (non-destructive)"
	@echo ""
	@echo "Setup & Cleanup:"
	@echo "  make reconfigure     Reconfigure CMake (forces cmake -S . -B build)"
	@echo "  make install-hooks   Install git hooks"
	@echo "  make clean           Remove build directory"
	@echo "  make distclean       Clean build + CMake cache"
	@echo ""
	@echo "Options (set with make VAR=value):"
	@echo "  BUILD_DIR            Custom build directory (default: build)"
	@echo "  CMAKE_BUILD_TYPE     Debug or Release (default: Debug)"
	@echo ""

# Default target
all: build test

# Build targets
build: $(BUILD_DIR)/Makefile
	@cmake --build $(BUILD_DIR)

debug: CMAKE_BUILD_TYPE := Debug
debug: reconfigure build

release: CMAKE_BUILD_TYPE := Release
release: reconfigure build

$(BUILD_DIR)/Makefile:
	@echo "Configuring CMake with CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)..."
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

reconfigure:
	@echo "Reconfiguring CMake with CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)..."
	@rm -rf $(BUILD_DIR)
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

rebuild: clean build

# Test targets
test: build
	@echo "Running tests..."
	@ctest --test-dir $(BUILD_DIR) --output-on-failure

test-verbose: build
	@echo "Running tests (verbose)..."
	@ctest --test-dir $(BUILD_DIR) --output-on-failure -VV

# Format targets
format: build
	@echo "Formatting source code..."
	@cmake --build $(BUILD_DIR) --target format

format-check: build
	@echo "Checking source code formatting..."
	@cmake --build $(BUILD_DIR) --target format-check

# Git hooks
install-hooks:
	@echo "Installing git hooks..."
	@./scripts/install-hooks.sh

# Cleanup targets
clean:
	@echo "Removing build directory..."
	@rm -rf $(BUILD_DIR)

distclean: clean
	@echo "Removing CMake cache and artifacts..."
	@find . -name "CMakeCache.txt" -delete
	@find . -name "CMakeFiles" -type d -exec rm -rf {} + 2>/dev/null || true
	@find . -name ".cmake" -type d -exec rm -rf {} + 2>/dev/null || true
	@find . -name "Testing" -type d -exec rm -rf {} + 2>/dev/null || true

# Combined convenience targets
check: format-check test
	@echo "All checks passed!"

all-checks: format-check build test
	@echo "All checks passed!"

setup: reconfigure install-hooks
	@echo "Setup complete! Run 'make test' to verify."
