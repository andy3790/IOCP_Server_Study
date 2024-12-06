#include <iostream>
#include <unordered_map>

#include <WS2tcpip.h>
#include <MSWSock.h>
#pragma comment(lib, "WS2_32.LIB")
#pragma comment(lib, "MSWSock.lib")

using namespace std;
constexpr short PORT_NUM = 4000;
constexpr int BUF_SIZE = 256;

HANDLE g_h_iocp; // 글로벌 IOCP 핸들 | 한개의 IOCP만이 존재하고 이걸로 모든 연결을 통제한다.

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND }; // OVERLAPPED 구조체가 행할 작업을 구분하기 위해 사용한다.
// switch 문을 사용하려면 enum class 가 아닌 enum으로 해서 정수 추리를 허용해야한다.

class EXP_OVER {
public:
	WSAOVERLAPPED _wsa_over;
	WSABUF _wsa_buf[1];
	char _send_buf[BUF_SIZE];
	COMP_TYPE _comp_type; // 이 OVERLAPPED 구조체가 행할 작업
	SOCKET _client_socket; // AcceptEX 를 통해 연결 소켓을 전달하기 위해 추가

public:
	EXP_OVER() {
		ZeroMemory(&_wsa_over, sizeof(_wsa_over));
		_wsa_buf[0].buf = _send_buf;
		_wsa_buf[0].len = BUF_SIZE;
	}

	EXP_OVER(size_t s_id, char num_bytes, char* mess) {
		ZeroMemory(&_wsa_over, sizeof(_wsa_over));
		_wsa_buf[0].buf = _send_buf;
		_wsa_buf[0].len = num_bytes + 2; // 헤더 2바이트를 고려하여 총 바이트 수에 2를 더함

		memcpy(_send_buf + 2, mess, num_bytes);

		// 보낼 메세지에 헤더를 붙임
		// 헤더의 크기가 2
		// 0번째 헤더에 보낼 총 바이트 수가 적혀있음
		// 1번째 헤더에 보내는 클라이언트의 id 가 적혀있음
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
	SESSION() { // 초기화 없이 생성될 수 없는 구조체
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
	// 위의 코드와 완벽히 동일하게 작동한다.
	// IPPROTO_TCP 자리에 0을 넣을 경우 WSASocket 함수가 SOCK_STREAM을 보고 IPPROTO_TCP를 추론하도록 작동한다.
	// 이 작동은 SOCK_DGRAM 이면 IPPROTO_UDP로 추론하는것과 같은 동작이다.
	// 해당 부분에 0을 집어넣음으로 코드가 TCP인지 UDP인지 추론하도록 하여 동적으로 프로토콜이 변해야할때 유리하다.

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

	// IOCP 준비
	g_h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0); // IOCP 객체 생성
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(s_socket), g_h_iocp, 9999, 0);
	// 9999 라는 숫자는 딱히 의미가 없다.
	// 해당 위치에는 해당 작업을 사용자가 구분할 수 있는 키값을 입력하여야하는데
	// 서버 소켓을 IOCP에 연결하는 작업은 비동기적으로 일어나는 것이 아니므로 정말 아무숫자나 상관이 없다.
	// 해당 숫자는 GetQueuedCompletionStatus를 통해서 완료 상태를 불러왔을 때 완료된 작업과 함께 리턴된다.
	// 그러므로 우리는 키값을 보고 이 작업이 어떤작업이였는지를 판단할 수 있다.
	// -------------------------------------------------------------------
	// 해당 숫자는 소켓의 연결번호로 생각하여도 좋을 듯 하다. 지금 서버 소켓을 리슨소켓으로 9999번에 연결했다 보아도 무방할듯 하다.
	// 이때 문제. 클라이언트가 9999 명 진입하게 되면 같은 번호로 등록시도를 하게 되어서 문제가 될 수도 있을 듯 하다.
	// 리슨 소켓은 충분히 큰 숫자로 연결하는 편이 좋을 듯 하다.

	SOCKET c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED); // IOCP를 위해 먼저 클라이언트 소켓을 만든다.
	EXP_OVER a_over;
	a_over._comp_type = OP_ACCEPT;
	a_over._client_socket = c_socket;
	AcceptEx(s_socket, c_socket, a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &a_over._wsa_over);

	while(true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		// 위의 3개의 변수는 매번 재사용되는 변수이며 while문의 반복과 함께 생성과 삭제를 반복한다.
		// 그러나 변수의 생성과 삭제는 비용이 작은 작업이고 IOCP에서의 작업은 안정성이 중요하기 때문에
		// 데이터의 오염이나 작업의 구분을 위해서라도 스코프 내에 변수를 선언함이 좋다.

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
				// 오류를 일으킨 클라이언트의 연결을 끊어버리자.
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