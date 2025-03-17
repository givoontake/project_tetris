#include <iostream>
#include "Session.h"
#include "protocol.h"

Session::Session()
{

}

Session::~Session()
{
}

bool Session::ConnectToServer()
{
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSAStartup 실패: " << iResult << "\n";
        return false;
    }

    // 소켓 생성 (TCP/IP 소켓)
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cout << clientSocket << " 소켓 생성 실패: " << WSAGetLastError() << "\n";
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);

    // 서버에 연결 시도
    iResult = connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    if (iResult == SOCKET_ERROR) {
        std::cout << "서버 연결 실패: " << WSAGetLastError() << "\n";
        closesocket(clientSocket);
        WSACleanup();
        return false;
    }

    std::cout << "서버에 성공적으로 연결되었습니다!\n";
    return true;
}

bool Session::ConnectToLocalFlask()
{
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSAStartup 실패: " << iResult << "\n";
        return false;
    }

    // 소켓 생성 (TCP/IP 소켓)
    flaskSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (flaskSocket == INVALID_SOCKET) {
        std::cout << flaskSocket <<" 소켓 생성 실패: " << WSAGetLastError() << "\n";
        WSACleanup();
        return false;
    }

    sockaddr_in flaskAddr;
    flaskAddr.sin_family = AF_INET;
    flaskAddr.sin_port = htons(flaskPort);
    flaskAddr.sin_addr.s_addr = inet_addr(flaskIP);

    // 서버에 연결 시도
    iResult = connect(flaskSocket, (SOCKADDR*)&flaskAddr, sizeof(flaskAddr));
    if (iResult == SOCKET_ERROR) {
        std::cout << "flask 연결 실패: " << WSAGetLastError() << "\n";
        closesocket(flaskSocket);
        WSACleanup();
        return false;
    }

    std::cout << "flask에 성공적으로 연결되었습니다!\n";
    std::cout << "test data 전송\n";
    ProcessSendPacket("테스트 데이터입니다.", SEND_FLASK);
    return true;
}

void Session::Disconnect()
{
    closesocket(clientSocket);
    WSACleanup();
    std::cout << "연결이 종료되었습니다." << std::endl;
}

void Session::ProcessRecvPacket(char* packet)
{
    
}

void Session::MergePacket(int recv_bytes, char* recv_data)
{
    static int r_size = 0;
    static char r_buffer[BUFFER_SIZE];

    // 일단 새로 들어온 데이터를 뒤에 붙임

    if (r_size + recv_bytes > BUFFER_SIZE)
    {
        // 버퍼 오버플로우 방지
       /* std::cout << "Buffer overflow.." << std::endl;
        r_size = 0;
        ZeroMemory(r_buffer, sizeof(r_buffer));
        std::cout << "Buffer clear complete." << std::endl;*/
        Disconnect();
        std::cout << "버퍼 오버플로우로 인해 종료합니다." << std::endl;
    }

    if (recv_bytes > 0)
    {
        memcpy(r_buffer + r_size, recv_data, recv_bytes);
        r_size += recv_bytes;
    } 

    else {
        std::cout << "recv_data: 0, 프로그램 종료.." << std::endl;
        Disconnect();
    }

    // 남은 데이터에 패킷이 충분히 쌓였는지 확인하며 처리
    while (r_size >= r_buffer[0]) // 남아있는 데이터 크기가 실제 처리가능한 데이터 크기이상 존재한다면
    {
        char p_buffer[BUFFER_SIZE];
        // 패킷 분리: packet_buffer에 복사 후 처리
        memcpy(p_buffer, r_buffer, r_buffer[0]);
        ProcessRecvPacket(p_buffer);

        // 처리한 패킷은 남은 데이터에서 제거
        r_size -= r_buffer[0];
        memmove(r_buffer, r_buffer + r_buffer[0], r_size);
    }
}

void Session::ProcessSendPacket(std::string message, int send_type)
{
    switch (send_type) {
    case SEND_SERVER: {
        int length = message.length(); // NULL 문자 제외한 길이
        const char* m = message.c_str(); // 데이터 전송을 위해 string -> const char로 변환

        int result = send(clientSocket, m, length, 0);
        if (result == SOCKET_ERROR) {
            std::cout << "데이터 전송 실패: " << WSAGetLastError() << "\n";
        }
        break;
    }

    case SEND_FLASK: {
            // HTTP POST 요청 메시지 구성
            std::string httpPostRequest =
                u8"POST /receive HTTP/1.1\r\n"
                "Host: " + std::string(flaskIP) + ":" + std::to_string(flaskPort) + "\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: " + std::to_string(message.size()) + "\r\n\r\n" + message;

            std::cout << httpPostRequest << std::endl;

            // Flask로 데이터 전송
            int flaskResult = send(flaskSocket, httpPostRequest.c_str(), httpPostRequest.size(), 0);
            if (flaskResult == SOCKET_ERROR) {
                std::cerr << "Flask로 데이터 전송 실패: " << WSAGetLastError() << "\n";
            }
            break;
        }

    }
}
