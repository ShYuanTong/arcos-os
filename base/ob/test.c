#include <arcos.h>
#include <ob.h>
#include <stdio.h>
#include <stdlib.h>

PROCESS CurrentProcess;

//
// macro to check returned STATUS values
//
#define CHECK_STATUS(X) \
    do { \
        if ((X) != STATUS_SUCCESS) { \
            printf("Function returned status %d, line %d\n", (X), __LINE__); \
            exit(1); \
        } \
    } while (0)

//
// KeCurrentProcess is a global variable exported from the scheduler
// and keeps a pointer to the process that is currently executing
//
PPROCESS KeCurrentProcess = &CurrentProcess;

//
// holds a pointer to the FOO type object
//
POBJECT_TYPE fooType;

//
// defines structure for objects of type foo
//
typedef struct _FOO {
    ULONG value;
} FOO, *PFOO;


VOID
DeleteFoo(
    PFOO foo
    )
{
    printf("FOO object with value %d getting deleted\n", foo->value);
}

VOID
DumpFoo(
    PFOO foo,
    PCHAR buffer,
    ULONG bufferSize
    )
{
    _snprintf(buffer, bufferSize, "FOO, value: %d", foo->value);
}

STATUS
InitializeFoo(
    )
{
    OBJECT_TYPE_INITIALIZER typeInitializer;
    STATUS status;
    
    //
    // create a new object type: foo
    //
    typeInitializer.DumpMethod = DumpFoo;
    typeInitializer.DeleteMethod = DeleteFoo;
    status = ObCreateObjectType('foo', &typeInitializer, &fooType);

    return status;
}

STATUS
CreateFoo(
    ULONG value,
    PHANDLE Handle
    )
{
    STATUS status;
    PFOO foo;

    //
    // create a foo object
    //
    status = ObCreateObject(fooType, 0, sizeof(FOO), &foo);
    if (status == STATUS_SUCCESS) {
        //
        // initialize the object
        //
        foo->value = value;

        //
        // create a handle to the object
        //
        status = ObOpenObjectByPointer(foo, OBJ_INHERIT, fooType, Handle);

        //
        // we don't need the RAW pointer anymore, so dereference it
        //
        ObDereferenceObject(foo);

        return status;
    }

    return status;
}

ULONG
GetFooValue(
    HANDLE Handle
    )
{
    STATUS status;
    PFOO foo;

    status = ObReferenceObjectByHandle(Handle, fooType, &foo);
    if (status == STATUS_SUCCESS) {
        ULONG value = foo->value;

        ObDereferenceObject(foo);

        return value;
    }

    return (ULONG)-1;
}

void
DumpFooList()
{
    PFOO currentFoo = (PFOO)ObGetFirstObjectOfType(fooType);

    printf("*** LIST OF OBJECTS OF TYPE FOO ***\n");

    while (currentFoo) {
        CHAR buffer[256];
        ObDumpObject(currentFoo, buffer, sizeof(buffer));
        printf("%s\n", buffer);

        currentFoo = ObGetNextObjectOfType(currentFoo);
    }
}

int main()
{
    STATUS status;
    HANDLE handle1, handle2, handle3, handle4;

    //
    // ObInitProcess is called when the process manager creates a new process
    //
    status = ObInitProcess(NULL, KeCurrentProcess);
    CHECK_STATUS(status);
    
    status = InitializeFoo();
    CHECK_STATUS(status);

    status = CreateFoo(42, &handle1);
    CHECK_STATUS(status);

    status = CreateFoo(1985, &handle2);
    CHECK_STATUS(status);

    status = CreateFoo(2009, &handle3);
    CHECK_STATUS(status);

    printf("Foo has value: %d\n\n", GetFooValue(handle1));

    DumpFooList();

    ObCloseHandle(handle2);

    DumpFooList();

    ObCloseHandle(handle1);

    DumpFooList();

    status = CreateFoo(65536, &handle4);
    CHECK_STATUS(status);

    DumpFooList();

    //
    // ObKillProcess is called when the process manager kills a process
    //
    ObKillProcess(KeCurrentProcess);
}