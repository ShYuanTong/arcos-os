#include <arcos.h>
#include <ob.h>
#include <ke.h>
#include <mm.h>
#include <rtl.h>

//
// head of type objects list
//
POBJECT_TYPE ObTypeObjectListHead = NULL;

#ifdef OB_STANDALONE
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#define MmAlloc(X) malloc(X)
#define MmFree(X) free(X)
#undef ASSERT
#define ASSERT(X) do { if (!(X)) printf("Assertion failed: " __FILE__ "(" STRINGIFY(__LINE__) "): " #X "\n" ), exit(1); } while (0)
#define KeBugCheck(X) do { printf(X); exit(1); } while (0)
#endif

#define OBJECT_BODY_TO_HEADER(pBody)   ((POBJECT_HEADER)CONTAINING_RECORD((pBody), OBJECT_HEADER, Body))

//
// checks the object header for corruption
//
#define ASSERT_OBJECT(pHeader)  ASSERT((pHeader)->Magic == 0x0BEC0BEC)


STATUS
ObCreateObject(
    POBJECT_TYPE ObjectType,
    ULONG ObjectAttributes,
    ULONG ObjectBodySize,
    PVOID *Object
    )
{
    POBJECT_HEADER objectHeader;

    ASSERT(Object);

    // 
    // make sure NULL is returned in Object on error
    //
    *Object = NULL;

    //
    // allocate memory for the new object
    //
    objectHeader = MmAlloc(FIELD_OFFSET(OBJECT_HEADER, Body) + ObjectBodySize);
    if (!objectHeader) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // initialize fields in the object header
    //
    objectHeader->Magic = 0x0BEC0BEC;
    objectHeader->HandleCount = 0;
    objectHeader->PointerCount = 1;   // compensate for the returned pointer
    objectHeader->SignalState = 0;
    objectHeader->WaitQueue = NULL;
    objectHeader->Attributes = ObjectAttributes;
    objectHeader->Type = ObjectType;
    
    //
    // check to see if we are creating a new type (ObjectType == NULL)
    //
    if (ObjectType) {

        //
        // add newly created object to the head of the object list
        //
        objectHeader->NextObjectOfType = ObjectType->FirstObjectOfType;
        ObjectType->FirstObjectOfType = objectHeader;
    }

    //
    // return object body pointer to caller
    //
    *Object = &objectHeader->Body;
    
    return STATUS_SUCCESS;
}

STATUS
ObCreateObjectType(
    ULONG TypeName,
    POBJECT_TYPE_INITIALIZER ObjectTypeInitializer,
    POBJECT_TYPE *ObjectType
    )
{
    POBJECT_TYPE objectBody;
    STATUS status;
    
    ASSERT(TypeName);
    ASSERT(ObjectTypeInitializer);
    ASSERT(ObjectType);

    // 
    // make sure NULL is returned in ObjectType on error
    //
    *ObjectType = NULL;

    //
    // create the type object
    //
    status = ObCreateObject(NULL, OBJ_PERMANENT, sizeof(OBJECT_TYPE), (PVOID*)&objectBody);
    if (status) {
        return status;
    }

    //
    // initialize body of the type object
    //
    objectBody->Name = TypeName;
    objectBody->FirstObjectOfType = NULL;
    objectBody->Dump = ObjectTypeInitializer->DumpMethod;
    objectBody->Delete = ObjectTypeInitializer->DeleteMethod;
    
    // 
    // add type to global type list
    // 
    objectBody->NextTypeObject = ObTypeObjectListHead;
    ObTypeObjectListHead = objectBody;

    //
    // return a pointer to the type object back to caller
    //
    *ObjectType = objectBody;

    return STATUS_SUCCESS;
}

VOID
ObpUnlinkObjectFromTypeList(
    POBJECT_HEADER objectHeader
    )
{
    POBJECT_HEADER currentObject;

    ASSERT_OBJECT(objectHeader);
    
    //
    // make sure object has a type associated
    //
    ASSERT(objectHeader->Type);

    //
    // get the head of the list
    //
    currentObject = objectHeader->Type->FirstObjectOfType;

    //
    // handle special case when first object is the object being removed
    //
    if (currentObject == objectHeader) {
        objectHeader->Type->FirstObjectOfType = currentObject->NextObjectOfType;
        return;
    }

    //
    // walk the list of objects with this type
    //
    while (currentObject) {

        //
        // check to see if following object is the object to be unlinked
        //
        if (currentObject->NextObjectOfType == objectHeader) {

            //
            // unlink it
            //
            currentObject->NextObjectOfType = objectHeader->NextObjectOfType;
            return;
        }

        //
        // move to the next list entry
        //
        currentObject = currentObject->NextObjectOfType;
    }

    //
    // object was not in the list, crash
    //
    KeBugCheck("ObpUnlinkObjectFromTypeList: object was not in the list");
}

VOID
ObDereferenceObject(
    PVOID Object
    )
{
    POBJECT_HEADER objectHeader;

    ASSERT(Object);

    // 
    // get a pointer to object header
    //
    objectHeader = OBJECT_BODY_TO_HEADER(Object);

    ASSERT_OBJECT(objectHeader);

    //
    // crash if someone did extra ObDereferenceObject calls and
    // made resource tracking unreliable
    //
    ASSERT(objectHeader->PointerCount > 0);
    ASSERT(objectHeader->HandleCount < objectHeader->PointerCount);

    //
    // decrement reference counter
    //
    objectHeader->PointerCount--;

    //
    // check to see if this was the last reference
    //
    if (objectHeader->PointerCount == 0) {

        //
        // check to see if this object is not permanent
        //
        if (!(objectHeader->Attributes & OBJ_PERMANENT)) {

            //
            // object is not permanent and nobody uses it -> delete it
            //
            // start by removing it from the list
            //
            ObpUnlinkObjectFromTypeList(objectHeader);

            //
            // if object has a type specific delete method, call it
            //
            if (objectHeader->Type && objectHeader->Type->Delete) {
                objectHeader->Type->Delete(Object);
            }

            //
            // deallocate backing storage
            //
            MmFree(objectHeader);
        }
            
    }
    
}

STATUS
ObInitProcess(
    PPROCESS ParentProcess,
    PPROCESS NewProcess
    )
{
    ULONG i;
    POBJECT_HEADER objectHeader;
    ASSERT(NewProcess);

    //
    // initialize handle table
    //
    for (i = 0; i < MAX_HANDLE_COUNT; i++) {

        //
        // if parent specified and handle is inheritable, inherit it
        //
        if (ParentProcess && ParentProcess->HandleTable[i].Object
            && (ParentProcess->HandleTable[i].Attributes & OBJ_INHERIT)) {

            NewProcess->HandleTable[i].Attributes = ParentProcess->HandleTable[i].Attributes;
            NewProcess->HandleTable[i].Object = ParentProcess->HandleTable[i].Object;

            //
            // update reference counters
            //
            objectHeader = OBJECT_BODY_TO_HEADER(NewProcess->HandleTable[i].Object);
            objectHeader->HandleCount++;
            objectHeader->PointerCount++;
        } else {
            NewProcess->HandleTable[i].Object = NULL;
        }
    }  

    return STATUS_SUCCESS;
}

VOID
ObKillProcess(
    PPROCESS Process
    )
{
    ULONG i;
    
    ASSERT(Process);

    //
    // check the handle table for open handles
    //
    for (i = 0; i < MAX_HANDLE_COUNT; i++) {

        //
        // if this handle is open, close it
        //
        if (Process->HandleTable[i].Object) {

            ObCloseHandle((HANDLE)i);
        }
    }
}

STATUS
ObOpenObjectByPointer(
    PVOID Object,
    ULONG HandleAttributes,
    POBJECT_TYPE ObjectType,
    PHANDLE Handle
    )
{
    ULONG i;
    POBJECT_HEADER objectHeader = OBJECT_BODY_TO_HEADER(Object);
    
    ASSERT(Object);
    ASSERT(Handle);
    ASSERT(KeCurrentProcess);

    ASSERT_OBJECT(objectHeader);

    *Handle = NULL;

    //
    // if a type object was specified, check it
    //
    if (ObjectType) {
       
        if (objectHeader->Type != ObjectType) {
            return STATUS_OBJECT_TYPE_MISMATCH;
        }
    }
    
    //
    // find a free handle slot in current process
    //
    for (i = 0; i < MAX_HANDLE_COUNT; i++) {

        //
        // is this slot free?
        //
        if (KeCurrentProcess->HandleTable[i].Object == NULL) {

            //
            // write handle information in this slot
            //
            KeCurrentProcess->HandleTable[i].Object = Object;
            KeCurrentProcess->HandleTable[i].Attributes = HandleAttributes;

            //
            // increment reference counters
            //
            objectHeader->HandleCount++;
            objectHeader->PointerCount++;

            *Handle = (HANDLE)i;

            return STATUS_SUCCESS;
        }
    }

    return STATUS_HANDLE_LIMIT_REACHED;
}

STATUS
ObReferenceObject(
    PVOID Object,
    POBJECT_TYPE ObjectType
)
{
    POBJECT_HEADER objectHeader;

    ASSERT(Object);

    objectHeader = OBJECT_BODY_TO_HEADER(Object);

    ASSERT_OBJECT(objectHeader);

    //
    // if a type object was specified, check it
    //
    if (ObjectType) {
    
        if (objectHeader->Type != ObjectType) {
            return STATUS_OBJECT_TYPE_MISMATCH;
        }
    }

    //
    // increase reference counter
    //
    objectHeader->PointerCount++;

    return STATUS_SUCCESS;
}

STATUS
ObReferenceObjectByHandle(
    HANDLE Handle,
    POBJECT_TYPE ObjectType,
    PVOID *Object
    )
{
    POBJECT_HEADER objectHeader;
    
    ASSERT(Object);

    *Object = NULL;

    if ((ULONG)Handle >= MAX_HANDLE_COUNT)
        return STATUS_INVALID_HANDLE;


    if (KeCurrentProcess->HandleTable[(ULONG)Handle].Object) {

        objectHeader = OBJECT_BODY_TO_HEADER(KeCurrentProcess->HandleTable[(ULONG)Handle].Object);

        ASSERT_OBJECT(objectHeader);

        //
        // if a type object was specified, check it
        //
        if (ObjectType) {
        
            if (objectHeader->Type != ObjectType) {
                return STATUS_OBJECT_TYPE_MISMATCH;
            }
        }

        //
        // increase reference couter
        //
        objectHeader->PointerCount++;

        *Object = KeCurrentProcess->HandleTable[(ULONG)Handle].Object;

        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_HANDLE;
}

STATUS
ObCloseHandle(
    HANDLE Handle
    )
{
    if ((ULONG)Handle >= MAX_HANDLE_COUNT)
        return STATUS_INVALID_HANDLE;
    
    if (KeCurrentProcess->HandleTable[(ULONG)Handle].Object) {

        POBJECT_HEADER objectHeader = OBJECT_BODY_TO_HEADER(KeCurrentProcess->HandleTable[(ULONG)Handle].Object);

        ASSERT_OBJECT(objectHeader);

        objectHeader->HandleCount--;

        ObDereferenceObject(KeCurrentProcess->HandleTable[(ULONG)Handle].Object);
        
        KeCurrentProcess->HandleTable[(ULONG)Handle].Object = NULL;

        return STATUS_SUCCESS;
    } else {
    
        return STATUS_INVALID_HANDLE;
    }
}

PVOID
ObGetFirstObjectOfType(
    POBJECT_TYPE ObjectType
    )
{
    ASSERT(ObjectType);

    ASSERT_OBJECT(OBJECT_BODY_TO_HEADER(ObjectType));

    //
    // is there an object of this type?
    //
    if (ObjectType->FirstObjectOfType) {

        return &ObjectType->FirstObjectOfType->Body;
    } else {

        return NULL;
    }
}

PVOID
ObGetNextObjectOfType(
    PVOID Object
    )
{
    POBJECT_HEADER objectHeader = OBJECT_BODY_TO_HEADER(Object);

    ASSERT(Object);
    ASSERT_OBJECT(objectHeader);

    //
    // is there a next object?
    //
    if (objectHeader->NextObjectOfType) {

        return &objectHeader->NextObjectOfType->Body;
    } else {

        return NULL;
    }
}

VOID
ObDumpObject(
    PVOID Object,
    PCHAR Buffer,
    ULONG BufferSize
    )
{
    POBJECT_HEADER objectHeader = OBJECT_BODY_TO_HEADER(Object);
    ULONG length;

    ASSERT(Object);
    ASSERT(Buffer);
    ASSERT_OBJECT(objectHeader);

    RtlFormatString(Buffer, BufferSize, "Refs: %d Handles: %d ", objectHeader->PointerCount, objectHeader->HandleCount);
    length = RtlStringLength(Buffer);

    if (objectHeader->Type) {
        if (objectHeader->Type->Dump) {

            objectHeader->Type->Dump(Object, Buffer + length, BufferSize - length);
        }
    }
}

STATUS
ObWaitForSingleObject(
    HANDLE Handle
)
{
    STATUS status;

    PVOID object;

    POBJECT_HEADER header;

    status = ObReferenceObjectByHandle(Handle, NULL, &object);
    if (status != STATUS_SUCCESS) {
        return status;
    }

    header = OBJECT_BODY_TO_HEADER(object);

    ASSERT_OBJECT(header);

    if (!header->SignalState) {

        POB_WAIT_QUEUE_ENTRY entry = (POB_WAIT_QUEUE_ENTRY)MmAlloc(sizeof(OB_WAIT_QUEUE_ENTRY));

        if (entry != NULL) {
            
            entry->Process = KeCurrentProcess;
            entry->Next = header->WaitQueue;
            header->WaitQueue = entry;

            KeBlockProcess();
        } else {
        
            ObDereferenceObject(object);
            return STATUS_NO_MEMORY;
        }
    }   

    ObDereferenceObject(object);

    return STATUS_SUCCESS;
}

VOID
ObSignalObject(
    PVOID Object
)
{
    POBJECT_HEADER header = OBJECT_BODY_TO_HEADER(Object);

    POB_WAIT_QUEUE_ENTRY entry, next;

    ASSERT_OBJECT(header);

    header->SignalState = 1;

    entry = header->WaitQueue;

    while (entry) {

        KeResumeProcess(entry->Process);

        next = entry->Next;

        MmFree(entry);

        entry = next;
    }

    header->WaitQueue = NULL;
}

