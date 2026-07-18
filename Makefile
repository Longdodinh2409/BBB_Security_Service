# Khai báo trình biên dịch chéo
CROSS_COMPILE ?= arm-linux-gnueabihf-
CC = $(CROSS_COMPILE)gcc

# Chỉ định đường dẫn tới thư mục ảo chứa thư viện (Sysroot)
SYSROOT = ./sysroot

# Thêm -I để chỉ đường dẫn tìm file .h
CFLAGS = -Wall -Wextra -pthread -MMD -MP -I$(SYSROOT)/usr/include

# Thêm -L để chỉ đường dẫn tìm file .so, và -l để link với sqlite3
LDFLAGS = -pthread -L$(SYSROOT)/usr/lib -lsqlite3

# Thư mục Output (cùng cấp với các file .c/.h)
OUT_DIR = Output

TARGET = $(OUT_DIR)/main_SecSer
SRCS = main.c database.c threads.c uart.c

# Cập nhật OBJS và DEPS để thêm prefix $(OUT_DIR)/ phía trước tên file
OBJS = $(SRCS:%.c=$(OUT_DIR)/%.o)
DEPS = $(SRCS:%.c=$(OUT_DIR)/%.d)

all: $(TARGET)

# Rule để tự động tạo thư mục Output nếu chưa tồn tại
$(OUT_DIR):
	mkdir -p $(OUT_DIR)

# Link các file .o thành file thực thi
$(TARGET): $(OBJS) | $(OUT_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	scp $(TARGET) debian@192.168.137.2:/home/debian/Security_service_BBB

# Rule build các file .c thành .o đặt trong thư mục Output
# Dấu | $(OUT_DIR) (Order-only prerequisite) đảm bảo thư mục Output phải được tạo ra TRƯỚC KHI biên dịch
$(OUT_DIR)/%.o: %.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)

# Clean đơn giản là xóa luôn cả thư mục Output
clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)