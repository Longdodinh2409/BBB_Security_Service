#include "main.h"

static char s_acTXSearchMemberBuffer[LOG_BUFFER_SIZE];
static char s_acRXSearchMemberBuffer[LOG_BUFFER_SIZE];
static uint8_t s_u8State;
static uint8_t s_u8FingerID = NONE_FINGER_PRINT_ID;
static char s_acMemberName[MAX_NAME_LEN];
static char s_acTXTimeDisplayBuffer[64];

static bool bIsSendTimeDisplayBy1Sec = false;

static int s_pending_add_id = -1;

pthread_mutex_t g_time_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_time_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t g_db_mutex = PTHREAD_MUTEX_INITIALIZER;

void handle_exit(int sig)
{
    g_bIsContinueLoop = false;
	// printf("Prepare to end this process...\n");

	// temporarily unused this argument. Remove when use it!
	(void)sig;
}

void* handle_TimeDisplay_thread(void *arg)
{
	// unused argument
	(void)arg; 

	while(g_bIsContinueLoop)
	{
        time_t raw_time = time(NULL); // Lấy raw Unix Timestamp

        // Chuyển con số thành chuỗi để thiết bị đầu cuối bên kia dễ đọc
        snprintf(s_acTXTimeDisplayBuffer, sizeof(s_acTXTimeDisplayBuffer), "#TS=%ld;", raw_time);

        // Gửi chuỗi này qua UART
        int bytes_written = write(uart_fd, s_acTXTimeDisplayBuffer, strlen(s_acTXTimeDisplayBuffer));
        
        if (bytes_written < 0) {
            printf("Loi truyen du lieu qua UART!\n");
        } else {
            printf("Da gui qua UART1: %s\n", s_acTXTimeDisplayBuffer);
        }

		// --- PTHREAD_COND_TIMEDWAIT --- //
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts); // Get real time from BBB system

        pthread_mutex_lock(&g_time_mutex);
        
        if (bIsSendTimeDisplayBy1Sec == true)
        {
            ts.tv_sec += 1;  // timeout 1 sec
        }
        else
        {
            ts.tv_sec += 60; // timeout 60 sec
        }

        // Thread sẽ block ở đây tối đa theo thời gian ts, HOẶC cho đến khi nhận được signal
        pthread_cond_timedwait(&g_time_cond, &g_time_mutex, &ts);
        
        pthread_mutex_unlock(&g_time_mutex);
    }

	return NULL;
}

void* handle_SearchMember_thread(void *arg)
{
	int parsed_count = 0;
	int result = 0;
	// unused argument
	(void)arg;

	while(g_bIsContinueLoop)
	{
		// Step 1: Wait RX data
		int rx_length = read(uart_fd, (void *)s_acRXSearchMemberBuffer, LOG_BUFFER_SIZE);
		if (rx_length > 0)
		{
			s_acRXSearchMemberBuffer[rx_length] = '\0';

			// Step 2A: Parsing frame: #State=x,#ID=y
			parsed_count = sscanf(s_acRXSearchMemberBuffer, "#State=%hhu,#ID=%hhu", &s_u8State, &s_u8FingerID);
			// hhu : half-half unsigned = uint8_t (1 byte)

			if (parsed_count == 2)
			{
				printf("Parsing success. State: %d - ID: %d\n", s_u8State, s_u8FingerID);

				if (s_u8State == FSM_NEW_FINGERPRINT_ADDED)
				{
					if (s_pending_add_id == s_u8FingerID)
					{
						printf("\n[SYSTEM] Van tay moi da duoc tao thanh cong (UART ID: %d).\n", s_pending_add_id);
						printf("BBB_Admin's Request > Hay go lenh: add %d <Ten_thanh_vien> de luu Member thu %d vao Database!\nBBB_Admin> ", (s_pending_add_id + 1), (s_pending_add_id + 1));
						fflush(stdout);
					}
					else
					{
						printf("[SYSTEM] Nhận trạng thái add thành công nhưng ID không khớp pending ID (%d).\n", s_pending_add_id);
					}
					continue;
				}

				// Step 3: Get ID and return Member Name
				result = get_user_name((s_u8FingerID + OFFSET_MEMBER_ID), s_acMemberName);
				if (result == 1)
				{
					printf("Found! It's %s\n", s_acMemberName);

					// UART send Member Name to STM32F4
					snprintf(s_acTXSearchMemberBuffer, LOG_BUFFER_SIZE, "#State=%hhu,#Name=%s;", s_u8State, s_acMemberName);
					int bytes_written = write(uart_fd, s_acTXSearchMemberBuffer, strlen(s_acTXSearchMemberBuffer));

					// Save history
					ProcessWriteHistoryLog(s_u8State, s_acMemberName);

					if (bytes_written < 0) 
					{
						printf("Error comm UART!\n");
					} 
					else 
					{
						printf("Da gui qua UART1: %s\n", s_acTXSearchMemberBuffer);
					}
				}
				else
				{
					printf("Unknown member\n");
					snprintf(s_acTXSearchMemberBuffer, LOG_BUFFER_SIZE, "#State=%hhu,#Name=Unknown;", s_u8State);
					int bytes_written = write(uart_fd, s_acTXSearchMemberBuffer, strlen(s_acTXSearchMemberBuffer));

					// Save history
					ProcessWriteHistoryLog(s_u8State, "Unknown");

					if (bytes_written < 0) 
					{
						printf("Error comm UART!\n");
					} 
					else 
					{
						printf("Da gui qua UART1: %s\n", s_acTXSearchMemberBuffer);
					}
				}
			}
			else
			{
				// Step 2B: Parsing frame: #State=x
				parsed_count = sscanf(s_acRXSearchMemberBuffer, "#State=%hhu", &s_u8State);
				if (parsed_count == 1)
				{
					printf("Parsing success. State: %d\n", s_u8State);

					if (s_u8State == FSM_ENROLL_REQUEST_ID)
					{
						int db_id = get_lowest_available_id();
						int uart_id = 0;

						if (db_id > 0)
						{
							uart_id = db_id - OFFSET_MEMBER_ID;
							s_pending_add_id = uart_id;
							snprintf(s_acTXSearchMemberBuffer, LOG_BUFFER_SIZE, "#State=%hhu,#ID=%hhu;", s_u8State, (uint8_t)uart_id);
							int bytes_written = write(uart_fd, s_acTXSearchMemberBuffer, strlen(s_acTXSearchMemberBuffer));

							if (bytes_written < 0)
							{
								printf("Error comm UART!\n");
							}
							else
							{
								printf("Da gui qua UART1: %s\n", s_acTXSearchMemberBuffer);
							}
						}
						else
						{
							printf("[SYSTEM] Khong the lay ID trong DB cho enroll.\n");
						}
					}
					else if (s_u8State == FSM_ENROLL_ID_ERROR)
					{
						s_pending_add_id = -1;
						printf("[SYSTEM] Rollback pending add ID do receive enroll error.\n");
					}
					else
					{
						pthread_mutex_lock(&g_time_mutex);
    
						if ((s_u8State == FSM_FINGER_BLOCK_5M) || (s_u8State == FSM_FINGER_BLOCK_10M))
						{
							bIsSendTimeDisplayBy1Sec = true;

							// Save history
							ProcessWriteHistoryLog(s_u8State, "Locked temporarily");

							// Đánh thức TimeDisplay_thread NGAY LẬP TỨC để bắt đầu gửi mỗi 1 giây
							pthread_cond_signal(&g_time_cond); 
						}
						if (s_u8State == FSM_FINGER_BLOCK_INF)
						{
							// Save history
							ProcessWriteHistoryLog(s_u8State, "Locked infinity!");
							printf("BBB_Admin's Request > Hay go lenh: 'unblock' de thoat khoi tinh trang Lock Infinity!\n");

							// Đánh thức TimeDisplay_thread NGAY LẬP TỨC để bắt đầu gửi mỗi 1 giây
							pthread_cond_signal(&g_time_cond); 
						}
						else if (s_u8State == FSM_FINGER_UNBLOCK)
						{
							bIsSendTimeDisplayBy1Sec = false;

							// Save history
							ProcessWriteHistoryLog(s_u8State, "Unlocked");

							// cũng có thể gọi signal ở đây nếu muốn thread lập tức dừng gửi 1s 
							// và chuyển ngay sang chế độ chờ 60s mà không phải đợi nốt chu kỳ 1s hiện tại.
							pthread_cond_signal(&g_time_cond);
						}
						else if (s_u8State == FSM_SYSTEM_STM32F4_WAKEUP)
						{
							pthread_cond_signal(&g_time_cond);
						}
						
						pthread_mutex_unlock(&g_time_mutex);
					}
				}
				else
				{
					printf("Parsing failed. Frame: %s. Wait another frame!\n", s_acRXSearchMemberBuffer);
					continue;
				}
			}
		}
		else if (rx_length == 0)	// timeout
		{
			continue;
		}
		else	// unknown error
		{
			printf("Unknown error in UART - Search Member thread \n");
		}
	}

	return NULL;
}

void* handle_CLI_thread(void *arg)
{
    (void)arg;
    char input_buffer[256];
    int input_id;
    char input_name[MAX_NAME_LEN];
	int req;

    // Tạo độ trễ nhỏ lúc khởi động để tránh terminal in đè lên các log khởi tạo của hệ thống
    sleep(1); 

    while (g_bIsContinueLoop)
    {
        // printf("BBB_Admin> ");
        // fflush(stdout); // Ép terminal in ra dấu nhắc lệnh ngay lập tức

        // fgets sẽ block luồng này lại cho đến khi bạn nhấn Enter (Không ăn CPU)
        if (fgets(input_buffer, sizeof(input_buffer), stdin) != NULL)
        {
            // Xóa bỏ ký tự '\n' ở cuối chuỗi do Enter tạo ra
            input_buffer[strcspn(input_buffer, "\n")] = '\0';

            // Nếu chỉ nhấn Enter mà không nhập gì thì bỏ qua
            if (strlen(input_buffer) == 0) continue;

            // --- KIỂM TRA LỆNH XÓA ---
            // Format mong muốn: delete <ID>
            if (sscanf(input_buffer, "delete %d", &input_id) == 1)
            {
				req = get_user_name(input_id, s_acMemberName);
				if (req == 1)
					printf("get_user_name success!\n");
				else
					printf("get_user_name failed!\n");
                
				req = delete_member(input_id);
				if (req == 1)
					printf("delete_member success!\n");
				else
					printf("delete_member failed!\n");

				// Save history
				ProcessWriteHistoryLog((uint8_t)FSM_REMOVE_SPECIFIC_FINGERPRINT, s_acMemberName);

            }
            // --- KIỂM TRA LỆNH THÊM ---
            // Format mong muốn: add <ID> <Tên có chứa dấu cách>
            // %[^\n] nghĩa là đọc toàn bộ các ký tự cho đến khi gặp \n (cho phép khoảng trắng)
            else if (sscanf(input_buffer, "add %d %[^\n]", &input_id, input_name) == 2)
            {
                // if (s_u8State == FSM_NEW_FINGERPRINT_ADDED && s_pending_add_id == input_id)
				if ((s_pending_add_id + 1) == input_id)
				{
					int res = add_new_member(input_id, input_name);
					if (res == 1) 
					{
						// Xóa cờ trạng thái sau khi thêm thành công để tránh add lặp lại
						s_pending_add_id = -1;
						
						// Save history
						ProcessWriteHistoryLog((uint8_t)FSM_NEW_FINGERPRINT_ADDED, input_name);
					}
				}
				// else if (s_u8State != FSM_NEW_FINGERPRINT_ADDED)
				// {
				// 	printf("Loi: Module chua san sang! Vui long quet van tay de tao ID moi tren module truoc.\n");
				// }
				else 
				{
					printf("Loi: ID khong khop! ID dang cho de them la %d.\n", s_pending_add_id);
				}
            }
			else if (strcmp(input_buffer, "unblock") == 0)
			{
				snprintf(s_acTXSearchMemberBuffer, LOG_BUFFER_SIZE, "#State=%hhu;", FSM_FINGER_UNBLOCK);
				int bytes_written = write(uart_fd, s_acTXSearchMemberBuffer, strlen(s_acTXSearchMemberBuffer));

				if (bytes_written < 0) {
					printf("Loi truyen du lieu qua UART!\n");
				} else {
					printf("Da gui qua UART1: %s\n", s_acTXTimeDisplayBuffer);
				}

				bIsSendTimeDisplayBy1Sec = false;

				// Save history
				ProcessWriteHistoryLog(FSM_FINGER_UNBLOCK, "Unlocked");

				// cũng có thể gọi signal ở đây nếu muốn thread lập tức dừng gửi 1s 
				// và chuyển ngay sang chế độ chờ 60s mà không phải đợi nốt chu kỳ 1s hiện tại.
				pthread_cond_signal(&g_time_cond);
			}
            // // --- KIỂM TRA LỆNH THOÁT ---
            // else if (strcmp(input_buffer, "exit") == 0 || strcmp(input_buffer, "quit") == 0)
            // {
            //     printf("Dang tat he thong...\n");
            //     g_bIsContinueLoop = false;
            //     break; // Thoát vòng lặp, kết thúc chương trình an toàn
            // }
            // // --- CÚ PHÁP SAI ---
            else
            {
                printf("BBB_Admin's >Lenh khong hop le! %s \n", input_buffer);
                printf(" - Them:   add <ID> <Ten>\n");
                printf(" - Xoa:    delete <ID>\n");
				printf(" - Ket thuc Block Inifinity:    unblock\n");
                printf(" - Thoat:  exit\n");
            }
        }
    }

    return NULL;
}

void ProcessWriteHistoryLog(uint8_t u8State, const char* pcname)
{
	char acLog[64];

	if (pcname == NULL)
	{
		printf("Error with pcname parameter. State: %d\n", u8State);
		pthread_mutex_unlock(&log_lock);
		return;
	}

	pthread_mutex_lock(&log_lock);

	FILE* log_fp = fopen("Security_history_log.txt", "a");
	if (log_fp == NULL)
	{
		printf("Fail at open & create Security_history_log.txt file\n");
		pthread_mutex_unlock(&log_lock);
		return;
	}

	// Get TimeStamp
	time_t raw_time = time(NULL);
    struct tm *time_info = localtime(&raw_time);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

	if (u8State == (uint8_t)FSM_FINGER_WAIT_SEARCH)
	{
		sprintf(acLog, "[%s] Accessed: %s\n", time_str, pcname);
	}
	else if (u8State == (uint8_t)FSM_NEW_FINGERPRINT_ADDED)
	{
		sprintf(acLog, "[%s] Added: %s\n", time_str, pcname);
	}
	else if (u8State == (uint8_t)FSM_REMOVE_SPECIFIC_FINGERPRINT)
	{
		sprintf(acLog, "[%s] Removed: %s\n", time_str, pcname);
	}
	else if (u8State == (uint8_t)FSM_FINGER_BLOCK_5M)		
	{
		sprintf(acLog, "[%s] System locked %d minutes\n", time_str, 5);
	}
	else if (u8State == (uint8_t)FSM_FINGER_BLOCK_10M)
	{
		sprintf(acLog, "[%s] System locked %d minutes\n", time_str, 10);
	}
	else if (u8State == (uint8_t)FSM_FINGER_BLOCK_INF)
	{
		sprintf(acLog, "[%s] System locked infinity!\n", time_str);
	}
	else if (u8State == (uint8_t)FSM_FINGER_UNBLOCK)
	{
		sprintf(acLog, "[%s] System unlock!\n", time_str);
	}
	else
	{
		sprintf(acLog, "[%s] Invalid event. State: %d\n", time_str, u8State);
	}

	fprintf(log_fp, "%s", acLog);

	fclose(log_fp);
	
	pthread_mutex_unlock(&log_lock);
}
