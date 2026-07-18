#include "main.h"

int uart_fd;

void UART_Init(void)
{
	// 1. UART1 Configuration
	// 1.1. Common Configuration
    // O_RDWR: Mở để đọc và ghi
    // O_NOCTTY: Không biến cổng này thành terminal điều khiển của hệ thống
    uart_fd = open("/dev/ttyS1", O_RDWR | O_NOCTTY);
    
    if (uart_fd == -1) {
        printf("Loi: Khong the mo cong UART1 (/dev/ttyS1). Kiem tra lai quyen root!\n");
        return;
    }

    // 2. CẤU HÌNH UART1 (Baudrate 115200, 8 bit dữ liệu, no parity, 1 stop bit)
    struct termios options;
    tcgetattr(uart_fd, &options); // Lấy cấu hình hiện tại của cổng

    cfsetispeed(&options, B115200); // Đặt tốc độ nhận là 115200
    cfsetospeed(&options, B115200); // Đặt tốc độ truyền là 115200

    options.c_cflag |= (CLOCAL | CREAD); // Cho phép bộ nhận và đặt chế độ local
    options.c_cflag &= ~PARENB;          // Không dùng bit kiểm tra chẵn lẻ (No parity)
    options.c_cflag &= ~CSTOPB;          // Dùng 1 bit dừng (1 Stop bit)
    options.c_cflag &= ~CSIZE;           // Xóa cấu hình kích thước bit cũ
    options.c_cflag |= CS8;              // Đặt kích thước dữ liệu là 8 bit (8 Data bits)

    // Áp dụng cấu hình ngay lập tức
    tcsetattr(uart_fd, TCSANOW, &options);

	// 1.2. Specific Configuration
	struct termios tty;
	tcgetattr(uart_fd, &tty);

	// Tắt các xử lý ký tự đặc biệt của Linux (chuyển sang Raw Mode)
	cfmakeraw(&tty); 

	// Cấu hình điều kiện block cho hàm read()
	tty.c_cc[VMIN]  = 1; // Block cho đến khi nhận được ít nhất 1 byte
	tty.c_cc[VTIME] = 5; // Không sử dụng inter-character timer (chờ vô hạn)

	tcsetattr(uart_fd, TCSANOW, &tty);

    printf("UART1 da san sang. Dang bat dau truyen du lieu...\n");
}

void UART_End(void)
{
	// 4. ĐÓNG CỔNG UART
    close(uart_fd);
    printf("Da dong cong UART1.\n");
}
