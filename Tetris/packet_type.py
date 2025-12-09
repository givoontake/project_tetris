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

C2S_MOVE          = 23
S2C_MOVE          = 24

S2C_SPAWN         = 25

S2C_GAMEOVER      = 27

S2C_GAMEEND       = 29

S2C_CLEARLINE     = 31

S2C_FIX           = 33

# ---- Login ----
S2C_LOGIN_PACKET = ["size", "type", "id", "user_name"]
C2S_LOGIN_PACKET = ["size", "type", "user_id", "user_password"]

# ---- Message ----
S2C_MESSAGE_PACKET = ["size", "type", "id", "user_name", "message"]
C2S_MESSAGE_PACKET = ["size", "type", "id", "message"]

# ---- Disconnect ----
C2S_DISCONNECT_PACKET = ["size", "type", "id"]
S2C_DISCONNECT_PACKET = ["size", "type", "id"]

# ---- Add Open Room ----
C2S_ADD_OPEN_ROOM_PACKET = ["size", "type", "id", "max_user", "room_name"]
S2C_ADD_OPEN_ROOM_PACKET = ["size", "type", "id", "max_user", "room_name"]

# ---- Add Lock Room ----
C2S_ADD_LOCK_ROOM_PACKET = ["size", "type", "id", "max_user", "room_name", "room_password"]
S2C_ADD_LOCK_ROOM_PACKET = ["size", "type", "id", "max_user", "room_name", "room_password"]

# ---- Add User ----
C2S_ADD_USER_PACKET = ["size", "type", "id", "room_id", "name"]
S2C_ADD_USER_PACKET = ["size", "type", "id", "is_add", "name"]

# ---- Delete User ----
C2S_DELETE_USER_PACKET = ["size", "type"]
S2C_DELETE_USER_PACKET = ["size", "type", "id", "new_host_id"]

# ---- Ready ----
C2S_READY_PACKET = ["size", "type"]
S2C_READY_PACKET = ["size", "type", "id", "is_ready"]

# ---- Start ----
C2S_START_PACKET = ["size", "type"]
S2C_START_PACKET = ["size", "type", "is_start"]

# ---- Kick ----
C2S_KICK_PACKET = ["size", "type", "id", "kick_user_id"]
S2C_KICK_PACKET = ["size", "type", "kick_user_id"]

C2S_MOVE_PACKET = ["size", "type", "move_type"]
S2C_MOVE_PACKET = ["size", "type", "id", "move_type"]

S2C_SPAWN_PACKET = ["size", "type", "id", "tetromino_type", "spawn_x", "spawn_y"]

S2C_GAMEOVER_PACKET = ["size", "type", "id"]

S2C_GAMEEND_PACKET = ["size", "type", "id"]

S2C_CLEARLINE_PACKET = ["size", "type", "id", "rows"] # 뒤에 클라어될 라인들 인덱스(rows)가 가변 크기로 붙어온다

S2C_FIX_PACKET = ["size", "type", "id", "fixed_x", "fixed_y"]


# 타입 코드(int) -> 필드 목록(list[str]) 매핑
PACKET_STRUCT: dict[int, list[str]] = {
    # ---- Login ----
    S2C_LOGIN:         S2C_LOGIN_PACKET,
    C2S_LOGIN:         C2S_LOGIN_PACKET,

    # ---- Message ----
    S2C_MESSAGE:       S2C_MESSAGE_PACKET,
    C2S_MESSAGE:       C2S_MESSAGE_PACKET,

    # ---- Disconnect ----
    S2C_DISCONNECT:    S2C_DISCONNECT_PACKET,
    C2S_DISCONNECT:    C2S_DISCONNECT_PACKET,

    # ---- Add Open Room ----
    S2C_ADD_OPEN_ROOM: S2C_ADD_OPEN_ROOM_PACKET,
    C2S_ADD_OPEN_ROOM: C2S_ADD_OPEN_ROOM_PACKET,

    # ---- Add Lock Room ----
    S2C_ADD_LOCK_ROOM: S2C_ADD_LOCK_ROOM_PACKET,
    C2S_ADD_LOCK_ROOM: C2S_ADD_LOCK_ROOM_PACKET,

    # ---- Add User ----
    S2C_ADD_USER:      S2C_ADD_USER_PACKET,
    C2S_ADD_USER:      C2S_ADD_USER_PACKET,

    # ---- Delete User ----
    S2C_DELETE_USER:   S2C_DELETE_USER_PACKET,
    C2S_DELETE_USER:   C2S_DELETE_USER_PACKET,

    # ---- Ready ----
    S2C_READY:         S2C_READY_PACKET,
    C2S_READY:         C2S_READY_PACKET,

    # ---- Start ----
    S2C_START:         S2C_START_PACKET,
    C2S_START:         C2S_START_PACKET,

    # ---- Kick ----
    S2C_KICK:          S2C_KICK_PACKET,
    C2S_KICK:          C2S_KICK_PACKET,

    C2S_MOVE:          C2S_MOVE_PACKET,
    S2C_MOVE:          S2C_MOVE_PACKET,

    S2C_SPAWN:         S2C_SPAWN_PACKET,

    S2C_GAMEOVER:      S2C_GAMEOVER_PACKET,

    S2C_GAMEEND:       S2C_GAMEEND_PACKET,

    S2C_CLEARLINE:     S2C_CLEARLINE_PACKET,

    S2C_FIX:           S2C_FIX_PACKET
}

PRINT_TYPE = {
    1:  "S2C_LOGIN",
    2:  "C2S_LOGIN",
    3:  "S2C_MESSAGE",
    4:  "C2S_MESSAGE",

    7:  "S2C_DISCONNECT",
    8:  "C2S_DISCONNECT",

    9:  "C2S_ADD_OPEN_ROOM",
    10: "S2C_ADD_OPEN_ROOM",

    11: "C2S_ADD_LOCK_ROOM",
    12: "S2C_ADD_LOCK_ROOM",

    13: "C2S_ADD_USER",
    14: "S2C_ADD_USER",

    15: "C2S_DELETE_USER",
    16: "S2C_DELETE_USER",

    17: "C2S_READY",
    18: "S2C_READY",

    19: "C2S_START",
    20: "S2C_START",

    21: "C2S_KICK",
    22: "S2C_KICK",

    23: "C2S_MOVE",
    24: "S2C_MOVE",

    25: "S2C_SPAWN",

    27: "S2C_GAMEOVER",

    29: "S2C_GAMEEND",

    31: "S2C_CLEARLINE",

    33: "S2C_FIX"
}