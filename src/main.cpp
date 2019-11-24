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

void startServer(std::string_view host,
				 std::string_view port,
				 std::string_view directory)
{
	std::cout << host << " " << port << " " << directory << std::endl;
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

	int epfd = epoll_create(5);
    if (epfd == -1) {
        perror("epoll_create");
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

	 while (1)
     {
         pause();
     }
}