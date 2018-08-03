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

#ifndef TASK_H_INCLUDED
#define TASK_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#   if defined(TASK_BUILD_SHARED)
#       define TASK_API __declspec(dllexport)
#   elif defined(TASK_USE_SHARED)
#       define TASK_API __declspec(dllimport)
#   else
#       define TASK_API /* nothing */
#   endif
#elif __GNUC__ >= 4
#   define TASK_API __attribute__((visibility("default")))
#else
#   define TASK_API /* nothing */
#endif

// All calls return 0 on success and errno on failure

typedef struct task_t task_t;
typedef void (*task_fn)(task_t* task, void* arg);

// Create a new task with entry point and argument
TASK_API int task_create(task_t** task, task_fn entry, void* arg);

// Delete the task
TASK_API int task_delete(task_t* task);

// Yield a value to caller 
TASK_API int task_yield(void* value);

// Run task as a generator
TASK_API int task_next(task_t* task, void** yield);

// Post a task to current thread
TASK_API int task_post(task_fn entry, void* arg);

// Suspend the current task, and resume main task
TASK_API int task_suspend();

// Resume the supended task
TASK_API int task_resume(task_t* task);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // TASK_H_INCLUDED