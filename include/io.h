#ifndef __IO_H__
#define __IO_H__

#include <arcos.h>
#include <ob.h>

#define FIFO_SIZE	16

// Used by IoInterruptHandler to store chars in buffer. Fixed size.
typedef struct _FIFO 
{
	CHAR buffer[FIFO_SIZE];
	ULONG length;
} FIFO;

POBJECT_TYPE    serialType;
POBJECT_TYPE    ledType;

// Definitions of pointers to methods WriteFile and readFile, used in struct below.
typedef VOID (*IOWRITEFILE_METHOD)(PVOID buffer, ULONG bufferSize);
typedef VOID (*IOREADFILE_METHOD)(PVOID buffer, ULONG bufferSize);

typedef struct _FILE {
	IOWRITEFILE_METHOD write;
	IOREADFILE_METHOD read;
} FILE, *PFILE;

// Node to waiting queue for writeFile.
typedef struct _IO_WAITING_NODE {
	PPROCESS pprocess;
	PCHAR pbuffer;		// Maximum of stuff to write = 512 bytes
	ULONG bufferSize;
	struct _IO_WAITING_NODE *next;
} IO_WAITING_NODE, *PIO_WAITING_NODE;

// Waiting queue for writeFile (FIFO).
typedef struct _IO_WAITING_QUEUE {
	PIO_WAITING_NODE first;
	PIO_WAITING_NODE last;
} IO_WAITING_QUEUE, *PIO_WAITING_QUEUE;


// Structs for buffer queue to writeFile.
typedef struct _IO_BUFFER_NODE {
	CHAR buffer[512];		// Maximum of string to write = 512 chars
	ULONG bufferSize;
        ULONG sizeCounter;
	struct _IO_BUFFER_NODE *next;
} IO_BUFFER_NODE, *PIO_BUFFER_NODE;

typedef struct _IO_BUFFER_QUEUE {
	PIO_BUFFER_NODE first;
	PIO_BUFFER_NODE last;
} IO_BUFFER_QUEUE, *PIO_BUFFER_QUEUE;


ULONG
IoWriteFile(
        HANDLE file,
        PVOID buffer,
        ULONG bufferSize
        );

ULONG
IoReadFile(
        HANDLE file,
        PVOID buffer,
        ULONG bufferSize
        );

HANDLE
IoCreateFile(
        ULONG filename           // Hm.
        );

PCHAR
GetLine();

VOID
IoInterreuptHandler(CHAR c);	// Changed, but it matters only to me.

VOID
IoTransmitterInterruptHandler();

STATUS
IoInitialize();

#endif
