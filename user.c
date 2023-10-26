#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_SIZE 1024
#define MAX_CLIENTS 5

struct SharedData
{
    char message[SHM_SIZE];
    int pids[MAX_CLIENTS];
    int activeClients;
};

void signalHandler(int signum);
void cleanup();

struct SharedData *data;
char username[20];
int pidsUsed = 0;
int shmid;

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Enter a username. Example: ./user user1\n");
        exit(1);
    }

    strcpy(username, argv[1]);

    key_t key = ftok("shmfile", 65);
    shmid = shmget(key, sizeof(struct SharedData), 0666 | IPC_CREAT);
    if (shmid == -1)
    {
        perror("shmget");
        exit(1);
    }
    data = (struct SharedData *)shmat(shmid, NULL, 0);

    signal(SIGUSR1, signalHandler);
    signal(SIGINT, signalHandler);

    if (data->activeClients == 5)
    {
        printf("The maximum number of clients has been reached. Cannot create client.\n");
        exit(1);
    }

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {

        if (data->pids[i] != 0)
        {
            pidsUsed = 1;
        }
    }
    if (pidsUsed == 0)
    {
        memset(data->pids, 0, sizeof(data->pids));
    }

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {

        if (data->pids[i] == 0)
        {
            data->pids[i] = getpid();
            data->activeClients++;
            pidsUsed = 1;
            break;
        }
    }
    atexit(cleanup);
    while (1)
    {
        char localMessage[SHM_SIZE];
        fgets(localMessage, SHM_SIZE, stdin);
        localMessage[strlen(localMessage) - 1] = '\0';

        snprintf(data->message, SHM_SIZE, "[%s]: %s", username, localMessage);

        for (int i = 0; i < MAX_CLIENTS; ++i)
        {
            if (data->pids[i] != 0 && data->pids[i] != getpid())
            {
                kill(data->pids[i], SIGUSR1);
            }
        }
    }

    shmdt(data);

    return 0;
}

void signalHandler(int signum)
{
    if (signum == SIGUSR1)
    {
        printf("%s\n", data->message);
    }
    else if (signum == SIGINT)
    {
        // Handle SIGINT (Ctrl+C) to clean up resources
        cleanup();
        exit(0);
    }
}

void cleanup()
{
    int emptyPids = 1;
    // delete the pid corresponding to the user
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (data->pids[i] != 0)
        {
            emptyPids = 0;
        }
        if (data->pids[i] == getpid())
        {
            data->pids[i] = 0;
            data->activeClients--;
            break;
        }
    }
    if (emptyPids)
    {
        shmdt(data);
        shmctl(shmid, IPC_RMID, NULL);
    }
    // Detach shared memory segment and mark it for deletion
}