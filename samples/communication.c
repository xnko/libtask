#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "../include/task.h"

const char* say_to_bot(const char* message)
{
    static char answer[1024*8];

    FILE* fp;
    int ok;
    char command[1024*10];

    strcpy(command, "curl -s -d \"user=Bob&send=&message=");

    int i = strlen(command);
	const char *c = message;
	char *hex = "0123456789abcdef";

    // Url encode the `message`
    while (*c)
    {
        if (isalnum(*c)) { command[i++] = *c; }
        else
        {
            command[i++] = '%';
            command[i++] = hex[*c >> 4];
            command[i++] = hex[*c >> 15];
        }

        ++c;
    }

    strcpy(command + i - 3, "\" -X POST http://ec2-54-215-197-164.us-west-1.compute.amazonaws.com/ui.php");

    fp = popen(command, "r");
    if (fp)
    {
        ok = fgets(answer, sizeof(answer), fp) != NULL;
        pclose(fp);

        if (ok) return answer;
    }

    return "I am busy now, please get back later";
}

struct {
    const char* text;
    task_t* alice;
    task_t* bob;
} channel = { 0, 0, 0 };

void alice(task_t* task, void* arg)
{
    const char* reply = 0;

    channel.alice = task;

    // Wait for Bob
    task_suspend();

    while (1)
    {
        // We got a message

        // Build a reply
        reply = say_to_bot(channel.text);

        // Reply to Bob
        channel.text = reply;
        task_resume(channel.bob);
    }
}

void bob(task_t* task, void* arg)
{
    char text[1024];
    const char* response;

    channel.bob = task;

    while (1)
    {
        // Read a message from input
        printf("    I >  ");
        fgets(text, sizeof(text), stdin);

        // Say to alice
        channel.text = text;
        task_resume(channel.alice);

        // Print response
        printf("Alice >  ");
        puts(channel.text);
    }
}

void main()
{
    task_post(alice, 0);
    task_post(bob, 0);
}