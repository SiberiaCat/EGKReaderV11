/*
 *  egkreader.c
 *  4D Plugin
 *
 *  Created by th on 14.10.08.
 *  Copyright 2008 Procedia. All rights reserved.
 *
 */

#include "4DPluginAPI.h"

#if VERSIONWIN

#include <string.h>
#include <process.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "egkreader.h"
#include "pasprintf.h"

unsigned char readbuffer[32000];

// Pipe Handles for eGKReader
HANDLE g_hReader_IN_Rd = NULL;
HANDLE g_hReader_IN_Wr = NULL;
HANDLE g_hReader_OUT_Rd = NULL;
HANDLE g_hReader_OUT_Wr = NULL;

HANDLE pid_child = NULL;


void myYield()
{
	//PA_Yield();
	//Sleep(1);
}

int checkChildRunnig()
{
	if(pid_child == NULL)
		goto NOT_FOUND;
	else 
	{
//		int state = 0;
//	//	waitpid(pid_child, &state, WNOHANG);
//	//	if (kill(pid_child, 0) == -1) {
//	//		pid_child = -1;
//		/*	goto NOT_FOUND;
//		}*/
		// process is active
		DWORD exitcode = 0;
		GetExitCodeProcess(pid_child, &exitcode);
		if (exitcode == STILL_ACTIVE) {
			return 0;
		}
		else {
			CloseHandle(pid_child);
			pid_child = 0;
			goto NOT_FOUND;
		}
	}
	
NOT_FOUND:
	// process not found
	if ((g_hReader_IN_Rd != NULL) && (g_hReader_IN_Wr != NULL))
	{
		CloseHandle(g_hReader_IN_Rd);
		CloseHandle(g_hReader_IN_Wr);
		g_hReader_IN_Rd = 0;
		g_hReader_IN_Wr = 0;
	}
	if((g_hReader_OUT_Rd != NULL) && (g_hReader_OUT_Wr != NULL))
	{
		CloseHandle(g_hReader_OUT_Rd);
		CloseHandle(g_hReader_OUT_Wr);
		g_hReader_OUT_Rd = 0;
		g_hReader_OUT_Wr = 0;
	}	
	return -1;
}


// --- add2str
void add2str ( char* *str, char c )
{
	char *old = *str;
	char *ptr; 

	if (old != 0) {
		//ptr = (char*)malloc(strlen(*str)*sizeof(char)+2);
		pasprintf(&ptr, "%s%c", old, c);
		*str = ptr;
		free(old);
	}
	else {		
		//ptr = (char*)malloc(sizeof(char)+1);
		pasprintf(&ptr, "%c", c);	
		*str = ptr;
	}
}


int reader_start(char *exec, char mode)
{
	// active child found
	if (checkChildRunnig() == 0) 
	{
		// kill old process
		TerminateProcess(pid_child,0);
	}

	//Init
	g_hReader_IN_Rd = NULL;
	g_hReader_IN_Wr = NULL;
	g_hReader_OUT_Rd = NULL;
	g_hReader_OUT_Wr = NULL;

	// open pipes
	SECURITY_ATTRIBUTES saAttributes;
	saAttributes.bInheritHandle = true;
	saAttributes.lpSecurityDescriptor = 0;
	saAttributes.nLength = sizeof(SECURITY_ATTRIBUTES); 

	if ( ! CreatePipe(&g_hReader_IN_Rd, &g_hReader_IN_Wr, &saAttributes, 0) ) 
		return -1;
	//if ( ! SetHandleInformation(g_hReader_IN_Rd, HANDLE_FLAG_INHERIT, 0))
	if ( ! SetHandleInformation(g_hReader_IN_Wr, HANDLE_FLAG_INHERIT, 0))
		return -1;
	if ( ! CreatePipe(&g_hReader_OUT_Rd, &g_hReader_OUT_Wr, &saAttributes, 0) ) 
		return -1;
	//if ( ! SetHandleInformation(g_hReader_OUT_Wr, HANDLE_FLAG_INHERIT, 0))
	if ( ! SetHandleInformation(g_hReader_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
		return -1;
	
	// create child
	PROCESS_INFORMATION piProcInfo; 
	STARTUPINFO siStartInfo;

	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
	siStartInfo.cb = sizeof(STARTUPINFO); 
	siStartInfo.hStdError = g_hReader_OUT_Wr;
    siStartInfo.hStdOutput = g_hReader_OUT_Wr;
    siStartInfo.hStdInput = g_hReader_IN_Rd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	char *cmdline = 0;
	pasprintf(&cmdline, "%s l=%c", exec, mode);

	if(CreateProcess(NULL,cmdline,NULL,NULL,TRUE,0,NULL,NULL,&siStartInfo,&piProcInfo)) 
	{
		pid_child = piProcInfo.hProcess;
		//CloseHandle(piProcInfo.hProcess);
		//CloseHandle(piProcInfo.hThread);
		DWORD dwWritten; 
		CHAR cmd[] = "<open/>";
		if(!WriteFile(g_hReader_IN_Wr, cmd, sizeof(cmd), &dwWritten, NULL))
			return -1;	
		return 0;
	}
	return -1;
}


void reader_stop()
{
	checkChildRunnig();
	if(g_hReader_IN_Wr != NULL)
	{
		DWORD dwWritten; 
		CHAR close[] = "<close/>";
		WriteFile(g_hReader_IN_Wr, close, sizeof(close), &dwWritten, NULL);

		CHAR exit[] = "<exit/>";
		WriteFile(g_hReader_IN_Wr, exit, sizeof(exit), &dwWritten, NULL);

		CloseHandle(g_hReader_IN_Rd);
		CloseHandle(g_hReader_IN_Wr);
		CloseHandle(g_hReader_OUT_Rd);
		CloseHandle(g_hReader_OUT_Wr);

		g_hReader_IN_Rd = 0;
		g_hReader_IN_Wr = 0;
		g_hReader_OUT_Rd = 0;
		g_hReader_OUT_Wr = 0;
	}

}

int reader_read(long *pDataLen, unsigned char *pData)
{
	int err = 0;
	checkChildRunnig();
	if ((g_hReader_IN_Rd != NULL)&&(g_hReader_OUT_Rd != NULL))
	{
		DWORD dwWritten; 
		CHAR command[] = "<read/>";
		WriteFile(g_hReader_IN_Wr, command, sizeof(command), &dwWritten, NULL);

		int bufptr = 0;
		int lasttagptr = 0;
		int mode = 0;
		char *cmd = 0;
		int c;
		int exit = 0;
		int kvk = 0;

		myYield();

		DWORD dwRead; 
		CHAR chBuf[2];
		do {
			ReadFile( g_hReader_OUT_Rd, chBuf, 1, &dwRead, NULL);
			c = chBuf[0];
			//c = fgetc(fread_reader);
			switch (c) 
			{
				case EOF:
					if (checkChildRunnig() == -1)
					{
						*pDataLen = 0;
						return -2;
					}
					myYield();
					break;
				case 0x0d:
				case 0x0a:
					// ignore chars	
					if(kvk != 0)
					{
						add2str(&cmd,c);
						readbuffer[bufptr] = c;
						bufptr++;
					}
					break;		
				case '<':
					// open tag
					free(cmd);
					cmd = 0;
					add2str( &cmd, c);
					readbuffer[bufptr] = c;
					lasttagptr = bufptr;
					bufptr++;
					break;
					
				case '>':
					// close tag
					add2str( &cmd, c);
					readbuffer[bufptr] = c;
					bufptr++;
					if (strcmp(cmd, "<egkreader_daten>") == 0) {
						bufptr = 0;
						mode = 1;
					}
					else if (strcmp(cmd, "<egkreader_status>") == 0) {
						bufptr = 0;
						mode = 2;
					}
					else if (strcmp(cmd, "</egkreader_daten>") == 0) {
						memcpy(pData, readbuffer, lasttagptr);
						(*pDataLen) = lasttagptr;
						exit = 1;
//						// Datendump erstellen
//						FILE *fp;
//						int j = 0;
//						fp = fopen( "egkreader_daten.xml", "w");
//						for (j = 0; j < lasttagptr; j++)
//						{
//							int c = pData[j];
//							fputc(c, fp);
//						}
//						fclose(fp);
					}
					else if (strcmp(cmd, "</egkreader_status>") == 0) {
						memcpy(pData, readbuffer, lasttagptr);
						(*pDataLen) = lasttagptr;
						exit = 1;						
//						// Datendump erstellen
//						FILE *fp;
//						int j = 0;
//						fp = fopen( "egkreader_status.xml", "w");
//						for (j = 0; j < lasttagptr; j++)
//						{
//							int c = pData[j];
//							fputc(c, fp);
//						}
//						fclose(fp);
					}
					else if (strcmp(cmd, "<kvkdaten>") == 0){
						kvk = 1;
					}
					else if (strcmp(cmd, "</kvkdaten>") == 0){
						kvk = 0;
					}
					break;
				default:
					// read cmd
					add2str( &cmd, c);
					readbuffer[bufptr] = c;
					bufptr++;
					break;
			}
			myYield();
		} while (exit == 0);
	}
	else
		err = -2;
	return err;
}


int reader_eject()
{
	int err = 0;
	checkChildRunnig();
	//if ((fread_reader != 0) && (fwrite_reader != 0))
	//{
	//	fprintf(fwrite_reader, "<eject/>");
	//	fflush(fwrite_reader);
	//}
	//else
	//	err = -2;
	if(g_hReader_IN_Wr != NULL)
	{
		DWORD dwWritten; 
		CHAR command[] = "<eject/>";
		WriteFile(g_hReader_IN_Wr, command, sizeof(command), &dwWritten, NULL);
	}
	else err = -2;

	return err;
}

/* mod ab 16.09.2009 Begin */
int reader_erase()
{
	int err = 0;
	checkChildRunnig();

	if(g_hReader_IN_Wr != NULL)
	{
		DWORD dwWritten; 
		CHAR command[] = "<erase/>";
		WriteFile(g_hReader_IN_Wr, command, sizeof(command), &dwWritten, NULL);
	}
	else err = -2;

	return err;
}
/* mod ab 16.09.2009 End */


int reader_state(long *pDataLen, unsigned char *pData)
{
	int err = 0;
	checkChildRunnig();
	if ((g_hReader_IN_Wr != NULL)&&(g_hReader_OUT_Rd != NULL))
	{
		DWORD dwWritten; 
		CHAR cmdRead[] = "<state/>";
		WriteFile(g_hReader_IN_Wr, cmdRead, sizeof(cmdRead), &dwWritten, NULL);

		int bufptr = 0;
		int lasttagptr = 0;
		int mode = 0;
		char *cmd = 0;
		int c;
		int exit = 0;
		
		myYield();

		DWORD dwRead;
		CHAR chBuf[2];
		do {
			ReadFile( g_hReader_OUT_Rd, chBuf, 1, &dwRead, NULL);
			c = chBuf[0];
			//c = fgetc(fread_reader);
			switch (c) 
			{
				case EOF:
					if (checkChildRunnig() == -1)
					{
						*pDataLen = 0;
						return -2;
					}
				case 0x0d:
				case 0x0a:
					// ignore chars
					break;
					
				case '<':
					// open tag
					free(cmd);
					cmd = 0;
					add2str( &cmd, c);
					readbuffer[bufptr] = c;
					lasttagptr = bufptr;
					bufptr++;
					break;
					
				case '>':
					// close tag
					add2str( &cmd, c);
					readbuffer[bufptr] = c;
					bufptr++;
					if (strcmp(cmd, "<egkreader_status>") == 0) {
						bufptr = 0;
						mode = 2;
					}
					else if (strcmp(cmd, "</egkreader_status>") == 0) {
						memcpy(pData, readbuffer, lasttagptr);
						(*pDataLen) = lasttagptr;
						exit = 1;						
					}
					break;
				default:
					// read cmd
					add2str( &cmd, c);
					readbuffer[bufptr] = c;
					bufptr++;
					break;
			}
			myYield();
		} while (exit == 0);
	}
	else
		err = -2;
	return err;
}
#endif