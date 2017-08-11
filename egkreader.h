/*
 *  egkreader.h
 *  4D Plugin
 *
 *  Created by th on 14.10.08.
 *  Copyright 2008 Procedia. All rights reserved.
 *
 */

#pragma once

#define BUFFER_MAX_SIZE 65535

int reader_start(char *exec, char mode);
void reader_stop();
int reader_read(long *pDataLen, unsigned char *pData);
int reader_eject();
int reader_state(long *pDataLen, unsigned char *pData);
int reader_erase();		// mod ab 15.09.2009
