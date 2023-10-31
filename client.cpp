#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unordered_map>
#include <unordered_set>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <sys/types.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <cmath>
#include <vector>

using namespace std;

typedef int Socket;

#define NONE						0
#define TCP_NEW_CONNECTION			1
#define TCP_EXISTING_CONNECTION		2
#define UDP_CONNECTION				3
#define KEYBOARD_INPUT				4

#define MAX_NO 						50
#define MAX_SIZE 					1551

struct MUDP {
    char topic[50];
    char type;
    char payload[1500];
};

struct MTCP {
	char id[10], topic[50];
	bool disconnect, subscribe, sf;
};

int main(int argc, char **argv)
{
	bool stop = false;
    struct sockaddr_in server_info;
	fd_set fds, fdsTmp;
    int flag = 1;
	Socket fd;
	MUDP mUDP;
	MTCP mTCP;

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

    FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
    FD_SET(fd, &fds);

	inet_aton(argv[2], &server_info.sin_addr);
	server_info.sin_port = htons(atoi(argv[3]));
    server_info.sin_family = AF_INET;

    connect(fd, (struct sockaddr*)&server_info, sizeof(server_info));
    send(fd, argv[1], strlen(argv[1]), 0);
    recv(fd, &stop, sizeof(stop), 0);

    if (stop)
	{
        close(fd);
        return 0;
    }

    while (true)
	{
        fd_set fdsTmp;
		struct sockaddr_in udpSockaddr {};
		int messageSize;
		char command[MAX_SIZE];
        char *topic, *sf;

        FD_ZERO(&fdsTmp);
        fdsTmp = fds;

        select(fd + 1, &fdsTmp, NULL, NULL, NULL);
		memset(&mUDP, '\0', sizeof(mUDP));
		memset(&mTCP, '\0', sizeof(mTCP));

        if (FD_ISSET(STDIN_FILENO, &fdsTmp))
		{
            memset(command, '\0', MAX_SIZE);
			fgets(command, MAX_SIZE, stdin);

			if (strstr(command, "exit"))
			{
                strcpy(mTCP.id, argv[1]);
                mTCP.disconnect = true;
                send(fd, &mTCP, sizeof(mTCP), 0);
                break;
            }

			if (strstr(command, "subscribe") == command)
			{
                strtok(command, " ");
                topic = strtok(NULL, " ");
                sf = strtok(NULL, " \n");
				mTCP.sf = !(sf[0] == '0');
				mTCP.subscribe = true;
				strcpy(mTCP.id, argv[1]);
				strcpy(mTCP.topic, topic);
                send(fd, &mTCP, sizeof(mTCP), 0);
                cout << "Subscribed to topic.\n";
            } else if (strstr(command, "unsubscribe") == command) {
                topic = strtok(command, " ");
                topic = strtok(NULL, " \n");
				strcpy(mTCP.topic, topic);
                strcpy(mTCP.id, argv[1]);
                send(fd, &mTCP, sizeof(mTCP), 0); 
                cout << "Unsubscribed from topic.\n";
            }
		}
		else if (FD_ISSET(fd, &fdsTmp))
		{
            recv(fd, &udpSockaddr, sizeof(udpSockaddr), 0);
            recv(fd, &messageSize, sizeof(messageSize), 0);
            recv(fd, &mUDP, messageSize, 0);

			cout << inet_ntoa(udpSockaddr.sin_addr) << ":" << ntohs(udpSockaddr.sin_port) << " - " << mUDP.topic;

            if (!messageSize) {
                close(fd);
                return 0;
            }

			if (mUDP.type == 3)
			{
                cout << " - STRING - ";
				cout << mUDP.payload << "\n";
				continue;
			}

            if (mUDP.type == 0)
			{
                uint32_t a;
                char* s = mUDP.payload + 1;

                memcpy(&a, s, sizeof(uint32_t));
				cout << " - INT - " << (mUDP.payload[0] == 1 ? "-" : "");
                cout << ntohl(a) << "\n";
				continue;
            }

			if (mUDP.type == 2) {
				char* s = mUDP.payload + 1;
				float no;

				uint8_t b;
                uint32_t a;

                memcpy(&a, s, sizeof(uint32_t));
                memcpy(&b, s + sizeof(uint32_t), sizeof(uint8_t));
				cout << " - FLOAT - " << (mUDP.payload[0] == 1 ? "-" : "");
				no = ntohl(a) * 1.0 / pow(10, (int)b);
                cout << fixed << setprecision((int)b) << no << "\n";
				continue;
            }
			
			if (mUDP.type == 1) {
                uint16_t a;
				float no;

                memcpy(&a, mUDP.payload, sizeof(uint16_t));
				no = ntohs(a) * 1.0 / 100;
                cout << " - SHORT_REAL - " << fixed << setprecision(2) << no << "\n";
            }
		}
    }

    close(fd);
    return 0;
}
