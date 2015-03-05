#include <windows.h>
#include <stdio.h>

#define THREADCOUNT 1
#define BUFSIZE 512

HANDLE ghMutex;

DWORD WINAPI InstanceThread(LPVOID);
DWORD WINAPI PipeThread(LPVOID);
VOID GetAnswerToRequest(LPTSTR, LPTSTR, LPDWORD);


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

BOOL   fConnected = FALSE;
DWORD  dwThreadId = 0;
LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\CmdQueue");
HANDLE hPipe = INVALID_HANDLE_VALUE, hThread = NULL;

int main( void ) {
	DWORD ThreadID;	
	
	// Create a mutex with no initial owner
	ghMutex = CreateMutex(
	NULL,              // default security attributes
	FALSE,             // initially not owned
	"Global\\CmdQueueMutex");             // unnamed mutex
	
	if (ghMutex == NULL)     {
		printf("CreateMutex error: %d\n", GetLastError());
		return 1;
	}
	else {
		if( GetLastError() == ERROR_ALREADY_EXISTS ){
			// already running, push to pipe
			printf("Already running");
			CloseHandle( ghMutex );
			return 1;
		}
		else {
			// create named pipe in thread
			//PipeThread(NULL);
			
			hThread = CreateThread(
			NULL,       // default security attributes
			0,          // default stack size
			(LPTHREAD_START_ROUTINE) PipeThread,
			NULL,       // no thread function arguments
			0,          // default creation flags
			&ThreadID); // receive thread identifier
			
			if( hPipe == NULL )        {
				printf("CreateThread error: %d\n", GetLastError());
				return 1;
			}
			
			printf("Sleeping...\n");
			sleep(5000);
			
			CloseHandle(hThread);
			CloseHandle(ghMutex);
			
			printf("After pipe\n");
		}
	}
	
	return 0;
}


DWORD WINAPI PipeThread(LPVOID lpParam){
	UNREFERENCED_PARAMETER(lpParam);
		
	printf("Hello!");
	//	return TRUE;
	
	for (;;)
	{
		printf( TEXT("\nPipe Server: Main thread awaiting client connection on %s\n"), lpszPipename);
		hPipe = CreateNamedPipe(
		lpszPipename,             // pipe name
		PIPE_ACCESS_DUPLEX,       // read/write access
		PIPE_TYPE_MESSAGE |       // message type pipe
		PIPE_READMODE_MESSAGE |   // message-read mode
		PIPE_WAIT,                // blocking mode
		PIPE_UNLIMITED_INSTANCES, // max. instances
		BUFSIZE,                  // output buffer size
		BUFSIZE,                  // input buffer size
		0,                        // client time-out
		NULL);                    // default security attribute
		
		if (hPipe == INVALID_HANDLE_VALUE)       {
			printf(TEXT("CreateNamedPipe failed, GLE=%d.\n"), GetLastError());
			return -1;
		}
		
		// Wait for the client to connect; if it succeeds,
		// the function returns a nonzero value. If the function
		// returns zero, GetLastError returns ERROR_PIPE_CONNECTED.
		
		fConnected = ConnectNamedPipe(hPipe, NULL) ?
		TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
		
		if (fConnected)       {
			printf("Client connected, creating a processing thread.\n");
			
			// Create a thread for this client.
			hThread = CreateThread(
			NULL,              // no security attribute
			0,                 // default stack size
			InstanceThread,    // thread proc
			(LPVOID) hPipe,    // thread parameter
			0,                 // not suspended
			&dwThreadId);      // returns thread ID
			
			if (hThread == NULL)          {
				printf(TEXT("CreateThread failed, GLE=%d.\n"), GetLastError());
				return -1;
			}
			else CloseHandle(hThread);
		}
		else
		// The client could not connect, so close the pipe.
		CloseHandle(hPipe);
	}
	
	printf("Pipe closed\n");
	return TRUE;
}


DWORD WINAPI InstanceThread(LPVOID lpvParam)// This routine is a thread processing function to read from and reply to a client
// via the open pipe connection passed from the main loop. Note this allows
// the main loop to continue executing, potentially creating more threads of
// of this procedure to run concurrently, depending on the number of incoming
// client connections.
{
	HANDLE hHeap      = GetProcessHeap();
	TCHAR* pchRequest = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE*sizeof(TCHAR));
	TCHAR* pchReply   = (TCHAR*)HeapAlloc(hHeap, 0, BUFSIZE*sizeof(TCHAR));
	
	DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0;
	BOOL fSuccess = FALSE;
	HANDLE hPipe  = NULL;

// Do some extra error checking since the app will keep running even if this
// thread fails.

if (lpvParam == NULL)   {
printf( "\nERROR - Pipe Server Failure:\n");
printf( "   InstanceThread got an unexpected NULL value in lpvParam.\n");
printf( "   InstanceThread exitting.\n");
if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
return (DWORD)-1;
}

if (pchRequest == NULL)   {
printf( "\nERROR - Pipe Server Failure:\n");
printf( "   InstanceThread got an unexpected NULL heap allocation.\n");
printf( "   InstanceThread exitting.\n");
if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
return (DWORD)-1;
}

if (pchReply == NULL)   {
printf( "\nERROR - Pipe Server Failure:\n");
printf( "   InstanceThread got an unexpected NULL heap allocation.\n");
printf( "   InstanceThread exitting.\n");
if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
return (DWORD)-1;
}

// Print verbose messages. In production code, this should be for debugging only.
printf("InstanceThread created, receiving and processing messages.\n");

// The thread's parameter is a handle to a pipe object instance.

hPipe = (HANDLE) lpvParam;

// Loop until done reading
while (1)    {
// Read client requests from the pipe. This simplistic code only allows messages
// up to BUFSIZE characters in length.
fSuccess = ReadFile(
hPipe,        // handle to pipe
pchRequest,    // buffer to receive data
BUFSIZE*sizeof(TCHAR), // size of buffer
&cbBytesRead, // number of bytes read
NULL);        // not overlapped I/O

if (!fSuccess || cbBytesRead == 0)      {
if (GetLastError() == ERROR_BROKEN_PIPE)          {
printf(TEXT("InstanceThread: client disconnected.\n"), GetLastError());
}
else
{
printf(TEXT("InstanceThread ReadFile failed, GLE=%d.\n"), GetLastError());
}
break;
}

// Process the incoming message.
GetAnswerToRequest(pchRequest, pchReply, &cbReplyBytes);

// Write the reply to the pipe.
fSuccess = WriteFile(
hPipe,        // handle to pipe
pchReply,     // buffer to write from
cbReplyBytes, // number of bytes to write
&cbWritten,   // number of bytes written
NULL);        // not overlapped I/O

if (!fSuccess || cbReplyBytes != cbWritten)      {
printf(TEXT("InstanceThread WriteFile failed, GLE=%d.\n"), GetLastError());
break;
}
}

// Flush the pipe to allow the client to read the pipe's contents
// before disconnecting. Then disconnect the pipe, and close the
// handle to this pipe instance.

FlushFileBuffers(hPipe);
DisconnectNamedPipe(hPipe);
CloseHandle(hPipe);

HeapFree(hHeap, 0, pchRequest);
HeapFree(hHeap, 0, pchReply);

printf("InstanceThread exitting.\n");
return 1;
}


VOID GetAnswerToRequest( LPTSTR pchRequest,
LPTSTR pchReply,
LPDWORD pchBytes )// This routine is a simple function to print the client request to the console
// and populate the reply buffer with a default data string. This is where you
// would put the actual client request processing code that runs in the context
// of an instance thread. Keep in mind the main thread will continue to wait for
// and receive other client connections while the instance thread is working.
{
printf( TEXT("Client Request String:\"%s\"\n"), pchRequest );

// Check the outgoing message to make sure it's not too long for the buffer.
/*
if (FAILED(StringCchCopy( pchReply, BUFSIZE, TEXT("default answer from server") )))
{
*pchBytes = 0;
pchReply[0] = 0;
printf("StringCchCopy failed, no outgoing message.\n");
return;
}
*/
*pchBytes = (lstrlen(pchReply)+1)*sizeof(TCHAR);
}



