#include <stdio.h>
#include "../include/task.h"

void pong(task_t* task, void* arg)
{
    *((task_t**)arg) = task;

    int iteration = 1;

    while (1)
    {
        task_suspend();

        printf("pong %d\r\n", iteration++);
    }
}

void main()
{
    int iteration = 0;
    task_t* ponger;

    task_post(pong, &ponger);

    while (iteration++ < 30)
    {
        printf("ping %d\r\n", iteration);

        task_resume(ponger);
    }
}