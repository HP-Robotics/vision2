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
        rc = recvfrom(cb->s, buf, sizeof(buf), 0, (struct sockaddr *) &si_other, &slen);
        if (rc == -1)
            break;

        cb->callback(cb->s, buf, rc, &si_other, slen);
    }

    close(cb->s);

}


static pthread_t thread;

int socket_start(char *hostport, void *callback)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    int port;
    char listen_as[1024];
    struct hostent *hostnm;
    struct sockaddr_in server;
    int s;
    static struct callback_info cb;
    int optval;
    char *p;


    strcpy(listen_as, hostport);
    p = strchr(listen_as, ':');
    if (!p)
    {
        fprintf(stderr, "Error: '%s' could not be parsed for host:port\n", hostport);
        return -1;
    }
    *p = '\0';
    port = atoi(p+1);
     

    /* Pick up 'our' host name and therefore our address for the bind() */
    hostnm = (struct hostent *) gethostbyname(listen_as);
    if (hostnm == (struct hostent *) 0)
    {
        fprintf(stderr, "Gethostbyname of '%s' failed\n", listen_as);
        return -1;
    }

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
