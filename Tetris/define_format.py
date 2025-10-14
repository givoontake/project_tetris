# define_format.py
"""
C++ define.h의 #pragma pack(1) 구조체와 1:1로 대응하는 struct 포맷 모음.
- 리틀엔디안 고정('<')
- size: unsigned short(H), type: unsigned char(B)
- int: unsigned int(I), long long: signed long long(q)
- bool: 1바이트로 간주 → unsigned char(B)
- 고정 길이 char 배열: N바이트 문자열 'Ns'
※ 아래 길이 상수는 define.h와 반드시 동일해야 합니다.
"""

MAX_ROOM_NAME = 48
MAX_ROOM_PASSWORD = 48
MAX_USER_NAME = 48

HEADER_FMT = "<HB"

# Test Login
S2C_TEST_LOGIN_PACKET_FMT = "<HBI"
C2S_TEST_LOGIN_PACKET_FMT = "<HBI"

# Login
S2C_LOGIN_PACKET_FMT = "<HBI"
C2S_LOGIN_PACKET_FMT = "<HBI"

# Message
S2C_MESSAGE_PACKET_FMT = "<HBI"
C2S_MESSAGE_PACKET_FMT = "<HBI"

# Test (핑/지연 등)
# S2C_TEST_PACKET_FMT = "<HBIq"
# C2S_TEST_PACKET_FMT = "<HBIq"

# Disconnect
C2S_DISCONNECT_PACKET_FMT = "<HBI"
S2C_DISCONNECT_PACKET_FMT = "<HBI"

# Add Open Room
C2S_ADD_OPEN_ROOM_PACKET_FMT = f"<HBIB{MAX_ROOM_NAME}s"
S2C_ADD_OPEN_ROOM_PACKET_FMT = f"<HBIB{MAX_ROOM_NAME}s" 

# Add Lock Room
C2S_ADD_LOCK_ROOM_PACKET_FMT = f"<HBIB{MAX_ROOM_NAME}s{MAX_ROOM_PASSWORD}s"
S2C_ADD_LOCK_ROOM_PACKET_FMT = f"<HBIB{MAX_ROOM_NAME}s{MAX_ROOM_PASSWORD}s"

# Add User
C2S_ADD_USER_PACKET_FMT = f"<HBII{MAX_USER_NAME}s"
S2C_ADD_USER_PACKET_FMT = f"<HBIB{MAX_USER_NAME}s"  # bool → 1바이트(B)

# Delete User
C2S_DELETE_USER_PACKET_FMT = "<HBI"
S2C_DELETE_USER_PACKET_FMT = "<HBII"

# Ready
C2S_READY_PACKET_FMT = "<HBIB"  # bool → 1바이트(B)
S2C_READY_PACKET_FMT = "<HBIB"  # bool → 1바이트(B)

# Start
C2S_START_PACKET_FMT = "<HBI"
S2C_START_PACKET_FMT = "<HBB"  # bool → 1바이트(B)

# Kick
C2S_KICK_PACKET_FMT = "<HBII"
S2C_KICK_PACKET_FMT = "<HBI"
