#pragma once
#include "define_tetromino.h"
constexpr int BOARD_WIDTH = 10;
constexpr int BOARD_HEIGHT = 20;

enum MOVE_TYPE { RIGHT, LEFT, DOWN, ROTATE };

class Tetris
{
	bool board[BOARD_HEIGHT][BOARD_WIDTH];
public:
	Tetris();

	void CheckCollision(Tetromino t, MOVE_TYPE type, int x, int y);
};

