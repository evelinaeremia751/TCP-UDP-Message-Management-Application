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

struct UnreadMessage {
	MUDP mUDP;
	int messageSize;
	struct sockaddr_in udpSockaddr;
};

struct Client {
	string id;
	bool online;
	int tcpFd;

	unordered_set<string> subscriptionsNoSf;
	unordered_set<string> subscriptionsWithSf;
	vector<UnreadMessage> unreadMessages;
};

char buff[MAX_SIZE];
fd_set fds, fdsTmp;
Socket tcpFd, udpFd, maxFd;
int ret, flag = 1;
MUDP mUDP;
struct sockaddr_in serverSockaddr, clientSockaddr, udpSockaddr;
unordered_map<string, Client*> clients;

int getMessageSize(MUDP& mUDP)
{
	int size = 51;

	if (!mUDP.type)
		size += 5;
	else {
		if (mUDP.type == 2)
			size += 7;
		else if (mUDP.type == 1)
			size += 2;
		else if (mUDP.type == 3)
			size += strlen(mUDP.payload);
	}

	return size;
}

int getFdType(fd_set& fds, Socket tcpFd, Socket udpFd, Socket curr)
{
	if (curr == tcpFd)
		if (FD_ISSET(tcpFd, &fds))
			return TCP_NEW_CONNECTION;

	if (curr == udpFd)
		if (FD_ISSET(udpFd, &fds))
			return UDP_CONNECTION;

	if (curr == STDIN_FILENO)
		if (FD_ISSET(STDIN_FILENO, &fds))
			return KEYBOARD_INPUT;

	if (FD_ISSET(curr, &fds))
		return TCP_EXISTING_CONNECTION;

	return NONE;
}

void tcpNewConnection()
{
	int ret;
    socklen_t size = sizeof(clientSockaddr);
    Socket newFd = accept(tcpFd, (struct sockaddr*)&clientSockaddr, &size);
	bool stop = false;

    FD_SET(newFd, &fds);
    memset(buff, '\0', MAX_SIZE);

	ret = recv(newFd, buff, sizeof(buff), 0);
    if (ret < 0) exit(1);

    if (clients[buff])
	{
        if (!clients[buff]->online)
		{
            clients[buff]->online = true;
            clients[buff]->tcpFd = newFd;

			cout << "New client " << buff << " connected from ";
    		cout << inet_ntoa(clientSockaddr.sin_addr) << ":";
        	cout << ntohs(clientSockaddr.sin_port) << ".\n";

            ret = send(newFd, &stop, sizeof(stop), 0);
            if (ret < 0) exit(1);

			for (const auto& unreadMessage : clients[buff]->unreadMessages)
			{
                ret = send(newFd, &unreadMessage.udpSockaddr, sizeof(unreadMessage.udpSockaddr), 0);
			    if (ret < 0) exit(1);

				ret = send(newFd, &unreadMessage.messageSize, sizeof(unreadMessage.messageSize), 0);
			    if (ret < 0) exit(1);

                ret = send(newFd, &unreadMessage.mUDP, sizeof(unreadMessage.mUDP), 0);
			    if (ret < 0) exit(1);
			}

			clients[buff]->unreadMessages.clear();
        } else
		{
            cout << "Client " << buff << " already connected.\n";
            stop = true;

            ret = send(newFd, &stop, sizeof(stop), 0);
            if (ret < 0) exit(1);
        }
    } else {
        Client* client = new Client;

        client->id = string(buff);
        client->online = true;
        client->tcpFd = newFd;
        clients[buff] = client;

        cout << "New client " << buff << " connected from ";
    	cout << inet_ntoa(clientSockaddr.sin_addr) << ":";
        cout << ntohs(clientSockaddr.sin_port) << ".\n";

        ret = send(newFd, &stop, sizeof(stop), 0);
        if (ret < 0) exit(1);
	}

    maxFd = max(maxFd, newFd);
}

void tcpExistingConnection(Socket curr)
{
	int ret;
	Client* client;
	MTCP mTCP{};

    ret = recv(curr, &mTCP, sizeof(mTCP), 0);
    if (ret < 0) exit(1);

	client = clients[mTCP.id];

	if (!mTCP.disconnect && !mTCP.subscribe)
	{
		if (client->subscriptionsNoSf.find(mTCP.topic) != client->subscriptionsNoSf.end())
			client->subscriptionsNoSf.erase(mTCP.topic);

		if (client->subscriptionsWithSf.find(mTCP.topic) != client->subscriptionsWithSf.end())
			client->subscriptionsWithSf.erase(mTCP.topic);

		return;
	}

	if (mTCP.subscribe && mTCP.sf)
	{
		client->subscriptionsWithSf.insert(mTCP.topic);
		return;
	}

	if (mTCP.subscribe)
	{
		client->subscriptionsNoSf.insert(mTCP.topic);
		return;
	}

	if (mTCP.disconnect)
	{
		FD_CLR(curr, &fds);
        clients[mTCP.id]->online = false;
        cout << "Client " << mTCP.id << " disconnected.\n";
	}
}

void udpConnection()
{
    socklen_t size = sizeof(udpSockaddr);
    int messageSize;

	memset(&mUDP, '\0', sizeof(mUDP));
	memset(&udpSockaddr, '\0', sizeof(udpSockaddr));
    ret = recvfrom(udpFd, &mUDP, sizeof(mUDP), 0, (struct sockaddr*)&udpSockaddr, &size);
	if (ret < 0) exit(1);

	messageSize = getMessageSize(mUDP);

	for (auto& entry : clients) {
		Client* client = entry.second;

		if (client->online)
		{
			if (client->subscriptionsNoSf.find(mUDP.topic) != client->subscriptionsNoSf.end() ||
				client->subscriptionsWithSf.find(mUDP.topic) != client->subscriptionsWithSf.end()) {
				ret = send(client->tcpFd, &udpSockaddr, sizeof(udpSockaddr), 0);
				if (ret < 0) exit(1);

            	ret = send(client->tcpFd, &messageSize, sizeof(messageSize), 0);
				if (ret < 0) exit(1);

            	ret = send(client->tcpFd, &mUDP, messageSize, 0);
				if (ret < 0) exit(1);
			}
		}
		else if (client->subscriptionsWithSf.find(mUDP.topic) != client->subscriptionsWithSf.end())
		{
			UnreadMessage unreadMessage {};

			unreadMessage.messageSize = messageSize;
			unreadMessage.mUDP = mUDP;
			unreadMessage.udpSockaddr = udpSockaddr;
			client->unreadMessages.push_back(unreadMessage);
		}
	}
}

int keyboardInput()
{
	int ret;

    memset(buff, '\0', MAX_SIZE);
	fgets(buff, MAX_SIZE, stdin);

    if (strstr(buff, "exit") == buff)
	{
        for (const auto& entry : clients) {
			Client* client = entry.second;
            int messageSize = 0;

        	ret = send(client->tcpFd, &udpSockaddr, sizeof(udpSockaddr), 0);
			if (ret < 0) exit(1);

            ret = send(client->tcpFd, &messageSize, sizeof(messageSize), 0);
			if (ret < 0) exit(1);

            ret = send(client->tcpFd, &mUDP, messageSize, 0);
			if (ret < 0) exit(1);
        }

        return 1;
    }

	return 0;
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	serverSockaddr.sin_addr.s_addr = INADDR_ANY;
    serverSockaddr.sin_port = htons(atoi(argv[1]));
	serverSockaddr.sin_family = AF_INET;

    tcpFd = socket(AF_INET, SOCK_STREAM, 0);
	udpFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (tcpFd < 0 || udpFd < 0) exit(1);

	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	FD_SET(udpFd, &fds);
    FD_SET(tcpFd, &fds);

	maxFd = max(tcpFd, udpFd);
    setsockopt(tcpFd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
	bind(udpFd, (struct sockaddr*)&serverSockaddr, sizeof(serverSockaddr));
    bind(tcpFd, (struct sockaddr*)&serverSockaddr, sizeof(serverSockaddr));
	listen(tcpFd, MAX_NO);

    for (;;) {
        FD_ZERO(&fdsTmp);
        fdsTmp = fds;
        select(maxFd + 1, &fdsTmp, NULL, NULL, NULL);

        for (Socket curr = 0; curr <= maxFd; curr++)
		{
			int fdType = getFdType(fdsTmp, tcpFd, udpFd, curr);

            if (fdType == TCP_NEW_CONNECTION) {
                tcpNewConnection();
				continue;
            }

            if (fdType == UDP_CONNECTION) {
                udpConnection();
                continue;
            }
			
			if (fdType == TCP_EXISTING_CONNECTION) {
                tcpExistingConnection(curr);
				continue;
			}

			if (fdType == KEYBOARD_INPUT)
                if (keyboardInput())
					return 0;
        }
    }

    return 0;
}
