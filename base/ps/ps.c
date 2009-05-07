/* 
 * File:   ps.c
 * Author: Magnus Söderling
 *
 */

#include <arcos.h>
#include <ps.h>
#include <mm.h>
#include <ob.h>


STATUS
PsCreateProcess(
        PVOID (*PStartingAddress)(),
        ULONG Priority,
        PHANDLE ProcessHandle,
        PCHAR Args
        ){
    STATUS status;
    PVOID mempointer;
    PPROCESS newprocess = NULL;


    status = ObInitProcess(NULL, newprocess);
    if (status != STATUS_SUCCESS)
        return status;


    mempointer = MmAlloc(PROCESS_MEMORY_TO_ALLOCATE);
    if (mempointer == NULL)
        return STATUS_NO_MEMORY;
  
    ProcessHandle = (PHANDLE)newprocess;
    return STATUS_SUCCESS;
}