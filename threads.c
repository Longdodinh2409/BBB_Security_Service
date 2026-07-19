#include "main.h"

static char s_acTXSearchMemberBuffer[LOG_BUFFER_SIZE];
static char s_acRXSearchMemberBuffer[LOG_BUFFER_SIZE];
static uint8_t s_u8State;
static uint8_t s_u8FingerID = NONE_FINGER_PRINT_ID;
static char s_acMemberName[MAX_NAME_LEN];
static char s_acTXTimeDisplayBuffer[64];

static bool bIsSendTimeDisplayBy1Sec = false;

pthread_mutex_t g_time_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_time_cond = PTHREAD_COND_INITIALIZER;

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

				// Step 3: Get ID and return Member Name
				result = get_user_name((s_u8FingerID + OFFSET_MEMBER_ID), s_acMemberName);
				if (result == 1)
				{
					printf("Found! It's %s\n", s_acMemberName);

					// UART send Member Name to STM32F4
					snprintf(s_acTXSearchMemberBuffer, LOG_BUFFER_SIZE, "#State=%hhu,#Name=%s;", s_u8State, s_acMemberName);
					int bytes_written = write(uart_fd, s_acTXSearchMemberBuffer, strlen(s_acTXSearchMemberBuffer));

					if (bytes_written < 0) 
					{
						printf("Loi truyen du lieu qua UART!\n");
					} 
					else 
					{
						printf("Da gui qua UART1: %s\n", s_acTXSearchMemberBuffer);
					}
				}
				else
				{
					if (s_u8State == FSM_NEW_FINGERPRINT_ADDED)
					{
						result = add_new_member((s_u8FingerID), "John");
						if (result == 1)
						{
							result = get_user_name((s_u8FingerID), s_acMemberName);
							printf("Added new member, %s!\n", s_acMemberName);
						}
					}
					else
					{
						printf("Unknown member\n");
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

					pthread_mutex_lock(&g_time_mutex);
    
					if ((s_u8State == FSM_FINGER_BLOCK_5M) || (s_u8State == FSM_FINGER_BLOCK_10M))
					{
						bIsSendTimeDisplayBy1Sec = true;
						// Đánh thức TimeDisplay_thread NGAY LẬP TỨC để bắt đầu gửi mỗi 1 giây
						pthread_cond_signal(&g_time_cond); 
					}
					else if (s_u8State == FSM_FINGER_UNBLOCK)
					{
						bIsSendTimeDisplayBy1Sec = false;
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
