#include <iostream>
#include <WS2tcpip.h>
#pragma comment(lib, "WS2_32.LIB")


using namespace std;
constexpr short  PORT_NUM = 4000;
constexpr int BUF_SIZE = 256;

SOCKET client;
WSAOVERLAPPED c_over;
WSABUF c_wsabuf[1];
CHAR c_mess[BUF_SIZE];

void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flage);
void do_recv()
{
	c_wsabuf[0].buf = c_mess;
	c_wsabuf[0].len = BUF_SIZE;
	DWORD recv_flag = 0;
	memset(&c_over, 0, sizeof(c_over));
	WSARecv(client, c_wsabuf, 1, 0, &recv_flag, &c_over, recv_callback);
}

void CALLBACK send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flags)
{
	do_recv();
}

void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flage)
{
	if (0 == num_bytes) return;
	cout << "Client sent : " << c_mess << endl;
	c_wsabuf[0].len = num_bytes;
	memset(&c_over, 0, sizeof(c_over));
	WSASend(client, c_wsabuf, 1, 0, 0, &c_over, send_callback);
}



int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	SOCKET server = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(server, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(server, SOMAXCONN);

	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);

	client = WSAAccept(server, reinterpret_cast<sockaddr*>(&cl_addr), &addr_size, NULL, NULL);

	if (client == INVALID_SOCKET) {
		cout << "WSAAccept failed with error: " << WSAGetLastError() << endl;
	}
	else { // 접속 성공시 클라이언트 정보 출력
		char clientIP[INET_ADDRSTRLEN];
		InetNtopA(AF_INET, &cl_addr.sin_addr, clientIP, sizeof(clientIP));

		cout << "Client connected: " << clientIP << ":" << ntohs(cl_addr.sin_port) << endl;
	}

	do_recv();
	while (true) SleepEx(100, true);
	closesocket(server);
	WSACleanup();
}