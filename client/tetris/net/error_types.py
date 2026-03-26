SERVER_ERROR = 1
INVALID_REQUEST = 2

ROOM_NOT_FOUND = 10
ROOM_FULL = 11
ROOM_INGAME = 12

ROOM_NOT_ALL_READY = 20
ROOM_NOT_ENOUGH_PLAYERS = 21
ROOM_INVALID_PASSWORD = 22

ERROR_MESSAGES = {
    SERVER_ERROR: "서버와의 상태가 원활하지 않습니다.",
    INVALID_REQUEST: "잘못된 요청입니다.",
    
    ROOM_NOT_FOUND: "존재하지 않는 방입니다.",
    ROOM_FULL: "방이 꽉 찼습니다.",
    ROOM_INGAME: "이미 시작된 방입니다.",

    ROOM_NOT_ALL_READY: "모두 준비해야 시작할 수 있습니다.",
    ROOM_NOT_ENOUGH_PLAYERS: "최소 2명이 모여야 시작할 수 있습니다.",
    ROOM_INVALID_PASSWORD: "방 비밀번호와 일치하지 않습니다."
}