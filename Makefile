# Compiler and tools
CC          := gcc
RM          := rm -rf
MKDIR       := mkdir -p

# Directories
SRCDIR      := src
SERVERDIR   := server
CLIENTDIR   := client
INCDIR      := inc
BUILDDIR    := build
BINDIR      := bin

# Targets
SERVER_NAME := server.out
CLIENT_NAME := client.out
SERVER_EXE  := $(BINDIR)/$(SERVER_NAME)
CLIENT_EXE  := $(BINDIR)/$(CLIENT_NAME)

# Flags
CFLAGS      := -Wall -Wextra -pthread
CPPFLAGS    := -I$(INCDIR)
LDFLAGS     :=
LDLIBS      := -lrt -pthread

# Sources
COMMON_SRC  := $(shell find $(SRCDIR) -type f -name '*.c')
SERVER_SRC  := $(shell find $(SERVERDIR) -type f -name '*.c')
CLIENT_SRC  := $(shell find $(CLIENTDIR) -type f -name '*.c')
ALL_SRC     := $(COMMON_SRC) $(SERVER_SRC) $(CLIENT_SRC)

# Objects
OBJECTS     := $(patsubst %.c,$(BUILDDIR)/%.o,$(ALL_SRC))

# Dependency files
DEPS        := $(OBJECTS:.o=.d)

.PHONY: all clean debug release

all: $(SERVER_EXE) $(CLIENT_EXE)

# Link server
$(SERVER_EXE): $(filter $(BUILDDIR)/$(SRCDIR)/% \
                        $(BUILDDIR)/$(SERVERDIR)/%, $(OBJECTS)) | $(BINDIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Link client
$(CLIENT_EXE): $(filter $(BUILDDIR)/$(SRCDIR)/% \
                        $(BUILDDIR)/$(CLIENTDIR)/%, $(OBJECTS)) | $(BINDIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Compile rule
$(BUILDDIR)/%.o: %.c
	@$(MKDIR) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BINDIR):
	@$(MKDIR) $@

clean:
	$(RM) $(BUILDDIR) $(BINDIR)

debug: CFLAGS += -g -O0 -DDEBUG
debug: all

release: CFLAGS += -O2 -DNDEBUG
release: all

-include $(DEPS)
