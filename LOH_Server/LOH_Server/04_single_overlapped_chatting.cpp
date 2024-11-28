#include <iostream>
#include <WS2tcpip.h>
#include <unordered_map>
#pragma comment(lib, "WS2_32.LIB")

using namespace std;
constexpr short  PORT_NUM = 4000;
constexpr int BUF_SIZE = 256;

void CALLBACK send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED send_over, DWORD recv_flags);
void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED recv_over, DWORD recv_flage);

class EXP_OVER {
public:
	WSAOVERLAPPED _wsa_over;
	size_t _s_id;
	WSABUF _wsa_buf;
	char _send_msg[BUF_SIZE];

public:
	EXP_OVER(size_t s_id, char num_bytes, char* mess) : _s_id(s_id) {
		ZeroMemory(&_wsa_over, sizeof(_wsa_over));
		_wsa_buf.buf = _send_msg;
		_wsa_buf.len = num_bytes + 2; // ��� 2����Ʈ�� ����Ͽ� �� ����Ʈ ���� 2�� ����

		memcpy(_send_msg + 2, mess, num_bytes);

		// ���� �޼����� ����� ����
		// ����� ũ�Ⱑ 2
		// 0��° ����� ���� �� ����Ʈ ���� ��������
		// 1��° ����� ������ Ŭ���̾�Ʈ�� id �� ��������
		_send_msg[0] = num_bytes + 2;
		_send_msg[1] = s_id;
	}
	~EXP_OVER() {}
};

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
	SESSION() { // �ʱ�ȭ ���� ������ �� ���� ����ü
		cout << "Unexpected Constructor Call Error!\n";
		exit(-1);
	}
	SESSION(int id, SOCKET s) : _id(id), _socket(s) {
		_recv_wsabuf.buf = _recv_buf;
		_recv_wsabuf.len = BUF_SIZE;

		_send_wsabuf.buf = _recv_buf; // �� ������ echo �����̹Ƿ� ���� ������ ���������� �����ϴ�.
		_send_wsabuf.len = 0;
	}
	~SESSION() {
		closesocket(_socket);
	}

public:
	void do_recv() {
		DWORD recv_flag = 0;
		ZeroMemory(&_recv_over, sizeof(_recv_over));
		_recv_over.hEvent = reinterpret_cast<HANDLE>(_id); // CallBack �Լ����� �Ҹ� ������ �˱� ���� over�� �̺�Ʈ�� id�� �������.
		WSARecv(_socket, &_recv_wsabuf, 1, 0, &recv_flag, &_recv_over, recv_callback);
	}

	void do_send(int sender_id, int num_bytes, char* mess) {
		EXP_OVER* ex_over = new EXP_OVER(sender_id, num_bytes, mess);
		WSASend(_socket, &ex_over->_wsa_buf, 1, 0, 0, &ex_over->_wsa_over, send_callback);
	}
};


unordered_map<int, SESSION> clients;

void CALLBACK send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED send_over, DWORD recv_flags)
{
	EXP_OVER* ex_over = reinterpret_cast<EXP_OVER*>(send_over);
	delete ex_over;
}

void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED recv_over, DWORD recv_flage)
{
	unsigned long long s_id = reinterpret_cast<unsigned long long>(recv_over->hEvent);
	cout << "Client [" << s_id << "] Sent[" << num_bytes << " bytes] : " << clients[s_id]._recv_buf << endl;
	for (auto& cl : clients)
		cl.second.do_send(s_id, num_bytes, clients[s_id]._recv_buf);
	clients[s_id].do_recv();
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
		else { // ���� ������ Ŭ���̾�Ʈ ���� ���
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