############################################################################
# Makefile for a FreeRTOS POSIX demo (macOS/Linux) using the GCC/Posix port
############################################################################

CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -O2

# Directories (adjust relative paths if needed)
FREERTOS_KERNEL_DIR = ../Source
POSIX_PORT_DIR      = $(FREERTOS_KERNEL_DIR)/portable/ThirdParty/GCC/Posix

# Include Paths (these are relative to the EDF directory)
INCLUDES = -I. \
           -I$(FREERTOS_KERNEL_DIR)/include \
           -I$(POSIX_PORT_DIR)

# Application sources – assume they are in the current folder (EDF)
APP_SRCS = main.c custom_apis.c edf_scheduler.c posix_events.c

# FreeRTOS Kernel Sources
KERNEL_SRCS = \
    $(FREERTOS_KERNEL_DIR)/tasks.c \
    $(FREERTOS_KERNEL_DIR)/queue.c \
    $(FREERTOS_KERNEL_DIR)/list.c \
    $(FREERTOS_KERNEL_DIR)/timers.c \
    $(FREERTOS_KERNEL_DIR)/event_groups.c \
    $(FREERTOS_KERNEL_DIR)/stream_buffer.c \
    $(FREERTOS_KERNEL_DIR)/portable/MemMang/heap_4.c

# POSIX Port Source
PORT_SRCS = $(POSIX_PORT_DIR)/port.c

# Combine all sources
SRCS = $(APP_SRCS) $(KERNEL_SRCS) $(PORT_SRCS)
OBJS = $(SRCS:.c=.o)
TARGET = freertos_edf_demo

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
