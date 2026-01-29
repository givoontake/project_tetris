# define_format.py
"""
C++ define.h의 #pragma pack(1) 구조체와 1:1로 대응하는 struct 포맷 모음 (모두 signed).
- 리틀엔디안 고정('<')  ※ 필드 단위 포맷 사전(PACK_FIELD_FMT)은 엔디언 접두 미포함
- size:  signed short  (h)
- type:  signed char   (b)
- int:   signed int    (i)
- long long: signed long long (q)
- bool:  signed char   (b)  # 1바이트
- 고정 길이 char 배열: N바이트 문자열 'Ns'
※ 아래 길이 상수는 define.h와 반드시 동일해야 합니다.
"""

# ---- 길이 상수 ----
MAX_USER_ID = 16*3
MAX_USER_PASSWORD = 16*3
MAX_USER_NAME = 16*3
MAX_ROOM_NAME = 16*3
MAX_ROOM_PASSWORD = 16*3
MAX_INPUT = 16
MAX_CHAT_INPUT = 256
MAX_CHAT_BYTES = MAX_CHAT_INPUT*3

# ---- Test Login ----

# ---- Login ----
S2C_LOGIN_PACKET_FMT = "<hbi"
C2S_LOGIN_PACKET_FMT = f"<hbiiii{MAX_USER_NAME}s"

# ---- Message ----
S2C_MESSAGE_PACKET_FMT = f"<hbi{MAX_USER_NAME}s{MAX_CHAT_BYTES}s"
C2S_MESSAGE_PACKET_FMT = f"<hbi{MAX_CHAT_BYTES}s"

# ---- Test (핑/지연 등) ----
# S2C_TEST_PACKET_FMT = "<hbiq"
# C2S_TEST_PACKET_FMT = "<hbiq"

# ---- Disconnect ----
C2S_DISCONNECT_PACKET_FMT = "<hbi"
S2C_DISCONNECT_PACKET_FMT = "<hbi"

# ---- Add Open Room ----
C2S_ADD_OPEN_ROOM_PACKET_FMT = f"<hbib{MAX_ROOM_NAME}s"
S2C_ADD_OPEN_ROOM_PACKET_FMT = f"<hbib{MAX_ROOM_NAME}s"

# ---- Add Lock Room ----
C2S_ADD_LOCK_ROOM_PACKET_FMT = f"<hbib{MAX_ROOM_NAME}s{MAX_ROOM_PASSWORD}s"
S2C_ADD_LOCK_ROOM_PACKET_FMT = f"<hbib{MAX_ROOM_NAME}s{MAX_ROOM_PASSWORD}s"

# ---- Add User ----
C2S_ADD_USER_PACKET_FMT = f"<hbii{MAX_USER_NAME}s"
S2C_ADD_USER_PACKET_FMT = f"<hbib{MAX_USER_NAME}s"  # bool → signed char(b)

# ---- Delete User ----
C2S_DELETE_USER_PACKET_FMT = "<hb"
S2C_DELETE_USER_PACKET_FMT = "<hbii"

# ---- Ready ----
C2S_READY_PACKET_FMT = "<hb"  # bool → signed char(b)
S2C_READY_PACKET_FMT = "<hbib"  # bool → signed char(b)

# ---- Start ----
C2S_START_PACKET_FMT = "<hb"
S2C_START_PACKET_FMT = "<hbbi"  # bool → signed char(b)

# ---- Kick ----
C2S_KICK_PACKET_FMT = "<hbii"
S2C_KICK_PACKET_FMT = "<hbi"

C2S_MOVE_PACKET_FMT = "<hbb"
S2C_MOVE_PACKET_FMT = "<hbib"

S2C_SPAWN_PACKET_FMT = "<hbibbbb"

S2C_GANEOVER_PACKET_FMT = "<hbi"

S2C_GAMEEND_PACKET_FMT = "<hbi"

S2C_CLEARLINE_PACKET_FMT = "<hbibi"

S2C_FIX_PACKET_FMT = "<hbibb"

S2C_ADDLINE_PACKET_FMT = "<hbib"

S2C_UPDATE_SCORE_PACKET_FMT = "hbi"

# =====================================================================
#  필드 단위: "문자열 → 포맷" 사전 및(옵션) 스키마 샘플
#  - 엔디언 접두는 붙이지 않습니다(필드 단위 포맷이므로). 호출부에서 '<' 등을 결합하세요.
#  - 패킷 빌더가 이 사전을 참고하여 이름 기준으로 struct.pack에 쓸 포맷을 선택합니다.
# =====================================================================

# 추천 이름: PACK_FIELD_FMT  (간단 키-포맷 매핑)
PACK_FIELD_FMT = {
    "size": "h",
    "type": "b",

    "id": "i",            # int
    "temp_id": "i",
    "room_id": "i",
    "new_host_id": "i",
    "kick_user_id": "i",
    "score": "i",
    "win_count": "i",
    "lose_count": "i",
    "max_score": "i",
    # 1바이트 값
    "max_user": "b",      # char (signed)
    "is_add": "b",        # bool을 1바이트 signed char로 전송
    "is_ready": "b",
    "is_start": "b",
    "move_type": "b",
    "tetromino_type": "b",
    "next_tetromino_type": "b",
    "spawn_x": "b",
    "spawn_y": "b",
    "fixed_x": "b",
    "fixed_y": "b",
    "line_index": "b",
    "hole_x": "b",

    # 8바이트 정수
    "last_time": "q",     # long long

    # 고정 길이 문자열(바이트 배열)
    "user_id":       f"{MAX_USER_NAME}s",
    "user_password":  f"{MAX_USER_PASSWORD}s",
    "user_name":      f"{MAX_USER_NAME}s",
    "room_name":     f"{MAX_ROOM_NAME}s",
    "room_password": f"{MAX_ROOM_PASSWORD}s",
    "name":          f"{MAX_USER_NAME}s",
    "message":  f"{MAX_CHAT_BYTES}s"
}

FIELD_SIZE = {
    # 공통 헤더
    "size": 2,   # h
    "type": 1,   # b

    # 4바이트 정수
    "id": 4,
    "temp_id": 4,
    "room_id": 4,
    "new_host_id": 4,
    "kick_user_id": 4,
    "score": 4,
    "win_count": 4,
    "lose_count": 4,
    "max_score": 4,

    # 1바이트 값
    "max_user": 1,
    "is_add": 1,
    "is_ready": 1,
    "is_start": 1,
    "move_type": 1,
    "tetromino_type": 1,
    "next_tetromino_type": 1,
    "spawn_x": 1,
    "spawn_y": 1,
    "fixed_x": 1,
    "fixed_y": 1,
    "line_index": 1,
    "hole_x": 1,

    # 8바이트 정수
    "last_time": 8,

    # 고정 길이 문자열(바이트 배열)
    "user_id": MAX_USER_NAME,
    "user_password": MAX_USER_PASSWORD,
    "user_name": MAX_USER_NAME,
    "room_name": MAX_ROOM_NAME,
    "room_password": MAX_ROOM_PASSWORD,
    "name": MAX_USER_NAME,
    "message": MAX_CHAT_BYTES
}