#include <iostream>
#include <WS2tcpip.h>
#include <unordered_map>
#pragma comment(lib, "WS2_32.LIB")

using namespace std;
constexpr short  PORT_NUM = 4000;
constexpr int BUF_SIZE = 256;

void CALLBACK send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED send_over, DWORD recv_flags);
void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED recv_over, DWORD recv_flage);

class SESSION {
private:
	int _id;
	WSABUF _recv_wsabuf;
	WSABUF _send_wsabuf;
	WSAOVERLAPPED _recv_over;
	SOCKET _socket;

public:
	char _recv_buf[BUF_SIZE];

public:
	SESSION() { // 초기화 없이 생성될 수 없는 구조체
		cout << "Unexpected Constructor Call Error!\n";
		exit(-1);
	}
	SESSION(int id, SOCKET s) : _id(id), _socket(s) {
		_recv_wsabuf.buf = _recv_buf;
		_recv_wsabuf.len = BUF_SIZE;

		_send_wsabuf.buf = _recv_buf; // 이 서버는 echo 서버이므로 받은 정보와 보낼정보가 동일하다.
		_send_wsabuf.len = 0;
	}
	~SESSION() {
		closesocket(_socket);
	}

public:
	void do_recv() {
		DWORD recv_flag = 0;
		ZeroMemory(&_recv_over, sizeof(_recv_over));
		_recv_over.hEvent = reinterpret_cast<HANDLE>(_id); // CallBack 함수에서 불린 소켓을 알기 위해 over의 이벤트에 id를 적어보낸다.
		WSARecv(_socket, &_recv_wsabuf, 1, 0, &recv_flag, &_recv_over, recv_callback);
	}

	void do_send(int num_bytes) {
		ZeroMemory(&_recv_over, sizeof(_recv_over));
		_recv_over.hEvent = reinterpret_cast<HANDLE>(_id); // CallBack 함수에서 불린 소켓을 알기 위해 over의 이벤트에 id를 적어보낸다.
		_send_wsabuf.len = num_bytes;
		WSASend(_socket, &_send_wsabuf, 1, 0, 0, &_recv_over, send_callback);
	}
};


unordered_map<int, SESSION> clients;

void CALLBACK send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED send_over, DWORD recv_flags)
{
	int s_id = reinterpret_cast<int>(send_over->hEvent); // hEvent값을 int형으로 캐스팅하면 클라이언트의 id를 얻을 수 있다.
	clients[s_id].do_recv();
}

void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED recv_over, DWORD recv_flage)
{
	int s_id = reinterpret_cast<int>(recv_over->hEvent); // hEvent값을 int형으로 캐스팅하면 클라이언트의 id를 얻을 수 있다.
	cout << "Client Sent [" << num_bytes << " bytes] : " << clients[s_id]._recv_buf << endl;
	clients[s_id].do_send(num_bytes);
}



int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN server_addr;
	ZeroMemory(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	bind(s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(s_socket, SOMAXCONN);

	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);
	for (int i = 1; ; ++i) {
		SOCKET c_socket = WSAAccept(s_socket, reinterpret_cast<sockaddr*>(&cl_addr), &addr_size, 0, 0);
		
		if (c_socket == INVALID_SOCKET) {
			cout << "WSAAccept failed with error: " << WSAGetLastError() << endl;
		}
		else { // 접속 성공시 클라이언트 정보 출력
			char clientIP[INET_ADDRSTRLEN];
			InetNtopA(AF_INET, &cl_addr.sin_addr, clientIP, sizeof(clientIP));

			cout << "Client connected: " << clientIP << ":" << ntohs(cl_addr.sin_port) << endl;
		}

		clients.try_emplace(i, i, c_socket);
		clients[i].do_recv();
	}

	clients.clear();
	closesocket(s_socket);
	WSACleanup();
}