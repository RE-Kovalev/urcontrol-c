# Compiler and archiver
CC ?= cc
AR ?= ar

# Static library name
LIB_NAME  ?= liburcontrol.a
# Command-line application name
CLI_NAME  ?= urcontrol-cli

# Object files directory
OBJ_DIR ?= obj

# Automatic dependency generation
DEPFLAGS = -MMD -MP -MF $(@:.o=.d) -MT $@

# ALSA library
ALSA_LIBS = $(shell pkg-config --libs alsa)

# Compiler flags:
# -O2 - enable optimizations
# -Wall - enable common warnings
# -Wextra - enable additional warnings
# -Wpedantic - enforce strict ISO C compliance
# -std=c11 - use C11 language standart
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=gnu11
# Linker flags
# -pthread - POSIX thread support
LDFLAGS ?= $(ALSA_LIBS) -pthread


# Library source files
LIB_SOURCES = urcontrol.c urcontrol_params.c
LIB_OBJECTS = $(addprefix $(OBJ_DIR)/,$(LIB_SOURCES:.c=.o))

# Command-line application files
CLI_SOURCES = urcontrol_cli.c
CLI_OBJECTS = $(addprefix $(OBJ_DIR)/,$(CLI_SOURCES:.c=.o))

# BUILD TARGETS

.PHONY: all clean 

# Build the static library and command-line application
all: $(LIB_NAME) $(CLI_NAME)

# STATIC LIBRARY
$(LIB_NAME): $(LIB_OBJECTS)
	$(AR) rsc $@ $^

# Command-line application
$(CLI_NAME): $(CLI_OBJECTS) $(LIB_NAME)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compilation rules
$(OBJ_DIR)/%.o: %.c $(OBJ_DIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -pthread -c $< -o $@

# Cleanup
clean:
	rm -fr $(OBJ_DIR) $(LIB_NAME) $(CLI_NAME) 

# Automatic dependencies
# Include generated dependency files when available
-include $(wildcard $(OBJ_DIR)/*.d)