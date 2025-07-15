#pragma once
#include "define_tetromino.h"
constexpr int BOARD_WIDTH = 10;
constexpr int BOARD_HEIGHT = 20;

enum MOVE_TYPE { RIGHT, LEFT, DOWN, ROTATE, TIMEOUT, DROP }; // timeout-> 일정  시간이 지나 자동으로 아래로 한 칸 이동하는 것

class Tetris
{
	std::array<std::array<bool, BOARD_WIDTH>, BOARD_HEIGHT> board;
public:
	Tetris();

	bool HandleTetrominoKeyInput(Tetromino& tetromino, MOVE_TYPE type);
	int ClearLine();
	void AddLine(int num);
	int GetRandomX();
};

