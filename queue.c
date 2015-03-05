#include <windows.h>
#include <stdio.h>

#define BUFSIZE 512

HANDLE ghMutex;

DWORD WINAPI PipeThread(LPVOID);
void MyPipeClient(char *command);

char lpszPipename[128] = "\\\\.\\pipe\\CmdQueue";
char mutexName[128] = "Global\\CmdQueueMutex";
HANDLE hPipe = INVALID_HANDLE_VALUE, hThread = NULL;

#define QUEUE_ELEMENTS 100
#define MAX_COMMAND_LENGTH 1024
#define QUEUE_SIZE (QUEUE_ELEMENTS + 1)

char Queue[QUEUE_SIZE][MAX_COMMAND_LENGTH];

int QueueIn = 0, QueueOut = 0;
int QueuePut(char new[]);
int QueueGet(char old[]);

#define FLAG_LIST 1
#define FLAG_KILL 2


int flags = 0;
int commandStart = 1;

int main(int argc, char **argv) {
	char command[MAX_COMMAND_LENGTH];
	char path[MAX_COMMAND_LENGTH];
	command[0] = 0;
	//strcpy(command, "");
	if (argc > 1) {
		
		for (int i = 1; i < argc; ++i) {
			if (argv[i][0] == '/') {
				if (strcmp(argv[i], "/l") == 0) {
					flags += FLAG_LIST;
				}
				else if (strcmp(argv[i], "/k") == 0) {
					flags += FLAG_KILL;
				}
				else if (strcmp(argv[i], "/p") == 0) {
					i++;
					if (argc <= i) {
						printf("Queue id (any string or number) required\n");
						return 1;
					}
					strcat(mutexName, argv[i]);
					strcat(lpszPipename, argv[i]);
				}
				commandStart = i + 1;
			}
			else {
				break;
			}
		}
		
	    for (int i = commandStart; i < argc; ++i)
		{
			char *hasSpaces = strstr(argv[i], " ");
			if (hasSpaces) {
				strcat(command, "\"");
			}
			strcat(command, argv[i]);
			if (hasSpaces) {
				strcat(command, "\"");
			}
			strcat(command, " ");
		}
	}
	
	_getcwd(path, MAX_COMMAND_LENGTH);
	
	if (flags == 0 && strlen(command) == 0) {
		printf("Usage: %s [switch] [switch2] [command]\n", argv[0]);
		printf("Switches:\n");
		//printf(" -l : list jobs in queue\n"); // TODO implement!
		//printf(" -k : kill running job\n"); // TODO implement!
		printf(" -p <id> : use separate queue identified by id\n");
		return 0;
	}
	
	printf("Command: %s\n", command);
	//return;
	
	
	
	HANDLE hPipeThread = NULL;
	DWORD ThreadID;	
	
	// Create a mutex with no initial owner
	ghMutex = CreateMutex(NULL, FALSE, mutexName);   
	
	
	
	
	
	
	if (ghMutex == NULL)     {
		printf("CreateMutex error: %d\n", GetLastError());
		return 1;
	}
	else {
		if( GetLastError() == ERROR_ALREADY_EXISTS ){
			// already running, push to pipe
			printf("Queue is running\n");
			
			MyPipeClient(command);
			MyPipeClient(path);
			
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
			
			QueuePut(command);
			QueuePut(path);
			
			while (0 == QueueGet(command) && 0 == QueueGet(path)) {
				chdir(path);
				printf("Running %s...\n", command);
				system(command);
			}
			
			//sleep(50000);
			
			CloseHandle(hPipeThread);
			CloseHandle(ghMutex);
			
			printf("After pipe\n");
		}
	}
	
	return 0;
}

void MyPipeClient(char *command) {
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
	data[numRead] = 0;
	
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
		
		char data[MAX_COMMAND_LENGTH];
		
		DWORD numRead;
		DWORD numWritten;
		
		ConnectNamedPipe(pipe, NULL);
		
		ReadFile(pipe, data, MAX_COMMAND_LENGTH, &numRead, NULL);
	data[numRead] = 0;
	
	if (numRead > 0) {
	printf("%d: %s\n", numRead, data);
	}
	
	QueuePut(data);
	
	printf("in: %d, out: %d\n", QueueIn, QueueOut);
	
	
	WriteFile(pipe, "OK", strlen("OK"), &numWritten, NULL); 
	
	CloseHandle(pipe);
	}
	
	
	printf("Pipe closed\n");
	return TRUE;
	}
	
	
	
	// ---------------------------------- QUEUE STORAGE
	
	
	
	int QueuePut(char new[])
	{
	if(QueueIn == (( QueueOut - 1 + QUEUE_SIZE) % QUEUE_SIZE))
	{
	return -1; // Queue Full
	}
	
	strcpy(Queue[QueueIn], new);
	
	QueueIn = (QueueIn + 1) % QUEUE_SIZE;
	
	return 0; // No errors
	}
	
	int QueueGet(char old[])
	{
	if(QueueIn == QueueOut)
	{
	return -1; // Queue Empty - nothing to get
	}
	
	strcpy(old, Queue[QueueOut]);
	
	QueueOut = (QueueOut + 1) % QUEUE_SIZE;
	
	return 0; // No errors
	}				