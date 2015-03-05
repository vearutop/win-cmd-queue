#include <windows.h>
#include <stdio.h>

#define THREADCOUNT 1
#define BUFSIZE 512

HANDLE ghMutex;

DWORD WINAPI InstanceThread(LPVOID);
DWORD WINAPI PipeThread(LPVOID);
VOID GetAnswerToRequest(LPTSTR, LPTSTR, LPDWORD);
void MyPipeClient(char command[]);




BOOL   fConnected = FALSE;
DWORD  dwThreadId = 0;
LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\CmdQueue");
HANDLE hPipe = INVALID_HANDLE_VALUE, hThread = NULL;

int main(int argc, char **argv) {
	HANDLE hPipeThread = NULL;
	DWORD ThreadID;	
	
	// Create a mutex with no initial owner
	ghMutex = CreateMutex(
	NULL,              // default security attributes
	FALSE,             // initially not owned
	"Global\\CmdQueueMutex");             // unnamed mutex
	
	
	char command[1024];
	strcpy(command, "");
	
	if (argc > 1) {
	    for (int i = 1; i < argc; ++i)
		{
			strcat(command, argv[i]);
			strcat(command, " ");
		}
	}
	
	//printf(command);
	
	
	if (ghMutex == NULL)     {
		printf("CreateMutex error: %d\n", GetLastError());
		return 1;
	}
	else {
		if( GetLastError() == ERROR_ALREADY_EXISTS ){
			// already running, push to pipe
			printf("Queue is running");
			MyPipeClient(command);
			
			CloseHandle( ghMutex );
			return 1;
		}
		else {
			// create named pipe in thread
			
			hPipeThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) PipeThread, NULL, 0, &ThreadID); 
			
			if( hPipeThread == NULL )        {
				printf("CreateThread error: %d\n", GetLastError());
				return 1;
			}
			
			printf("Running %s...\n", command);
			system(command);
			//sleep(50000);
			
			CloseHandle(hPipeThread);
			CloseHandle(ghMutex);
			
			printf("After pipe\n");
		}
	}
	
	return 0;
}

void MyPipeClient(char command[]) {
	HANDLE pipe = CreateFile(lpszPipename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	
	if (pipe == INVALID_HANDLE_VALUE)
	{
		printf("Error: %s", GetLastError());
	}
	
	DWORD numWritten;
	WriteFile(pipe, command, strlen(command), &numWritten, NULL); 
	
	char data[1024];
	DWORD numRead;
	ReadFile(pipe, data, 1024, &numRead, NULL);
	if (numRead > 0) {
		printf("%s\n", data);
	}
}


DWORD WINAPI PipeThread(LPVOID lpParam){
	UNREFERENCED_PARAMETER(lpParam);
		
	printf("Creating pipe\n");
	
	while (TRUE)
	{
		HANDLE pipe = CreateNamedPipe(lpszPipename, PIPE_ACCESS_INBOUND | PIPE_ACCESS_OUTBOUND , PIPE_WAIT, 1, 1024, 1024, 120 * 1000, NULL);
	

		if (pipe == INVALID_HANDLE_VALUE)
		{
			printf("Error: %s", GetLastError());
		}
		
		char data[1024];
		DWORD numRead;
		DWORD numWritten;
		
		ConnectNamedPipe(pipe, NULL);
		
		ReadFile(pipe, data, 1024, &numRead, NULL);
		
		if (numRead > 0) {
			printf("%s\n", data);
		}
		
		
		WriteFile(pipe, "OK", strlen("OK"), &numWritten, NULL); 
		
		CloseHandle(pipe);
	}
	
	
	printf("Pipe closed\n");
	return TRUE;
}



// ---------------------------------- QUEUE STORAGE
#define QUEUE_ELEMENTS 1000
#define QUEUE_SIZE (QUEUE_ELEMENTS + 1)
LPTSTR Queue[QUEUE_SIZE];
int QueueIn = 0, QueueOut = 0;


int QueuePut(LPTSTR new){
	if(QueueIn == (( QueueOut - 1 + QUEUE_SIZE) % QUEUE_SIZE))    {
		return -1; /* Queue Full*/
	}
	
	Queue[QueueIn] = new;
	
	QueueIn = (QueueIn + 1) % QUEUE_SIZE;
	
	return 0; // No errors
}

int QueueGet(LPTSTR *old){
	if(QueueIn == QueueOut)    {
		return -1; /* Queue Empty - nothing to get*/
	}
	
	*old = Queue[QueueOut];
	
	QueueOut = (QueueOut + 1) % QUEUE_SIZE;
	
	return 0; // No errors
}


