#include <iostream>
#include "Session.h"

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
    ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ClientSocket == INVALID_SOCKET) {
        std::cout << "소켓 생성 실패: " << WSAGetLastError() << "\n";
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(ServerPort);
    serverAddr.sin_addr.s_addr = inet_addr(ServerIP);

    // 서버에 연결 시도
    iResult = connect(ClientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    if (iResult == SOCKET_ERROR) {
        std::cout << "서버 연결 실패: " << WSAGetLastError() << "\n";
        closesocket(ClientSocket);
        WSACleanup();
        return false;
    }

    std::cout << "서버에 성공적으로 연결되었습니다!\n";
    return true;
}

void Session::Disconnect()
{
    closesocket(ClientSocket);
    WSACleanup();
    std::cout << "연결이 종료되었습니다." << std::endl;
}

void Session::ProcessRecvPacket(char* packet)
{
    
}

void Session::ProcessSendPacket(std::string message)
{
    int length = message.length(); // NULL 문자 제외한 길이
    const char* m = message.c_str(); // 데이터 전송을 위해 string -> const char로 변환

    int result = send(ClientSocket, m, length, 0);
    if (result == SOCKET_ERROR) {
        std::cout << "데이터 전송 실패: " << WSAGetLastError() << "\n";
    }
}

void Session::MergePacket(char* packet)
{
    // 추후 작성
}
