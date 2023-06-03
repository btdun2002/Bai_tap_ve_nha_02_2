#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <time.h>

struct client_info
{
    int fd;
    char client_name[32];
    char client_id[32];
    int is_set_info;
};

int main()
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1)
    {
        perror("socket() failed");
        return 1;
    }

    unsigned long ulong_value = 1;
    ioctl(listener, FIONBIO, &ulong_value);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(9000);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind() failed");
        return 1;
    }

    if (listen(listener, 5))
    {
        perror("listen() failed");
        return 1;
    }

    fd_set fdread;

    struct client_info clients[64] = {0};
    int num_clients = 0;
    char buf[256];
    char message[1024];
    char *client_id;
    char *client_name;
    int ret;

    printf("Server started. Listening on port 9000\n");

    while (1)
    {
        FD_ZERO(&fdread);

        FD_SET(listener, &fdread);
        for (int i = 0; i < num_clients; i++)
            FD_SET(clients[i].fd, &fdread);

        ret = select(FD_SETSIZE, &fdread, NULL, NULL, NULL);

        if (FD_ISSET(listener, &fdread))
        {
            if (num_clients < 64)
            {
                int client = accept(listener, NULL, NULL);

                if (client == -1)
                {
                    if (errno == EWOULDBLOCK || errno == EAGAIN)
                    {
                        // Van dang cho ket noi
                        // Bo qua khong xu ly
                        continue;
                    }
                    else
                    {
                        perror("accept() failed");
                        break;
                    }
                }
                else
                {
                    clients[num_clients].fd = client;
                    unsigned long ulong = 1;
                    ioctl(client, FIONBIO, &ulong);
                    printf("New client connected %d\n", client);
                    num_clients++;
                }
            }
        }

        for (int i = 0; i < num_clients; i++)
        {
            if (FD_ISSET(clients[i].fd, &fdread))
            {
                ret = recv(clients[i].fd, buf, sizeof(buf), 0);
                if (ret == -1)
                {
                    if (errno == EWOULDBLOCK || errno == EAGAIN)
                    {
                        // Van dang doi du lieu
                        // Bo qua chuyen sang socket khac
                        continue;
                    }
                    else
                    {
                        perror("recv() failed");
                        continue;
                    }
                }
                else if (ret == 0)
                {
                    printf("Client disconnected %d\n", clients[i].fd);
                    close(clients[i].fd);
                    for (int j = i; j < num_clients - 1; j++)
                    {
                        clients[j] = clients[j + 1];
                    }
                    num_clients--;
                    i--;
                    continue;
                }
                else
                {
                    if (clients[i].is_set_info == 0)
                    {
                        printf("Received from %d: %s\n", clients[i].fd, buf);
                        buf[ret] = 0;
                        client_name = strtok(buf, ":");
                        client_id = strtok(NULL, ":");
                        if (client_name == NULL || client_id == NULL || strlen(client_name) == 0 || strlen(client_id) == 0)
                        {
                            send(clients[i].fd, "Invalid input format. Please send 'client_id: client_name' again\n", strlen("Invalid input format. Please send 'client_id: client_name' again\n"), 0);
                            close(clients[i].fd);
                            for (int j = i; j < num_clients - 1; j++)
                            {
                                clients[j] = clients[j + 1];
                            }
                            num_clients--;
                            i--;
                            continue;
                        }
                        strcpy(clients[i].client_name, client_name);
                        strcpy(clients[i].client_id, client_id);
                        clients[i].is_set_info = 1;
                    }
                    else if (clients[i].is_set_info == 1)
                    {
                        buf[ret] = 0;
                        time_t now = time(NULL);
                        struct tm *t = localtime(&now);
                        snprintf(message, sizeof(message), "%4d-%2d-%2d %2d:%2d:%2d %s: %s",
                                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                                 t->tm_hour, t->tm_min, t->tm_sec, clients[i].client_name, buf);

                        for (int j = 0; j < num_clients; j++)
                        {
                            if (j == i)
                                continue;
                            ret = send(clients[j].fd, message, strlen(message), 0);
                            if (ret == -1)
                            {
                                perror("send() failed");
                                return 1;
                            }
                        }
                    }
                    else
                    {
                        perror("Error is_set_info");
                        return 1;
                    }
                }
            }
        }
    }

    close(listener);

    return 0;
}