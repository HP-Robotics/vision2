/*-------------------------------------------------------------------------
* socket.c - Sub routines to provide data via a udp socket.
* -------------------------------------------------------------------------
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
* -------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>


#include "socket.h"
#include "vision.h"

struct callback_info
{
    int s;
    int (*callback)(int s, char *buf, int len, void *from, int from_len);
};

static void main_listen_thread(void *info)
{
    struct callback_info *cb = (struct callback_info *) info;
    char buf[4096];
    int rc;
    struct sockaddr_in si_other;
    socklen_t slen = sizeof(si_other);

    while (1)
    {
        memset(buf, 0, sizeof(buf));
        rc = recvfrom(cb->s, buf, sizeof(buf) - 1, 0, (struct sockaddr *) &si_other, &slen);
        if (rc == -1)
            break;

        cb->callback(cb->s, buf, rc, &si_other, slen);
    }

    close(cb->s);

}


static pthread_t thread;

int socket_start(int port, void *callback)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    struct sockaddr_in server;
    int s;
    static struct callback_info cb;
    int optval;
     

    server.sin_family      = AF_INET;
    server.sin_port        = htons(port);
    server.sin_addr.s_addr =  htonl(INADDR_ANY);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    {
        perror("Socket()");
        return -2;
    }

    optval = 1;
    (void) setsockopt (s, SOL_SOCKET, SO_REUSEADDR, (char *) &optval, sizeof (optval));

    if (bind(s, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
       perror("Bind()");
       return -3;
    }

    cb.s = s;
    cb.callback = callback;

    return(pthread_create(&thread, &attr, (void * (*)(void *))main_listen_thread, (void *) &cb));
}
