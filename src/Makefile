# Makefile for Secure Wipe
# NIST SP 800-88 Rev. 1 aligned data sanitization tool

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE -pthread
LDFLAGS = -pie -Wl,-z,relro,-z,now -pthread
TARGET = swipe
SRCDIR = src
OBJDIR = obj

# Source files
SOURCES = main.c config.c secure_utils.c trim_utils.c terminal_utils.c compliance.c ram_fill.c file_wipe.c directory_wipe.c
OBJECTS = $(SOURCES:.c=.o)

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files
%.o: %.c main.h config.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Install to system (optional)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/
	chmod 755 /usr/local/bin/$(TARGET)

# Uninstall from system (optional)
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Development targets
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Static analysis
lint:
	cppcheck --enable=all --std=c11 $(SOURCES)

# Format code
format:
	clang-format -i -style="{BasedOnStyle: Linux, IndentWidth: 4, TabWidth: 4, UseTab: Never}" $(SOURCES) *.h

# Run with basic test
test: $(TARGET)
	@echo "Creating test file..."
	@echo "This is test data for secure wipe verification." > test_file.txt
	@echo "Running secure wipe on test file..."
	@./$(TARGET) < /dev/null || true
	@echo "Test completed. Check if test_file.txt was removed."

# Help target
help:
	@echo "Secure Wipe Build System"
	@echo "========================"
	@echo "Targets:"
	@echo "  all      - Build the secure_wipe executable (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install to /usr/local/bin"
	@echo "  uninstall- Remove from /usr/local/bin"
	@echo "  debug    - Build with debug symbols"
	@echo "  lint     - Run static analysis"
	@echo "  format   - Format source code"
	@echo "  test     - Run basic functionality test"
	@echo "  help     - Show this help message"

.PHONY: all clean install uninstall debug lint format test help
