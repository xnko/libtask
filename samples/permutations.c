#include <stdio.h>
#include "../include/task.h"

static char series[] = { 'a', 'b', 'c', 'd', 0 };

void swap(char* x, char* y)
{
    char temp;
    temp = *x;
    *x = *y;
    *y = temp;
}

void permute(task_t* task, void* arg)
{
    task_t* permuter;
    int index = arg ? *(int*)arg : 0;
    int next = index + 1;
    int i;

    if (series[next] == 0)
    {
        task_yield(0);
        return;
    }

    for (i = index; series[i]; ++i)
    {
        swap(series + index, series + i);

        task_create(&permuter, permute, &next);
        while (!task_next(permuter, 0))
        {
            task_yield(0);
        }
        task_delete(permuter);

        swap(series + index, series + i);
    }
}

void main()
{
    task_t* permuter;

    task_create(&permuter, permute, 0);
    while (!task_next(permuter, 0))
    {
        printf("%s\r\n", series);
    }
    task_delete(permuter);
}