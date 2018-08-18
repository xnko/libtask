/* Copyright (c) 2014, Artak Khnkoyan <artak.khnkoyan@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#define PLATFORM_WINDOWS 0
#define PLATFORM_ANDROID 0
#define PLATFORM_APPLE   0
#define COMPILER_CLANG   0
#define COMPILER_GCC     0
#define COMPILER_MSVC    0
#define COMPILER_INTEL   0

// Platforms
#if defined(_WIN32) || defined(__WIN32__) || defined(_WIN64)
#   undef   PLATFORM_WINDOWS
#   define  PLATFORM_WINDOWS 1
#   if TASK_BUILD_SHARED && !defined(_CRT_SECURE_NO_WARNINGS)
#       define  _CRT_SECURE_NO_WARNINGS 1
#   endif
#   define  WIN32_LEAN_AND_MEAN 1
#   include <Windows.h>
#endif

#if defined(__ANDROID__)
#   undef   PLATFORM_ANDROID
#   define  PLATFORM_ANDROID 1
#endif

#if (defined(__APPLE__) && __APPLE__)
#   undef   PLATFORM_APPLE
#   define  PLATFORM_APPLE 1
#   include <TargetConditionals.h>
#endif

// Compilers
#if defined(__clang__)
#   undef   COMPILER_CLANG
#   define  COMPILER_CLANG 1
#   if PLATFORM_WINDOWS
#       define THREADLOCAL
#   else
#       define THREADLOCAL __thread
#   endif
#   ifndef FORCEINLINE
#       define FORCEINLINE inline __attribute__((__always_inline__))
#   endif
#endif

#if defined(__GNUC__)
#   undef   COMPILER_GCC
#   define  COMPILER_GCC 1
#   define THREADLOCAL __thread
#   ifndef FORCEINLINE
#       define FORCEINLINE inline __attribute__((__always_inline__))
#   endif
#endif

#if defined(__ICL) || defined(__ICC) || defined(__INTEL_COMPILER)
#   undef   COMPILER_INTEL
#   define  COMPILER_INTEL 1
#   define THREADLOCAL __declspec(thread)
#   ifndef FORCEINLINE
#       define FORCEINLINE __forceinline
#   endif
#endif

#if defined(_MSC_VER)
#   undef   COMPILER_MSVC
#   define   COMPILER_MSVC 1
#   define THREADLOCAL __declspec(thread)
#   ifndef FORCEINLINE
#       define FORCEINLINE __forceinline
#   endif
#endif

#if PLATFORM_WINDOWS == 0
#   include <pthread.h>
#endif

#include "../include/task.h"
#include <malloc.h>
#include "context.h"
typedef ucontext_t context_t;

typedef struct task_t {
    context_t context;
    struct task_t* parent;
    char*   stack;
    size_t  stack_size;
    task_fn entry;
    void*   arg;
    int     is_done;
    int     is_post;
    int     inherit_error_state;
} task_t;

typedef struct scheduler_t {
    struct task_t* current;
    struct task_t* prev;
    struct task_t  main;
    void* value; // last yield value
} scheduler_t;

#if PLATFORM_WINDOWS

static DWORD tlsScheduler = 0;

static scheduler_t* get_thread_scheduler()
{
    if (tlsScheduler == 0)
    {
        tlsScheduler = TlsAlloc();
        TlsSetValue(tlsScheduler, 0);

        return 0;
    }

    return TlsGetValue(tlsScheduler);
}

static void set_thread_scheduler(scheduler_t* scheduler)
{
    if (tlsScheduler == 0)
    {
        tlsScheduler = TlsAlloc();
    }

    TlsSetValue(tlsScheduler, scheduler);
}

#else // Not Windows
#   include <pthread.h>
static pthread_key_t keyScheduler = -1;
static void task_scheduler_delete(void* unused);

static scheduler_t* get_thread_scheduler()
{
    if (keyScheduler == -1)
    {
        pthread_key_create(&keyScheduler, task_scheduler_delete);
        pthread_setspecific(keyScheduler, 0);

        return 0;
    }

    return pthread_getspecific(keyScheduler);
}

static void set_thread_scheduler(scheduler_t* scheduler)
{
    if (keyScheduler == -1)
    {
        pthread_key_create(&keyScheduler, task_scheduler_delete);
    }

    pthread_setspecific(keyScheduler, scheduler);
}

#endif

static uintptr_t task_default_stack_size = 0;

static uintptr_t task_get_default_stack_size()
{
    if (task_default_stack_size == 0)
    {
        // ToDo: retrieve current thread stack size
        task_default_stack_size = 1024 * 1024;
    }

    return task_default_stack_size;
}

static void task_scheduler_delete(void* unused)
{
    scheduler_t* scheduler = get_thread_scheduler();

    if (scheduler != 0)
    {
        set_thread_scheduler(0);

        free(scheduler);
    }
}

#if PLATFORM_WINDOWS && !defined(TASK_BUILD_SHARED)

VOID WINAPI FlsCleanupCallback(_In_ PVOID lpFlsData)
{
    task_scheduler_delete(0);
}

#endif

static scheduler_t* task_scheduler_ensure()
{
#if PLATFORM_WINDOWS && !defined(TASK_BUILD_SHARED)
    static DWORD flsIndex = 0;
#endif
    scheduler_t* scheduler = get_thread_scheduler();

    if (scheduler == 0)
    {
        task_get_default_stack_size();

        scheduler = (scheduler_t*)calloc(1, sizeof (scheduler_t));

        scheduler->current = &scheduler->main;
        scheduler->prev = 0;

        set_thread_scheduler(scheduler);

        // On Windows shared build, cleanup in DllMain's DLL_THREAD_DETACH case
        // On Windows static build, cleanup using FlsAlloc(cleanup_callback)
        // On others, cleanup using pthread_key_create's destructor
#if PLATFORM_WINDOWS && !defined(TASK_BUILD_SHARED)
        if (flsIndex == 0)
        {
            flsIndex = FlsAlloc(FlsCleanupCallback);
        }
#endif
    }

    return scheduler;
}

#if PLATFORM_WINDOWS && TASK_BUILD_SHARED

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason) {
    case DLL_THREAD_DETACH:
        task_scheduler_delete(0);
        break;
    }

    return TRUE;
}

#endif

static void task_entry_point()
{
    scheduler_t* scheduler = task_scheduler_ensure();
	task_t* task = scheduler->current;

    task->entry(task, task->arg);
    task->is_done = 1;
    scheduler->prev = task;
    setcontext(&task->parent->context);
}

#if PLATFORM_WINDOWS && (COMPILER_MSVC || COMPILER_INTEL)
#pragma optimize("", off)
#endif
void task_swapcontext(struct task_t* current, struct task_t* other)
{
    scheduler_t* scheduler = task_scheduler_ensure();
	
    /*
     * save error state
     */

    int error = errno;
#if PLATFORM_WINDOWS && (COMPILER_MSVC || COMPILER_INTEL)
    DWORD win_error = GetLastError();
#endif

    scheduler->prev = current;
    scheduler->current = other;
	swapcontext(&current->context, &other->context);
    scheduler->current = current;

    if (scheduler->prev != 0 &&
        scheduler->prev->is_post &&
        scheduler->prev->is_done)
    {
        /*
         * if prev was posted and done then delete it
         */

        task_delete(scheduler->prev);
        scheduler->prev = 0;
    }

    if (scheduler->current->inherit_error_state == 0)
    {
        /*
         * if current task does not inherit error state, then
         * restore its last error state
         */

        errno = error;
#if PLATFORM_WINDOWS && (COMPILER_MSVC || COMPILER_INTEL)
        SetLastError(win_error);
#endif
    }
}
#if PLATFORM_WINDOWS && (COMPILER_MSVC || COMPILER_INTEL)
#pragma optimize("", on)
#endif


#if PLATFORM_WINDOWS

static DWORD page_size = 0;

static DWORD task_get_page_size()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    return si.dwPageSize;
}

static void* task_create_stack(size_t size)
{
    if (page_size == 0)
    {
        page_size = task_get_page_size();
    }

    void* vp = VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
    if (!vp)
    {
        errno = ENOMEM;
        return 0;
    }

    // needs at least 2 pages to fully construct the coroutine and switch to it
    size_t init_commit_size = page_size + page_size;
    char* pPtr = ((char*)vp) + size;
    pPtr -= init_commit_size;
	if (!VirtualAlloc(pPtr, init_commit_size, MEM_COMMIT, PAGE_READWRITE))
	{
		goto cleanup;
	}

    // create guard page so the OS can catch page faults and grow our stack
    pPtr -= page_size;
	if (VirtualAlloc(pPtr, page_size, MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD))
	{
		return vp;
	}

cleanup:

    VirtualFree(vp, 0, MEM_RELEASE);
    
    errno = ENOMEM;
    return 0;
}

static void task_delete_stack(void* stack, size_t size)
{
    VirtualFree(stack, 0, MEM_RELEASE);
}

#else

#include <sys/mman.h>

static int page_size = 0;

static void* task_create_stack(size_t size)
{
    if (page_size == 0)
    {
        page_size = getpagesize();
    }

    void* vp = mmap(
        0,                      // void *addr,
        size,                   // size_t length,
        PROT_READ | PROT_WRITE, // int prot,
        MAP_ANONYMOUS |         // int flags,
        MAP_NORESERVE |
        MAP_STACK |
        // MAP_UNINITIALIZED |
        MAP_PRIVATE,  
        -1,                     // int fd,
        0                       // off_t offset
    );
    if (!vp)
    {
        return 0;
    }
}

static void task_delete_stack(void* stack, size_t size)
{
    munmap(stack, size);
}

#endif

int task_create(struct task_t** task, task_fn entry, void* arg)
{
    uintptr_t stack_size = task_get_default_stack_size();

    *task = (task_t*)calloc(1, sizeof(**task));
    if (*task == 0)
    {
        errno = ENOMEM;
        return errno;
    }

    (*task)->stack = task_create_stack(stack_size);
    if ((*task)->stack == 0)
    {
        free(*task);

        errno = ENOMEM;
        return errno;
    }

    (*task)->stack_size = stack_size;
    (*task)->entry = entry;
    (*task)->arg = arg;
    (*task)->is_done = 0;
    (*task)->is_post = 0;
    (*task)->parent = 0;
    (*task)->inherit_error_state = 0;

#if PLATFORM_WINDOWS && (COMPILER_MSVC || COMPILER_INTEL)

    (*task)->context.uc.ContextFlags = CONTEXT_ALL;

    getcontext(&(*task)->context);

    (*task)->context.stack = (*task)->stack;
    (*task)->context.stack_size = (*task)->stack_size;

    makecontext(&(*task)->context, (void(*)())task_entry_point, 1, *task);

#else

    getcontext(&(*task)->context);

    (*task)->context.uc_stack.ss_sp = (*task)->stack;
    (*task)->context.uc_stack.ss_size = (*task)->stack_size;
    (*task)->context.uc_stack.ss_flags = 0;
    (*task)->context.uc_link = 0;

    makecontext(&(*task)->context, (void(*)())task_entry_point, 1, *task);

#endif

    return 0;
}

int task_delete(struct task_t* task)
{
    scheduler_t* scheduler = task_scheduler_ensure();
    if (scheduler->current == task)
    {
        /* Cannot delete self task */
        return EDEADLOCK;
    }

    task_delete_stack(task->stack, task->stack_size);
    free(task);

    return 0;
}

int task_yield(void* value)
{
    scheduler_t* scheduler = task_scheduler_ensure();
    task_t* current = scheduler->current;
    if (current == &scheduler->main)
    {
        /* Cannot yield main task */
        return EDEADLOCK;
    }
    
    scheduler->value = value;
    task_swapcontext(current, current->parent);

    return 0;
}

static int task_exec(task_t* task, void** yield)
{
    scheduler_t* scheduler = task_scheduler_ensure();
    int error_state;

    if (task->is_done)
    {
        /* Cannot exec done task */
        return EALREADY;
    }
    
    /*
     * save/restore current tasks inherit_error_state value
     */
    error_state = scheduler->current->inherit_error_state;

    task->parent = scheduler->current;
    task->is_post = 0;
	
    /*
     * inherit error state set by callee task
     */
    scheduler->current->inherit_error_state = 1;
    task_swapcontext(scheduler->current, task);
    scheduler->current->inherit_error_state = error_state;

    if (yield)
        *yield = scheduler->value;

    return 0;
}

int task_next(task_t* task, void** yield)
{
    int error = task_exec(task, yield);
    if (error)
    {
        return error;
    }

    if (task->is_done)
    {
        /* Done */
        return EALREADY;
    }

    return 0;
}

int task_post(task_fn entry, void* arg)
{
    task_t* task;
    int error = task_create(&task, entry, arg);
    scheduler_t* scheduler = task_scheduler_ensure();

    if (error)
    {
        return error;
    }

    task->parent = &scheduler->main;
    task->is_post = 1;
	
    task_swapcontext(scheduler->current, task);

    return 0;
}

int task_suspend()
{
    scheduler_t* scheduler = task_scheduler_ensure();
    task_t* current = scheduler->current;

    if (current == &scheduler->main)
    {
        // Cannot yeld main task
        return EDEADLOCK;
    }

    /*
     * save/restore current tasks inherit_error_state value
     */
    int error_state = current->inherit_error_state;

    current->inherit_error_state = 1;
    task_swapcontext(current, &scheduler->main);
    current->inherit_error_state = error_state;

    return 0;
}

int task_resume(task_t* task)
{
    scheduler_t* scheduler = task_scheduler_ensure();
    task_swapcontext(scheduler->current, task);

    return 0;
}