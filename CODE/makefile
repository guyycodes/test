###############################################################################
# Compiler and Flags
###############################################################################
CC      = gcc
# -static => produce a statically linked binary
# -I. => look in current directory for headers
# -g => include debug symbols (optional)
# -w => suppress warnings (optional, not recommended in production)
CFLAGS  = -I. -g -w
LDFLAGS = -static -lpthread -lrt

###############################################################################
# Source Files
###############################################################################
# Put any .c files common to both server and client here, e.g., your message queue code:
COMMON_SRC  = prototype_defs.c

# If your server has more .c files, list them all here (space-separated).
SRV_SRC     = server.c
CLI_SRC     = client.c

# Convert .c file names into .o file names automatically.
COMMON_OBJ  = $(COMMON_SRC:.c=.o)
SRV_OBJ     = $(SRV_SRC:.c=.o)
CLI_OBJ     = $(CLI_SRC:.c=.o)

# If you have headers that multiple .c files depend on, list them in DEPS:
DEPS        = prototype_defs.h

###############################################################################
# Default Target
###############################################################################
all: server client

###############################################################################
# Build Rules
###############################################################################

# Generic rule to build any .o from .c (depends on headers too)
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# Link the server executable (includes both SRV_OBJ and COMMON_OBJ)
server: $(SRV_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Link the client executable (includes both CLI_OBJ and COMMON_OBJ)
client: $(CLI_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

###############################################################################
# Cleaning
###############################################################################
.PHONY: clean
clean:
	rm -f *.o server client
