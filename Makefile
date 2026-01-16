CC ?= cc
CFLAGS ?= -Wall -Wextra -Werror -std=c99 -pedantic
LDLIBS ?= -pthread
INC_DIR := include
SRC_DIR := src
OBJ_DIR := build
LIB_NAME := libconsole_input.a

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: all clean example

all: $(LIB_NAME)

$(LIB_NAME): $(OBJS)
	@ar rcs $@ $^
	@echo "Built $@"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $@

example: $(LIB_NAME)
	$(CC) $(CFLAGS) -I$(INC_DIR) examples/basic.c $(LIB_NAME) -o examples/basic

clean:
	rm -rf $(OBJ_DIR) $(LIB_NAME) examples/basic
