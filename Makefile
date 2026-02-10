# Programs
CC          := gcc
RM          := rm -rf
MKDIR       := mkdir -p

# Flags
CFLAGS      := -Wall -Wextra
CPPFLAGS    := 
LDFLAGS     := 
LDLIBS      := 

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

# Sources
SOURCES     := $(shell find $(SRCDIR) -type f -name '*.c')
SERVER_SRC  := $(shell find $(SERVERDIR) -type f -name '*.c')
CLIENT_SRC  := $(shell find $(CLIENTDIR) -type f -name '*.c')

# Objects
OBJECTS     := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
SERVER_OBJ  := $(patsubst $(SERVERDIR)/%.c,$(BUILDDIR)/%.o,$(SERVER_SRC))
CLIENT_OBJ  := $(patsubst $(CLIENTDIR)/%.c,$(BUILDDIR)/%.o,$(CLIENT_SRC))

# Include paths
INC         := -I$(INCDIR)
CPPFLAGS    += $(INC)

# Dependency files
DEPS        := $(OBJECTS:.o=.d) $(SERVER_OBJ:.o=.d) $(CLIENT_OBJ:.o=.d)

.PHONY: all clean debug release

all: $(SERVER_EXE) $(CLIENT_EXE)

$(SERVER_EXE): $(OBJECTS) $(SERVER_OBJ) | $(BINDIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@ -lrt

$(CLIENT_EXE): $(OBJECTS) $(CLIENT_OBJ) | $(BINDIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR)/%.o: $(SERVERDIR)/%.c | $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR)/%.o: $(CLIENTDIR)/%.c | $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BINDIR):
	@$(MKDIR) $@

$(BUILDDIR):
	@$(MKDIR) $@

clean:
	$(RM) $(BUILDDIR) $(BINDIR)

debug: CFLAGS += -g -DDEBUG
debug: all

release: CFLAGS += -O2 -DNDEBUG
release: all

-include $(DEPS)
