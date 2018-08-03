#include <stdio.h>
#include "../include/task.h"

void fibonacci(task_t* task, void* arg)
{
    int series, first = 0, second = 1, next, c;

    series = *(int*)arg;

    for (c = 0; c < series; ++c)
    {
        if (c <= 1)
            next = c;
        else
        {
            next = first + second;
            first = second;
            second = next;
        }

        task_yield(&next);
    }
}

void main()
{
    task_t* task;
    int* next;
    int series = 30;

    task_create(&task, fibonacci, &series);
    
    while (!task_next(task, (void**)&next))
    {
        printf("%d\r\n", *next);
    }

    task_delete(task);
}