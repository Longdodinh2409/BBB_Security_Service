#include "main.h"
#include <signal.h>

pthread_mutex_t uart_mutex = PTHREAD_MUTEX_INITIALIZER;
bool g_bIsContinueLoop = true;
pthread_t timeDisplay_thread, SearchMember_thread, cli_thread;

int main()
{
	int return_check;

	// Init system
	UART_Init();
	Database_Init(DATABASE_NAME);
	signal(SIGINT, handle_exit);

	// Init thread
	return_check = pthread_create(&timeDisplay_thread, NULL, handle_TimeDisplay_thread, NULL);
	if (return_check != 0)
	{
		printf("Fail in create Time Display thread!\n");
		return -1;
	}

	return_check = pthread_create(&SearchMember_thread, NULL, handle_SearchMember_thread, NULL);
	if (return_check != 0)
	{
		printf("Fail in create Search Member thread!\n");
		return -1;
	}

	return_check = pthread_create(&cli_thread, NULL, handle_CLI_thread, NULL);
	if (return_check != 0)
	{
		printf("Fail in create CLI thread!\n");
		return -1;
	}
	
	// Process end
	pthread_join(timeDisplay_thread, NULL);
	pthread_join(SearchMember_thread, NULL);
	pthread_join(cli_thread, NULL);
	
	printf("Start to release all system resource...\n");
	// Close UART
	UART_End();
	// Close Database
	Database_End();
	
	// Release Mutex, etc....

	printf("Entire process end!\n");

	return 0;
}
