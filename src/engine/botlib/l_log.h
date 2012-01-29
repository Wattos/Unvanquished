/*
===========================================================================

OpenWolf GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the OpenWolf GPL Source Code (OpenWolf Source Code).  

OpenWolf Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

OpenWolf Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenWolf Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the OpenWolf Source Code is also subject to certain additional terms. 
You should have received a copy of these additional terms immediately following the 
terms and conditions of the GNU General Public License which accompanied the OpenWolf 
Source Code.  If not, please request a copy in writing from id Software at the address 
below.

If you have questions concerning this license or the applicable additional terms, you 
may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, 
Maryland 20850 USA.

===========================================================================
*/


/*****************************************************************************
 * name:		l_log.h
 *
 * desc:		log file
 *
 *
 *****************************************************************************/

//open a log file
void            Log_Open(char *filename);

//
void            Log_AlwaysOpen(char *filename);

//close the current log file
void            Log_Close(void);

//close log file if present
void            Log_Shutdown(void);

//write to the current opened log file
void QDECL      Log_Write(char *fmt, ...);

//write to the current opened log file with a time stamp
void QDECL      Log_WriteTimeStamped(char *fmt, ...);

//returns a pointer to the log file
FILE           *Log_FilePointer(void);

//flush log file
void            Log_Flush(void);