#pragma comment(lib, "Ws2_32.lib")
#include<iostream>
#include<WinSock2.h>
#include<string>
#include<string.h>
#include<time.h>
#include<fstream>
#include<stdio.h>
#include<vector>
#include<thread>
#include<mutex>
using namespace std;
#define TIMEOUT 500
#define SENDSUCCESS true
#define SENDFAIL false
#define CONNECTSUCCESS true
#define CONNECTFAIL false
#define SEQ1 '1'
#define ACK1 '#'
#define SEQ2 '3'
#define ACK2 SEQ1 + 1
#define SEQ3 '5'
#define ACK3 SEQ2 + 1
#define WAVE1 '7'
#define ACKW1 '#'
#define WAVE2 '9'
#define ACKW2 WAVE1 + 1
#define LAST '$'
#define NOTLAST '@'
#define LENGTH 16378
#define CheckSum 6
#define ACKMsg '%'
#define NAK '^'
SOCKET client;
SOCKADDR_IN serverAddr, clientAddr;
char message[200000000];
int filelen = 0;
int package_num;
int cwnd = 0;
int Maxcwnd = 6;
int acknum = 0;
thread recvd;
mutex mtx, mtx1;
unsigned char PkgCheck(char *arr, int len)
{
	if (len == 0)
		return ~(0);
	unsigned char ret = arr[0];
	for (int i = 1; i < len; i++)
	{
		unsigned int tmp = ret + (unsigned char)arr[i];
		tmp = tmp / (1 << 8) + tmp % (1 << 8);
		tmp = tmp / (1 << 8) + tmp % (1 << 8);
		ret = tmp;
	}
	return ~ret;
}
int StartClient()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		cout << "WSAStartup Error:" << WSAGetLastError() << endl;
		return -1;
	}
	client = socket(AF_INET, SOCK_DGRAM, 0);

	if (client == SOCKET_ERROR)
	{
		cout << "Socket Error:" << WSAGetLastError() << endl;
		return -1;
	}
	int Port = 1439;
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(Port);

	cout << "Start Client Success!" << endl;
	return 1;
}
void ConnectToServer()
{
	while (1)
	{
		char send[2];
		send[0] = SEQ1;
		send[1] = ACK1;
		sendto(client, send, 2, 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
		cout << "Client sent first handshake." << endl;
		int begin_time = clock();
		bool flag = SENDSUCCESS;
		char recv[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(client, recv, 2, 0, (sockaddr *)&serverAddr, &clientlen) == SOCKET_ERROR)
		{
			int over_time = clock();
			if (over_time - begin_time > TIMEOUT)
			{
				flag = SENDFAIL;
				break;
			}
		}
		if (flag && recv[0] == SEQ2 && recv[1] == ACK2)
		{
			cout << "Client received second handshake." << endl;
			send[0] = SEQ3;
			send[1] = ACK3;
			sendto(client, send, 2, 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
			cout << "Client sent third handshake." << endl;
			break;
		}
	}
}
void CloseClient(SOCKET client)
{
	while (1)
	{
		char send[2];
		send[0] = WAVE1;
		send[1] = ACKW1;
		sendto(client, send, 2, 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
		cout << "Client waved first hand." << endl;
		int begin_time = clock();
		bool flag = SENDSUCCESS;
		char recv[2];
		int clientlen = sizeof(clientAddr);
		while (recvfrom(client, recv, 2, 0, (sockaddr *)&serverAddr, &clientlen) == SOCKET_ERROR)
		{
			int over_time = clock();
			if (over_time - begin_time > TIMEOUT)
			{
				flag = SENDFAIL;
				break;
			}
		}
		if (flag && recv[0] == WAVE2 && recv[1] == ACKW2)
		{
			break;
		}
	}
}
void SendName(string filename, int size)
{
	char *name = new char[size + 1];
	for (int i = 0; i < size; i++)
	{
		name[i] = filename[i];
	}
	name[size] = '$';
	sendto(client, name, size + 1, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
	delete name;
}
void LoadMessage(string filename)
{
	if (filename == "quit")
	{
		return;
	}
	else
	{
		ifstream fin(filename.c_str(), ifstream::binary);
		if (!fin)
		{
			cout << "This file disappeared!" << endl;
			return;
		}
		unsigned char ch = fin.get();
		while (fin)
		{
			message[filelen] = ch;
			filelen++;
			ch = fin.get();
		}
		fin.close();
	}
	package_num = filelen / LENGTH + (filelen % LENGTH != 0);
	cout << package_num << endl;
}
void Sendpackage(char* msg, int len, int index, int last)
{
	char *buffer = new char[len + CheckSum];
	if (last)
	{
		buffer[1] = LAST;
	}
	else
	{
		buffer[1] = NOTLAST;
	}
	buffer[2] = index / 128;
	buffer[3] = index % 128;
	buffer[4] = len / 128;
	buffer[5] = len % 128;
	for (int i = 0; i < len; i++)
	{
		buffer[i + CheckSum] = msg[i];
	}
	buffer[0] = PkgCheck(buffer + 1, len + CheckSum - 1);
	sendto(client, buffer, len + CheckSum, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
}
void Recvmessage(SOCKET client, int begin_time)
{
	while (1)
	{
		int clientlen = sizeof(clientAddr);
		bool flag = true;
		char recv[3];
		while (recvfrom(client, recv, 3, 0, (sockaddr *)&serverAddr, &clientlen) == SOCKET_ERROR)
		{
			int end_time = clock();
			if (end_time - begin_time > TIMEOUT)
			{
				flag = false;
				//printf("Time out.\n");
				break;
			}
		}
		if (flag && recv[0] == ACKMsg)
		{
			printf("ack%d\n", recv[1] * 128 + recv[2]);
			acknum++;
		}
		else if (!flag)
		{
			break;
		}
	}
}
void Sendmessage()
{
	int status = 0;
	// 0 ÂýÆô¶¯ 1 ÓµÈû
	int index = 0;

	
	while (1)
	{
		if (cwnd == 0)
		{
			cwnd = 1;
		}
		else if (cwnd < Maxcwnd && status == 0)
		{
			if (cwnd * 2 > Maxcwnd)
			{
				status = 1;
				cwnd++;
			}
			else
			{
				cwnd *= 2;
			}
		}
		else if(status == 1)
		{
			cwnd++;
		}
		printf("cwnd:%d\n", cwnd);
		int i;
		for (i = 0; i < cwnd && i + index < package_num; i++)
		{
			//printf("%d", i + index);
			Sendpackage(message + (i + index) * LENGTH
				, (i + index) == package_num - 1 ? filelen - (package_num - 1)*LENGTH : LENGTH
				, (i + index), (i + index) == package_num - 1);
		}
		printf("send%d\n", i + index - 1);
		int begin_time = clock();
		
		Recvmessage(client, begin_time);
		//printf("\n");
		
		if (acknum == cwnd)
		{
			acknum = 0;
			index += cwnd;
		}
		else
		{
			//index += cwnd - acknum - 1;
			acknum = 0;
			cwnd = 0;
			status = 0;
			if (i + index >= package_num)
				return;
		}
	}
}
void Sendover()
{
	char send[3];
	send[0] = 'f';
	send[1] = 'i';
	send[2] = 'n';
	sendto(client, send, 3, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
}
int main()
{
	StartClient();
	cout << "Connecting to Server..." << endl;
	ConnectToServer();
	cout << "Connect Successfully" << endl;
	int time_out = 1;
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));
	string filename;
	cin >> filename;
	Sleep(100);
	SendName(filename, filename.length());
	
	LoadMessage(filename);
	Sendmessage();

	Sendover();
	cout << "Send Sucessfully!" << endl;
	CloseClient(client);
	WSACleanup();
	return 0;
}