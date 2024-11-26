#include <iostream>
#include <WS2tcpip.h>
#include <string>
#pragma comment (lib, "WS2_32.LIB")

constexpr short SERVER_PORT = 4000;
constexpr int BUF_SIZE = 256;

void error_display(const char* msg, int err_no)
{
	WCHAR* lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::cout << msg;
	std::wcout << L"¿¡·¯ " << lpMsgBuf << std::endl;
	while (true);
	LocalFree(lpMsgBuf);
}

int main()
{
	std::wcout.imbue(std::locale("korean"));

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);

	SOCKET server_s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
	SOCKADDR_IN server_addr;
	ZeroMemory(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if (SOCKET_ERROR == ::bind(server_s, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr))) {
		error_display("WSARecv Error :", WSAGetLastError());
	}
	listen(server_s, SOMAXCONN);

	int addr_size = sizeof(server_addr);
	SOCKET client_s = WSAAccept(server_s, reinterpret_cast<sockaddr*>(&server_addr), &addr_size, 0, 0);

	while (true) {
		char recv_buf[BUF_SIZE];
		WSABUF mybuf;
		mybuf.buf = recv_buf;
		mybuf.len = BUF_SIZE;

		DWORD recv_byte;
		DWORD recv_flag = 0;
		WSARecv(client_s, &mybuf, 1, &recv_byte, &recv_flag, 0, 0);
		std::cout << "Client Sent [" << recv_byte << " bytes] : " << recv_buf << std::endl;

		DWORD sent_byte;
		mybuf.len = recv_byte;
		WSASend(client_s, &mybuf, 1, &sent_byte, 0, 0, 0);
		std::cout << "\t\tSent to Client [" << sent_byte << " bytes]" << std::endl;
	}
	WSACleanup();
}