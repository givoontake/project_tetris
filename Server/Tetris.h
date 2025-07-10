#pragma once
#include "define_tetromino.h"
constexpr int BOARD_WIDTH = 10;
constexpr int BOARD_HEIGHT = 20;

enum MOVE_TYPE { RIGHT, LEFT, DOWN, ROTATE, TIMEOUT }; // timeout-> 일정  시간이 지나 자동으로 아래로 한 칸 이동하는 것

class Tetris
{
	bool board[BOARD_HEIGHT][BOARD_WIDTH];
public:
	Tetris();

	bool CheckCollision(Tetromino& tetromino, MOVE_TYPE type);
};

