// openscad_cl.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include<windows.h>
#include<signal.h>

#ifdef _DEBUG
void _trace(char *fmt, ...);
#define ASSERT(x) {if(!(x)) _asm{int 0x03}}
#define VERIFY(x) {if(!(x)) _asm{int 0x03}}
#else
#define ASSERT(x)
#define VERIFY(x) x
#endif
#ifdef _DEBUG
#define TRACE _trace
#else
inline void _trace(LPCTSTR fmt, ...) { }
#define TRACE  1 ? (void)0 : _trace
#endif




#pragma comment(lib,"User32.lib")
void HandleOutput(HANDLE hPipeRead);
DWORD WINAPI RedirThread(LPVOID lpvThreadParam);

HANDLE hChildProcess=NULL;
HANDLE hStdIn=NULL;
BOOL bRunThread=TRUE;

static void terminate (int param)
{
  printf ("\nTerminating openscad\n");
  TerminateProcess(hChildProcess, 1);
}

//int main(int argc,char *argv[]){
int _tmain(int argc, _TCHAR* argv[])
{
    HANDLE hOutputReadTemp,hOutputRead,hOutputWrite;
    HANDLE hInputWriteTemp,hInputRead,hInputWrite;
    HANDLE hErrorWrite;
    HANDLE hThread;
    DWORD ThreadId;
    SECURITY_ATTRIBUTES sa;

    sa.nLength=sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor=NULL;
    sa.bInheritHandle=TRUE;
	remove("openscad.log");

#define ELEMENTS(a) ((sizeof a) / sizeof a [0])
    //DWORD i;
    _TCHAR cmd[1000];
    cmd[0]='\0';
    wcscat_s(cmd, ELEMENTS(cmd), L"openscad.exe");
    for(int i = 1; i < argc; i++){
        wcscat_s(cmd, ELEMENTS(cmd), L" ");
        wcscat_s(cmd, ELEMENTS(cmd), argv[i]);
    }
	wprintf(L"command: %s\n", cmd);

    VERIFY(CreatePipe(&hOutputReadTemp,&hOutputWrite,&sa,0));
    VERIFY(DuplicateHandle(GetCurrentProcess(),hOutputWrite,GetCurrentProcess(),&hErrorWrite,0,TRUE,DUPLICATE_SAME_ACCESS));

    VERIFY(CreatePipe(&hInputRead,&hInputWriteTemp,&sa,0));
    VERIFY(DuplicateHandle(GetCurrentProcess(),hOutputReadTemp,GetCurrentProcess(),&hOutputRead,0,FALSE,DUPLICATE_SAME_ACCESS));
    VERIFY(DuplicateHandle(GetCurrentProcess(),hInputWriteTemp,GetCurrentProcess(),&hInputWrite,0,FALSE,DUPLICATE_SAME_ACCESS));

    CloseHandle(hOutputReadTemp);
    CloseHandle(hInputWriteTemp);

    hStdIn=GetStdHandle(STD_INPUT_HANDLE);

    //-
    PROCESS_INFORMATION pi;
    STARTUPINFO si;

    ZeroMemory(&si,sizeof(STARTUPINFO));
    si.cb=sizeof(STARTUPINFO);
    si.dwFlags=STARTF_USESTDHANDLES;
    si.hStdOutput=hOutputWrite;
    si.hStdInput=hInputRead;
    si.hStdError=hErrorWrite;

    VERIFY(CreateProcess(NULL, cmd, NULL,NULL,TRUE,CREATE_NEW_CONSOLE,NULL,NULL,&si,&pi));

    hChildProcess=pi.hProcess;

    CloseHandle(pi.hThread);
    //-

    CloseHandle(hOutputWrite);
    CloseHandle(hInputRead);
    CloseHandle(hErrorWrite);

    hThread=CreateThread(NULL,0,RedirThread,(LPVOID)hInputWrite,0,&ThreadId);
	VERIFY(hThread != NULL);

	signal(SIGINT,terminate);				// kill the child if we are terminated

    HandleOutput(hOutputRead);

    CloseHandle(hStdIn);

    bRunThread=FALSE;

    WaitForSingleObject(hThread,INFINITE);
    CloseHandle(hOutputRead);
    CloseHandle(hInputWrite);

    return 0;
}

void HandleOutput(HANDLE hPipeRead){
    CHAR lpBuffer[256];
    DWORD nRead = 0;
    DWORD nWrote;

	
    while(TRUE){
        if(!ReadFile(hPipeRead,lpBuffer,sizeof(lpBuffer),&nRead,NULL)||!nRead)
            if(GetLastError()==ERROR_BROKEN_PIPE)
                break;

        WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE),lpBuffer,nRead,&nWrote,NULL);
		FILE *fp;
		if(!fopen_s(&fp, "openscad.log", "ab")) {
			fwrite(lpBuffer, nRead, 1, fp);
			fclose(fp);
		}
		else
			printf("can't open log file: %d\n", errno);
    }
}

DWORD WINAPI RedirThread(LPVOID lpvThreadParam){
    CHAR buff[256];
    DWORD nRead,nWrote;
    HANDLE hPipeWrite=(HANDLE)lpvThreadParam;

    while(bRunThread){
        ReadConsole(hStdIn,buff,1,&nRead,NULL);

        buff[nRead]='\0';

        if(!WriteFile(hPipeWrite,buff,nRead,&nWrote,NULL)){
            if(GetLastError()==ERROR_NO_DATA)
                break;
        }
    }
    return 1;
}

