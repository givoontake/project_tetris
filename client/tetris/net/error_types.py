SERVER_ERROR = 1
INVALID_REQUEST = 2
LOGIN_FAILED = 3
DUPLICATE_LOGIN_ID = 4

ROOM_NOT_FOUND = 10
ROOM_FULL = 11
ROOM_INGAME = 12
NOT_FOUND_JOINABLE_ROOM = 13

ROOM_NOT_ALL_READY = 20
ROOM_NOT_ENOUGH_PLAYERS = 21
ROOM_INVALID_PASSWORD = 22

USER_NOT_FOUND = 30

ROOM_NAME_TOO_SHORT = 40
ROOM_PASSWORD_TOO_SHORT = 41

ERROR_MESSAGES = {
    SERVER_ERROR: "서버와의 상태가 원활하지 않습니다.",
    INVALID_REQUEST: "잘못된 요청입니다.",
    LOGIN_FAILED: "아이디나 비밀번호를 다시 확인하세요",
    DUPLICATE_LOGIN_ID: "이미 로그인된 계정입니다.",
    
    ROOM_NOT_FOUND: "존재하지 않는 방입니다.",
    ROOM_FULL: "방이 꽉 찼습니다.",
    ROOM_INGAME: "이미 시작된 방입니다.",
    NOT_FOUND_JOINABLE_ROOM: "참가 가능한 방을 찾을 수 없습니다.",

    ROOM_NOT_ALL_READY: "모두 준비해야 시작할 수 있습니다.",
    ROOM_NOT_ENOUGH_PLAYERS: "최소 2명이 모여야 시작할 수 있습니다.",
    ROOM_INVALID_PASSWORD: "방 비밀번호와 일치하지 않습니다.",

    USER_NOT_FOUND: "유저를 찾을 수 없습니다.",
    ROOM_NAME_TOO_SHORT: "방 이름은 최소 4자 이상이어야 합니다",
    ROOM_PASSWORD_TOO_SHORT: "비밀번호는 최소 4자 이상이어야 합니다"
}
