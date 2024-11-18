# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g

# Libraries to link
LIBS = -lm -lasound -lsqlite3 -lpthread -lcurl -lpaho-mqtt3c -lssl -lcrypto -lsqlite3 -lcjson

# Target application name
TARGET = sound_app

# Source files and object files
SRCS = sound_app.c abs.c alsa_thr.c fft_thr.c FFTImplementationCallback.c rt_nonfinite.c rtGetInf.c rtGetNaN.c sound_freq_initialize.c sound_freq_terminate.c sound_freq.c db_helper.c send_restapi.c
OBJS = $(SRCS:.c=.o)

# Default target to build the application
all: $(TARGET)

# Rule to link object files and create the executable with libraries
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# Rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

# Run the application
run: $(TARGET)
	./$(TARGET)

# Phony targets
.PHONY: all clean run