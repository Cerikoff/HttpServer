#include <iostream>
#include <string_view>

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

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