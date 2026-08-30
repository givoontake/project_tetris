#pragma once
constexpr int FPS = 50;
constexpr int FRAME_TIME = 1000 / FPS;
constexpr int MAX_TICK_WORKERS = 4;
constexpr int GAME_DB_WORKER_COUNT = 2;
constexpr int LOGIN_DB_WORKER_COUNT = 1;
constexpr int DB_WORKER_COUNT = GAME_DB_WORKER_COUNT + LOGIN_DB_WORKER_COUNT;

constexpr int CLEAR_LINE_SCORE = 10;

constexpr int BUF_SIZE = 2048;
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

constexpr int MOVE_TIMEOUT_TICK = static_cast<int>(FPS * 0.05); // 좌우, 아래 이동 3가지
constexpr int ROTATE_TIMEOUT_TICK = static_cast<int>(FPS * 0.1);
constexpr int DOWN_TIMEOUT_TICK = static_cast<int>(FPS * 0.5); // 자동 아래 이동 시간
constexpr int DROP_TIMEOUT_TICK = static_cast<int>(FPS * 0.1);
constexpr int GARBAGE_LINE_TIMEOUT_TICK = static_cast<int>(FPS * 10);

constexpr int MAX_FRIENDS = 100;
