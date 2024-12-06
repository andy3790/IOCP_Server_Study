#include <iostream>
#include <unordered_map>

#include <WS2tcpip.h>
#include <MSWSock.h>
#pragma comment(lib, "WS2_32.LIB")
#pragma comment(lib, "MSWSock.lib")

using namespace std;
constexpr short PORT_NUM = 4000;
constexpr int BUF_SIZE = 256;

HANDLE g_h_iocp; // �۷ι� IOCP �ڵ� | �Ѱ��� IOCP���� �����ϰ� �̰ɷ� ��� ������ �����Ѵ�.

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND }; // OVERLAPPED ����ü�� ���� �۾��� �����ϱ� ���� ����Ѵ�.
// switch ���� ����Ϸ��� enum class �� �ƴ� enum���� �ؼ� ���� �߸��� ����ؾ��Ѵ�.

class EXP_OVER {
public:
	WSAOVERLAPPED _wsa_over;
	WSABUF _wsa_buf[1];
	char _send_buf[BUF_SIZE];
	COMP_TYPE _comp_type; // �� OVERLAPPED ����ü�� ���� �۾�
	SOCKET _client_socket; // AcceptEX �� ���� ���� ������ �����ϱ� ���� �߰�

public:
	EXP_OVER() {
		ZeroMemory(&_wsa_over, sizeof(_wsa_over));
		_wsa_buf[0].buf = _send_buf;
		_wsa_buf[0].len = BUF_SIZE;
	}

	EXP_OVER(size_t s_id, char num_bytes, char* mess) {
		ZeroMemory(&_wsa_over, sizeof(_wsa_over));
		_wsa_buf[0].buf = _send_buf;
		_wsa_buf[0].len = num_bytes + 2; // ��� 2����Ʈ�� ����Ͽ� �� ����Ʈ ���� 2�� ����

		memcpy(_send_buf + 2, mess, num_bytes);

		// ���� �޼����� ����� ����
		// ����� ũ�Ⱑ 2
		// 0��° ����� ���� �� ����Ʈ ���� ��������
		// 1��° ����� ������ Ŭ���̾�Ʈ�� id �� ��������
		_send_buf[0] = num_bytes + 2;
		_send_buf[1] = s_id;
	}

	~EXP_OVER() {}
};

class SESSION {
private:
	EXP_OVER _recv_over;

public:
	int _id;
	SOCKET _socket;

public:
	SESSION() { // �ʱ�ȭ ���� ������ �� ���� ����ü
		cout << "Unexpected Constructor Call Error!\n";
		exit(-1);
	}
	SESSION(int id, SOCKET s) : _id(id), _socket(s) {

	}
	~SESSION() {
		closesocket(_socket);
	}

public:
	void do_recv() {
		DWORD recv_flag = 0;
		ZeroMemory(&_recv_over._wsa_over, sizeof(_recv_over._wsa_over));
		WSARecv(_socket, _recv_over._wsa_buf, 1, nullptr, &recv_flag, &_recv_over._wsa_over, nullptr);
	}

	void do_send(int sender_id, int num_bytes, char* mess) {
		EXP_OVER* ex_over = new EXP_OVER(sender_id, num_bytes, mess);
		WSASend(_socket, ex_over->_wsa_buf, 1, nullptr, 0, &ex_over->_wsa_over, nullptr);
	}
};


unordered_map<int, SESSION> clients;


int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	HANDLE h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	//SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
	// ���� �ڵ�� �Ϻ��� �����ϰ� �۵��Ѵ�.
	// IPPROTO_TCP �ڸ��� 0�� ���� ��� WSASocket �Լ��� SOCK_STREAM�� ���� IPPROTO_TCP�� �߷��ϵ��� �۵��Ѵ�.
	// �� �۵��� SOCK_DGRAM �̸� IPPROTO_UDP�� �߷��ϴ°Ͱ� ���� �����̴�.
	// �ش� �κп� 0�� ����������� �ڵ尡 TCP���� UDP���� �߷��ϵ��� �Ͽ� �������� ���������� ���ؾ��Ҷ� �����ϴ�.

	SOCKADDR_IN server_addr;
	ZeroMemory(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	bind(s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(s_socket, SOMAXCONN);

	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);

	int id = 0;

	// IOCP �غ�
	g_h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0); // IOCP ��ü ����
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(s_socket), g_h_iocp, 9999, 0);
	// 9999 ��� ���ڴ� ���� �ǹ̰� ����.
	// �ش� ��ġ���� �ش� �۾��� ����ڰ� ������ �� �ִ� Ű���� �Է��Ͽ����ϴµ�
	// ���� ������ IOCP�� �����ϴ� �۾��� �񵿱������� �Ͼ�� ���� �ƴϹǷ� ���� �ƹ����ڳ� ����� ����.
	// �ش� ���ڴ� GetQueuedCompletionStatus�� ���ؼ� �Ϸ� ���¸� �ҷ����� �� �Ϸ�� �۾��� �Բ� ���ϵȴ�.
	// �׷��Ƿ� �츮�� Ű���� ���� �� �۾��� ��۾��̿������� �Ǵ��� �� �ִ�.
	// -------------------------------------------------------------------
	// �ش� ���ڴ� ������ �����ȣ�� �����Ͽ��� ���� �� �ϴ�. ���� ���� ������ ������������ 9999���� �����ߴ� ���Ƶ� �����ҵ� �ϴ�.
	// �̶� ����. Ŭ���̾�Ʈ�� 9999 �� �����ϰ� �Ǹ� ���� ��ȣ�� ��Ͻõ��� �ϰ� �Ǿ ������ �� ���� ���� �� �ϴ�.
	// ���� ������ ����� ū ���ڷ� �����ϴ� ���� ���� �� �ϴ�.

	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED); // IOCP�� ���� ���� Ŭ���̾�Ʈ ������ �����.
	EXP_OVER a_over;
	a_over._comp_type = OP_ACCEPT;
	a_over._client_socket = c_socket;
	AcceptEx(s_socket, c_socket, a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &a_over._wsa_over);

	while(true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		// ���� 3���� ������ �Ź� ����Ǵ� �����̸� while���� �ݺ��� �Բ� ������ ������ �ݺ��Ѵ�.
		// �׷��� ������ ������ ������ ����� ���� �۾��̰� IOCP������ �۾��� �������� �߿��ϱ� ������
		// �������� �����̳� �۾��� ������ ���ؼ��� ������ ���� ������ �������� ����.

		BOOL ret = GetQueuedCompletionStatus(g_h_iocp, &num_bytes, &key, &over, INFINITE);
		EXP_OVER* ex_over = reinterpret_cast<EXP_OVER*>(over);
		if (FALSE == ret) {
			if (ex_over->_comp_type == OP_ACCEPT) {
				cout << "Accept Error";
				exit(-1);
			}
			else {
				cout << "GQCS Error on client[" << key << "]\n";
				closesocket(clients[key]._socket);
				clients.erase(key);
				// ������ ����Ų Ŭ���̾�Ʈ�� ������ ���������.
				if (ex_over->_comp_type == OP_SEND) delete ex_over;
				continue;
			}
		}

		switch (ex_over->_comp_type) {
		case OP_ACCEPT: {
			SOCKET nc_socket = ex_over->_client_socket;
			clients.try_emplace(id, id, nc_socket);
			CreateIoCompletionPort(reinterpret_cast<HANDLE>(nc_socket), g_h_iocp, id, 0);
			clients[id].do_recv();
			cout << "Accept Succes! Welcom Client [" << id << "]\n";
			id++;

			nc_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			ZeroMemory(&ex_over->_wsa_over, sizeof(ex_over->_wsa_over));
			ex_over->_client_socket = nc_socket;
			AcceptEx(s_socket, nc_socket, ex_over->_send_buf, 0, addr_size + 16, addr_size + 16, 0, &ex_over->_wsa_over);
			break;
		}
		case OP_RECV:
			break;
		case OP_SEND:
			break;
		}
	}

	clients.clear();
	closesocket(s_socket);
	WSACleanup();
}