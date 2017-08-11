/*
 *  egkreader.c
 *  4D Plugin
 *
 *  Created by th on 14.10.08.
 *  Copyright 2008 Procedia. All rights reserved.
 *
 */



#include "4DPluginAPI.h"

#if VERSIONMAC

#include <string.h>
#include <unistd.h>
#include "egkreader.h"

unsigned char readbuffer[32000];

// Pipe Handles for eGKReader
FILE *fread_reader = 0, *fwrite_reader = 0;
pid_t pid_child = -1;

void myYield()
{
//	PA_Yield();
	usleep(10);
}

int checkChildRunnig()
{
	if (pid_child == -1)
		goto NOT_FOUND;
	else {
		int state = 0;
		waitpid(pid_child, &state, WNOHANG);
		if (kill(pid_child, 0) == -1) {
			pid_child = -1;
			goto NOT_FOUND;
		}
		// process is active
		return 0;
	}
	
NOT_FOUND:
	// process not found
	if ((fread_reader != 0) && (fwrite_reader != 0))
	{
		fclose(fwrite_reader);
		fclose(fread_reader);
		fread_reader = 0;
		fwrite_reader = 0;
	}
	return -1;
}

void add2str ( char* *str, char c )
{
	char *old = *str;
	
	if (old != 0) {
		asprintf(str, "%s%c", old, c);
		free (old);
	}
	else {
		asprintf(str, "%c", c);
	}
	
}

int reader_start(char *exec, char mode)
{
	int fd_reader_in[2], fd_reader_out[2];

	// active child found
	if (checkChildRunnig() == 0) {
		// kill old process
		kill(pid_child, 15);
		int state = 0;
		waitpid(pid_child, &state, 0);
	}
	
	// Init
	fread_reader = 0;
	fwrite_reader = 0;

	// open pipes
	if (pipe(fd_reader_in) == -1)
		goto ERR1;
	if (pipe(fd_reader_out) == -1)
		goto ERR2;
	
	// create child
	if ((pid_child = fork()) == -1)
		goto ERR3;
	
	if (pid_child > 0) {
		// parent process
		// set file handles
		if ((fread_reader = fdopen (fd_reader_out[0], "r")) == 0)
			goto ERR3;
		close(fd_reader_out[1]);
		if ((fwrite_reader = fdopen (fd_reader_in[1], "w")) == 0)
			goto ERR2;
		close(fd_reader_in[0]);
		fcntl(fileno(fread_reader), F_SETFL, O_NONBLOCK);
		// send open command
		fprintf(fwrite_reader, "<open/>");
		fflush(fwrite_reader);
		// successful
		return 0;
	}
	else {
		// child process
		sleep (1);
		// close unused file handles
		close (fd_reader_out[0]);
		close (fd_reader_in[1]);
		// redirect stdio
		if (fd_reader_in[0] != STDIN_FILENO) {
			if (dup2 (fd_reader_in[0], STDIN_FILENO) != STDIN_FILENO)
				exit(-2);
			close (fd_reader_in[0]);
		}
		if (fd_reader_out[1] != STDOUT_FILENO) {
			if (dup2 (fd_reader_out[1], STDOUT_FILENO) != STDOUT_FILENO)
				exit(-2);
			close (fd_reader_out[1]);
		}
		// start egkreader
		char arg1[] = "l=0";
		if (mode != '0')
			arg1[2] = mode;
		if (execl (exec, "EGKReader", arg1, NULL) == -1) ;
		exit(-2);
	}
	return 0;
	
	// error handling
ERR3:
	close(fd_reader_out[0]);	
	close(fd_reader_out[1]);			
ERR2:
	close(fd_reader_in[0]);	
	close(fd_reader_in[1]);		
ERR1:
	return -1;
}


void reader_stop()
{
	checkChildRunnig();
	if ((fread_reader != 0) && (fwrite_reader != 0))
	{
		fprintf(fwrite_reader, "<close/>");
		fflush(fwrite_reader);
		fprintf(fwrite_reader, "<exit/>");
		fflush(fwrite_reader);
		fclose(fwrite_reader);
		fclose(fread_reader);
	}
}

int reader_read(long *pDataLen, unsigned char *pData)
{
	int err = 0;
	checkChildRunnig();
	if ((fread_reader != 0) && (fwrite_reader != 0))
	{
		fprintf(fwrite_reader, "<read/>");
		fflush(fwrite_reader);
		int bufptr = 0;
		int lasttagptr = 0;
		int mode = 0;
		char *cmd = 0;
		int c;
		int exit = 0;
		int kvk = 0;
		
		myYield();
		
		do {
			c = fgetc(fread_reader);
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
					if (kvk != 0)
					{
						add2str( &cmd, c);
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
					else if (strcmp(cmd, "<kvkdaten>") == 0) {
						kvk = 1;
					}
					else if (strcmp(cmd, "</kvkdaten>") == 0) {
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
	if ((fread_reader != 0) && (fwrite_reader != 0))
	{
		fprintf(fwrite_reader, "<eject/>");
		fflush(fwrite_reader);
	}
	else
		err = -2;
	return err;
}

int reader_state(long *pDataLen, unsigned char *pData)
{
	int err = 0;
	checkChildRunnig();
	if ((fread_reader != 0) && (fwrite_reader != 0))
	{
		fprintf(fwrite_reader, "<state/>");
		fflush(fwrite_reader);
		int bufptr = 0;
		int lasttagptr = 0;
		int mode = 0;
		char *cmd = 0;
		int c;
		int exit = 0;
		
		myYield();
		
		do {
			c = fgetc(fread_reader);
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


/* mod ab 15.09.2009 */
int reader_erase()
{
	int err = 0;
	checkChildRunnig();
	if ((fread_reader != 0) && (fwrite_reader != 0))
	{
		fprintf(fwrite_reader, "<erase/>");
		fflush(fwrite_reader);
	}
	else
		err = -2;
	return err;
}


#endif