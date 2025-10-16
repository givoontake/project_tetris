# packet_types.py
# C++ packetType.h와 "이름/값" 완전 일치
S2C_LOGIN         = 1
C2S_LOGIN         = 2
S2C_MESSAGE       = 3
C2S_MESSAGE       = 4

S2C_DISCONNECT    = 7
C2S_DISCONNECT    = 8

C2S_ADD_OPEN_ROOM = 9
S2C_ADD_OPEN_ROOM = 10

C2S_ADD_LOCK_ROOM = 11
S2C_ADD_LOCK_ROOM = 12

C2S_ADD_USER      = 13
S2C_ADD_USER      = 14

C2S_DELETE_USER   = 15
S2C_DELETE_USER   = 16

C2S_READY         = 17
S2C_READY         = 18

C2S_START         = 19
S2C_START         = 20

C2S_KICK          = 21
S2C_KICK          = 22

PACKET_STRUCT = { # 구조체를 대체

    # ---- Login ----
    S2C_LOGIN: ["size", "type", "id"],
    C2S_LOGIN: ["size", "type", "id"],

    # ---- Message ----
    S2C_MESSAGE: ["size", "type", "id"],
    C2S_MESSAGE: ["size", "type", "id"],

    # ---- Disconnect ----
    C2S_DISCONNECT: ["size", "type", "id"],
    S2C_DISCONNECT: ["size", "type", "id"],

    # ---- Add Open Room ----
    C2S_ADD_OPEN_ROOM: ["size", "type", "id", "max_user", "room_name"],
    S2C_ADD_OPEN_ROOM: ["size", "type", "id", "max_user", "room_name"],

    # ---- Add Lock Room ----
    C2S_ADD_LOCK_ROOM: ["size", "type", "id", "max_user", "room_name", "room_password"],
    S2C_ADD_LOCK_ROOM: ["size", "type", "id", "max_user", "room_name", "room_password"],

    # ---- Add User ----
    C2S_ADD_USER: ["size", "type", "id", "room_id", "name"],
    S2C_ADD_USER: ["size", "type", "id", "is_add", "name"],

    # ---- Delete User ----
    C2S_DELETE_USER: ["size", "type", "id"],
    S2C_DELETE_USER: ["size", "type", "id", "new_host_id"],

    # ---- Ready ----
    C2S_READY: ["size", "type", "id", "is_ready"],
    S2C_READY: ["size", "type", "id", "is_ready"],

    # ---- Start ----
    C2S_START: ["size", "type", "id"],
    S2C_START: ["size", "type", "is_start"],

    # ---- Kick ----
    C2S_KICK: ["size", "type", "id", "kick_user_id"],
    S2C_KICK: ["size", "type", "kick_user_id"],
}