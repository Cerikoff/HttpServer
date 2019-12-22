#include <iostream>
#include <string_view>
#include <string>
#include <sstream>
#include <iterator>
#include <fstream>
#include <thread>

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

void sendNotFound(const int fd)
{
	static const char notFound[] = 
		"HTTP/1.0 404 NOT FOUND\r\nContent-length: 0\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n";
	send(fd, notFound, sizeof notFound, MSG_NOSIGNAL);
}

void sendResp(const int fd)
{
	static char buffer[2048];
	int res = recv(fd, buffer, 2048, MSG_NOSIGNAL);
	if(res == 0 && errno != EAGAIN)
	{
		shutdown(fd, SHUT_RDWR);
		close(fd);
	}
	else if(res > 0)
	{
		std::string req(buffer);
		auto methodIt = req.find_first_of(' ');

		if(std::string method(req.substr(0, methodIt)); method.compare("GET"))
		{
			sendNotFound(fd);
		}
		else
		{
			req.erase(0, methodIt + 1);
			auto fileEndIndex = req.find_first_of("? ");

			std::string file(req.substr(1, fileEndIndex - 1));

			std::cout << file << file.size() << std::endl;

			std::ifstream is (file);
  			if (is) {
    			is.seekg (0, is.end);
    			int length = is.tellg();
    			is.seekg (0, is.beg);

    			char *buffer = new char [length];

    			is.read (buffer,length);

    			is.close();

				std::stringstream stream;
				stream << "HTTP/1.0 200 OK\r\n";
				stream << "Content-length: "<< length << "\r\n";
				stream << "Connection: close\r\n";
				stream << "Content-Type: text/html\r\n";
				stream << "\r\n";
				stream << buffer;

				auto resp = stream.str();

				send(fd, resp.c_str(), resp.size(), MSG_NOSIGNAL);

    			delete[] buffer;
			}
			else 
			{
				sendNotFound(fd);
			}
		}
	}
}

void startServer(const std::string &host,
				 const std::string &port,
				 const std::string &directory)
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
				int fd = evlist[i].data.fd;
				std::thread thread(sendResp, fd);
				thread.detach();
				epoll_ctl(epfd, EPOLL_CTL_DEL, evlist[i].data.fd, &event);
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