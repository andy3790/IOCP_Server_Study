#include <iostream>
#include <WS2tcpip.h>
#pragma comment (lib, "WS2_32.LIB")

const char* SERVER_ADDR = "127.0.0.1";
const short SERVER_PORT = 4000;
const int BUF_SIZE = 256;

int main()
{
	std::wcout.imbue(std::locale("korean"));

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);
	
	SOCKET server_s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, 0);

	SOCKADDR_IN server_addr;
	ZeroMemory(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr);

	connect(server_s, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));

	while (true) {
		char buf[BUF_SIZE];
		std::cout << "Enter Message : ";
		std::cin.getline(buf, BUF_SIZE);

		DWORD sent_byte;
		WSABUF mybuf;
		mybuf.buf = buf;
		mybuf.len = static_cast<ULONG>(strlen(buf)) + 1;

		WSASend(server_s, &mybuf, 1, &sent_byte, 0, 0, 0);
		std::cout << "\t\tSent to Server [" << sent_byte << " bytes]" << std::endl;

		char recv_buf[BUF_SIZE];
		WSABUF mybuf_r;
		mybuf_r.buf = recv_buf;
		mybuf_r.len = BUF_SIZE;

		DWORD recv_byte;
		DWORD recv_flag = 0;

		WSARecv(server_s, &mybuf_r, 1, &recv_byte, &recv_flag, 0, 0);
		std::cout << "Server Sent [" << recv_byte << " bytes] : " << recv_buf << std::endl;
		std::cout << std::endl;
	}

	WSACleanup();
}