#include "main.h"

static char s_acTXSearchMemberBuffer[LOG_BUFFER_SIZE];
static char s_acRXSearchMemberBuffer[LOG_BUFFER_SIZE];
static uint8_t s_u8State;
static uint8_t s_u8FingerID = NONE_FINGER_PRINT_ID;
static char s_acMemberName[MAX_NAME_LEN];
static char s_acTXTimeDisplayBuffer[64];

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
        snprintf(s_acTXTimeDisplayBuffer, sizeof(s_acTXTimeDisplayBuffer), "#TS=%ld\n", raw_time);

        // Gửi chuỗi này qua UART
        int bytes_written = write(uart_fd, s_acTXTimeDisplayBuffer, strlen(s_acTXTimeDisplayBuffer));
        
        if (bytes_written < 0) {
            printf("Loi truyen du lieu qua UART!\n");
        } else {
            printf("Da gui qua UART1: %s", s_acTXTimeDisplayBuffer);
        }

        sleep(60); // Đợi 60 giây trước khi gửi tiếp
    }

	return NULL;
}

void* handle_SearchMember_thread(void *arg)
{
	// unused argument
	(void)arg;

	while(g_bIsContinueLoop)
	{
		// Step 1: Wait RX data
		int rx_length = read(uart_fd, (void *)s_acRXSearchMemberBuffer, LOG_BUFFER_SIZE);
		if (rx_length > 0)
		{
			s_acRXSearchMemberBuffer[rx_length] = '\0';

			// Step 2: Parsing frame: #State=x,#ID=y
			int parsed_count = sscanf(s_acRXSearchMemberBuffer, "#State=%hhu,#ID=%hhu", &s_u8State, &s_u8FingerID);
			// hhu : half-half unsigned = uint8_t (1 byte)

			if (parsed_count == 2)
			{
				printf("Parsing success. State: %d - ID: %d\n", s_u8State, s_u8FingerID);

				// Step 3: Get ID and return Member Name
				int result = get_user_name(s_u8FingerID, s_acMemberName);
				if (result == 1)
				{
					printf("Found! It's %s", s_acMemberName);
					// UART send Member Name to STM32F4
					snprintf(s_acTXSearchMemberBuffer, LOG_BUFFER_SIZE, "#ID=%hhu,#Name=%s", s_u8FingerID, s_acMemberName);

					int bytes_written = write(uart_fd, s_acTXSearchMemberBuffer, strlen(s_acTXSearchMemberBuffer));

					if (bytes_written < 0) 
					{
						printf("Loi truyen du lieu qua UART!\n");
					} 
					else 
					{
						printf("Da gui qua UART1: %s\n", s_acTXTimeDisplayBuffer);
					}
				}
				else
				{
					printf("Unknown member\n");
				}
			}
			else
			{
				printf("Parsing failed. Wait another frame!\n");
				continue;
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
