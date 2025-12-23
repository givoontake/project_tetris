#pragma once
constexpr int FPS = 50;
constexpr int FRAME_TIME = 1000 / FPS;
constexpr int MAX_TICK_WORKERS = 4;

constexpr int CLEAR_LINE_SCORE = 10;

constexpr int BUF_SIZE = 10240;
constexpr int MAX_MESSAGE_SIZE = 512;
constexpr int PORT_NUM = 12345;

constexpr int MAX_USER = 10000;
constexpr int MAX_ROOM = 5000;
constexpr int MAX_ROOM_NAME = 48; // 16자 * UTF-8 1문자 크기(3)
constexpr int MAX_ROOM_PASSWORD = 48;
constexpr int MAX_ARRAY_SIZE = 127;
constexpr int ID_SIZE = 16;

constexpr int MAX_USER_ID = 48;
constexpr int MAX_USER_PASSWORD = 48;
constexpr int MAX_USER_NAME = 48;

constexpr int MOVE_TIMEOUT_TICK = FPS / 10; // 0.1s
constexpr int ROTATE_TIMEOUT_TICK = FPS / 25; // 0.1s
constexpr int DOWN_TIMEOUT_TICK = FPS / 2; // 0.5s
constexpr int DROP_TIMEOUT_TICK = FPS / 10; // 0.1s
constexpr int GARBAGE_LINE_TIMEOUT_TICK = FPS * 10; // 10s