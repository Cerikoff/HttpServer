#include <iostream>
#include <string_view>
#include <string>

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define MAX_EVENTS 32

int set_nonblock(int fd)
{
    int flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
} 

void startServer(std::string_view host,
				 std::string_view port,
				 std::string_view directory)
{
    pid_t pid;

    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    if (setsid() < 0)
        exit(EXIT_FAILURE);

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    if (pid > 0)
        exit(EXIT_SUCCESS);

    umask(0);

    chdir(directory.data());

    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }

	int s = socket(AF_INET, SOCK_STREAM, 0);
	set_nonblock(s);

	if (s == -1)
	{
		perror("socket error");
		exit(1);
	}

	struct sockaddr_in servAddr;
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = inet_addr(host.data());
	servAddr.sin_port = htons(std::stoi(port.data()));
	
	int yes = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    } 

	if (bind(s, (struct sockaddr *) &servAddr, sizeof(servAddr)) == -1) {
        perror("bind error");
        close(s);
        exit(1);
    }

	if (listen(s, 10) == -1) {
        perror("listen error");
        close(s);
        exit(1);
    }

	int epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create");
    }

	struct epoll_event event;
	event.data.fd = s;
	event.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, s, &event);

	while(true)
	{
		struct epoll_event evlist[MAX_EVENTS];
		int N = epoll_wait(epfd, evlist, MAX_EVENTS, -1);

		for(unsigned i = 0; i < N; ++i)
		{
			if(evlist[i].data.fd == s) 
			{
				int slaveSocket = accept(s, 0, 0);
				set_nonblock(slaveSocket);

				struct epoll_event event;
				event.data.fd = slaveSocket;
				event.events = EPOLLIN;
				epoll_ctl(epfd, EPOLL_CTL_ADD, slaveSocket, &event);
			}
			else
			{
				static char buffer[2048];
				int res = recv(evlist[i].data.fd, buffer, 2048, MSG_NOSIGNAL);
				if(res == 0 && errno != EAGAIN)
				{
					shutdown(evlist[i].data.fd, SHUT_RDWR);
					close(evlist[i].data.fd);
				}
				else if(res > 0)
				{
					static char response[] = "HTTP/1.0 200 OK\r\n"
											"Content-length: 5\r\n"
											"Connection: close\r\n"
											"Content-Type: text/html\r\n"
											"\r\n"
											"hello";
					send(evlist[i].data.fd, response, sizeof response, MSG_NOSIGNAL);
				}
			}
		}
	}
}

int main(int argc, char** argv)
{
	char *optString = "h:p:d:";
	auto opt = getopt(argc, argv, optString);

	std::string host;
	std::string port;
	std::string directory;

	while (opt != -1)
	{
		switch (opt)
		{
		case 'h':
			host = optarg;
			break;

		case 'p':
			port = optarg;
			break;

		case 'd':
			directory = optarg;
			break;
		
		default:
			break;
		}

		opt = getopt(argc, argv, optString);
	}

	startServer(host, port, directory);
}