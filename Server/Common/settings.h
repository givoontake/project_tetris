#pragma once
constexpr int TICK_RATE = 50;
constexpr int TICK_INTERVAL_MS = 1000 / TICK_RATE;

constexpr int CLEAR_LINE_SCORE = 10;

constexpr int BUF_SIZE = 2048;
constexpr int MAX_MESSAGE_SIZE = 512;
constexpr int SERVER_PORT = 12345;

constexpr int MAX_PLAYER_COUNT = 10000;
constexpr int MAX_ROOM_COUNT = 5000;
constexpr int MAX_ROOM_NAME_SIZE = 48; // 16자 * UTF-8 1문자 크기(3)
constexpr int MAX_ROOM_PASSWORD_SIZE = 48;
constexpr int MAX_ARRAY_SIZE = 127;
constexpr int ID_SIZE = 16;

constexpr int MAX_PLAYER_ID_SIZE = 48;
constexpr int MAX_PLAYER_PASSWORD_SIZE = 48;
constexpr int MAX_PLAYER_NAME_SIZE = 48;

constexpr int MOVE_TIMEOUT_TICK = static_cast<int>(TICK_RATE * 0.05); // 좌우, 아래 이동 3가지
constexpr int ROTATE_TIMEOUT_TICK = static_cast<int>(TICK_RATE * 0.1);
constexpr int DOWN_TIMEOUT_TICK = static_cast<int>(TICK_RATE * 0.5); // 자동 아래 이동 시간
constexpr int DROP_TIMEOUT_TICK = static_cast<int>(TICK_RATE * 0.1);
constexpr int GARBAGE_LINE_TIMEOUT_TICK = static_cast<int>(TICK_RATE * 10);

constexpr int MAX_FRIEND_COUNT = 100;
