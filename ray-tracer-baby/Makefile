CC := cc

CFLAGS := -std=c11 -Wall
DEBUG_FLAGS := -D_DEBUG -D_LOGGING -D_DEFAULT_SOURCE -DSTB_IMAGE_WRITE_IMPLEMENTATION -g
RELEASE_FLAGS := -O2

# Directories
INCLUDE := -I./include -I./ -I./include/cmm
SRC_DIR := ./src
TARGET_DIR := ./target

TARGET_RELEASE_DIR := $(TARGET_DIR)/release
TARGET_DEBUG_DIR := $(TARGET_DIR)/debug
RUST_TARGET_DIR := ../obj-rs/target/release

# Source files
C_SRCS := $(wildcard $(SRC_DIR)/*.c) main.c

ifdef RELEASE
    CFLAGS += $(RELEASE_FLAGS)
    TARGET_PATH := $(TARGET_DIR)/release
else
    CFLAGS += $(DEBUG_FLAGS)
    TARGET_PATH := $(TARGET_DIR)/debug
endif

C_OBJS := $(patsubst %.c,$(TARGET_PATH)/%.o,$(notdir $(C_SRCS)))

RUST_LIB := ../obj-rs/target/release/libobj.a
LIBS := embree3 glfw GL GLEW cglm m pthread dl

ASSEMBLY := ray-tracer-baby
TARGET := $(TARGET_PATH)/$(ASSEMBLY)

all: $(TARGET)

$(TARGET): $(C_OBJS) $(RUST_LIB) | $(TARGET_PATH)
	$(CC) $^ -o $@ $(CFLAGS) $(INCLUDE) $(LIBS:%=-l%)

$(TARGET_PATH)/%.o: $(SRC_DIR)/%.c | $(TARGET_PATH)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(TARGET_PATH)/main.o: main.c | $(TARGET_PATH)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(TARGET_PATH):
	mkdir -p $@

# Dependency information
$(TARGET_PATH)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -MMD -MP -c $< -o $@

-include $(C_OBJS:.o=.d)

test:
	$(C_OBJS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(TARGET_DIR)
