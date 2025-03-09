# Thư mục
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
LIB_DIR = lib
INC_DIR = $(LIB_DIR)/inc

# Compiler
CC = gcc
CFLAGS = -Wall -Wextra -I$(INC_DIR) -MMD -MP

# Danh sách các tệp nguồn
LIB_SRCS = $(wildcard $(LIB_DIR)/*.c)
MAIN_SRC = main.c
SRCS = $(LIB_SRCS) $(MAIN_SRC)

# Tạo danh sách tệp object với đường dẫn tương ứng
LIB_OBJS = $(patsubst $(LIB_DIR)/%.c,$(OBJ_DIR)/$(LIB_DIR)/%.o,$(LIB_SRCS))
MAIN_OBJ = $(OBJ_DIR)/main.o
OBJS = $(LIB_OBJS) $(MAIN_OBJ)

# Tên file thực thi
TARGET = $(BUILD_DIR)/server

# Build tất cả
all: $(BUILD_DIR) $(OBJ_DIR) $(TARGET)

# Tạo thư mục build nếu chưa có
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Tạo thư mục obj và obj/lib nếu chưa có
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR) $(OBJ_DIR)/$(LIB_DIR)

# Biên dịch các file .c trong lib/
$(OBJ_DIR)/$(LIB_DIR)/%.o: $(LIB_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Biên dịch main.c
$(OBJ_DIR)/main.o: main.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Liên kết tất cả object files thành chương trình thực thi
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

# Dọn dẹp
clean:
	rm -rf $(BUILD_DIR)

# Tự động include dependencies từ -MMD
-include $(OBJS:.o=.d)

