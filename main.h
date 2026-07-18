#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <fcntl.h>       // Cho các cờ điều khiển File (O_RDWR, O_NOCTTY)
#include <termios.h>     // Cấu hình UART
#include <pthread.h>     // Cấu hình Multi-thread
#include <sqlite3.h>     // API cơ sở dữ liệu
#include <time.h>        // Xử lý Unix Timestamp
#include <termios.h>     // Thư viện cấu hình UART chuyên dụng trên Linux
#include <unistd.h>      // Chứa các hàm write(), close(), usleep()

// Định nghĩa kích thước bộ đệm để tránh tràn RAM
#define MAX_NAME_LEN 			(16)
#define LOG_BUFFER_SIZE 		(128)
#define UART_DEVICE 			("/dev/ttyO1")
#define DATABASE_NAME			("memberdb")
#define DATABASE_TABLE_NAME		("memberlst")
#define NONE_FINGER_PRINT_ID	(99)

// Enum trạng thái máy ảo (FSM) của module vân tay
typedef enum {
    FSM_NONE,
    FSM_FINGER_SEND_GENIMG = 1, // Yêu cầu chụp ảnh mặt kính
    FSM_FINGER_WAIT_GENIMG,     // Đợi kết quả chụp ảnh
    FSM_FINGER_SEND_IMG2TZ,     // Yêu cầu chuyển ảnh thành đặc trưng
    FSM_FINGER_WAIT_IMG2TZ,     // Đợi kết quả chuyển đặc trưng
    FSM_FINGER_SEND_SEARCH,     // Yêu cầu tìm kiếm 1:N trong thư viện Flash
    FSM_FINGER_WAIT_SEARCH,     // Đợi kết quả tìm kiếm (Quan trọng nhất)
    FSM_FINGER_DELAY            // Nghỉ 1 chút xíu
} Fingerprint_State_t;

// Biến toàn cục để dùng chung
extern int uart_fd;
extern pthread_mutex_t uart_mutex;
extern bool g_bIsContinueLoop;
extern pthread_t timeDisplay_thread, SearchMember_thread;

void UART_Init(void);
void UART_End(void);
int Database_Init(const char* db_name);
void Database_End(void);
void handle_exit(int sig);
void* handle_TimeDisplay_thread(void *arg);
void* handle_SearchMember_thread(void *arg);
int get_user_name(int id, char* output_name);

#endif