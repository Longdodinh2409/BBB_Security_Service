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
#define UART_DEVICE 			("/dev/ttyS1")
#define DATABASE_NAME			("memberdb.db")
#define DATABASE_TABLE_NAME		("memberlst")
#define NONE_FINGER_PRINT_ID	(99)
#define OFFSET_MEMBER_ID		(1)

// Enum trạng thái máy ảo (FSM) của module vân tay
typedef enum {
    FSM_NONE,
    FSM_FINGER_SEND_GENIMG = 1, // Yêu cầu chụp ảnh mặt kính
    FSM_FINGER_WAIT_GENIMG,     // Đợi kết quả chụp ảnh
    FSM_FINGER_SEND_IMG2TZ,     // Yêu cầu chuyển ảnh thành đặc trưng
    FSM_FINGER_WAIT_IMG2TZ,     // Đợi kết quả chuyển đặc trưng
    FSM_FINGER_SEND_SEARCH,     // Yêu cầu tìm kiếm 1:N trong thư viện Flash
    FSM_FINGER_WAIT_SEARCH,     // Đợi kết quả tìm kiếm (Quan trọng nhất)
    FSM_FINGER_DELAY,        	// Nghỉ 1 chút xíu
	FSM_FINGER_BLOCK_5M,		// Bị block 5 phút khi sai vân tay 5 lần
	FSM_FINGER_BLOCK_10M,		// Bị block 10 phút khi sai vân tay 10 lần
	FSM_FINGER_BLOCK_INF,		// Bị block mãi mãi khi sai vân tay 15 lần, cho đến khi BBB unlock
	FSM_FINGER_UNBLOCK,			// Đã hết giờ Block

	FSM_SYSTEM_STM32F4_WAKEUP,	// STM32F4 wake up (initialize done!)

	FSM_ENROLL_REQUEST_ID,      // STM32F4 yêu cầu BBB cấp ID mới cho enrollment
	FSM_ENROLL_ID_ERROR,        // BBB không cấp được ID mới cho enrollment
	FSM_NEW_FINGERPRINT_ADDED,
	FSM_REMOVE_SPECIFIC_FINGERPRINT
} Fingerprint_State_t;

// Biến toàn cục để dùng chung
extern int uart_fd;
extern pthread_mutex_t uart_mutex;
extern bool g_bIsContinueLoop;
extern pthread_t timeDisplay_thread, SearchMember_thread;
extern pthread_mutex_t log_lock;

void UART_Init(void);
void UART_End(void);
int Database_Init(const char* db_name);
void Database_End(void);
void handle_exit(int sig);

void* handle_TimeDisplay_thread(void *arg);
void* handle_SearchMember_thread(void *arg);
void* handle_CLI_thread(void *arg);
void ProcessWriteHistoryLog(uint8_t u8State, const char* pcname);

int get_user_name(int id, char* output_name);
int get_lowest_available_id(void);
int add_new_member(int id, const char* name);
int delete_member(int id) ;

#endif