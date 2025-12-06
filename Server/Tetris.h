#pragma once
#include "define_tetromino.h"
constexpr int RESERVE_HEIGHT = 2;
constexpr int BOARD_WIDTH = 10;
constexpr int BOARD_HEIGHT = 20;
constexpr int TOTAL_HEIGHT = BOARD_HEIGHT + RESERVE_HEIGHT;

constexpr int RIGHT = 0;
constexpr int LEFT = 1;
constexpr int DOWN = 2;
constexpr int ROTATE = 3;
constexpr int DROP = 4;
constexpr int TIMEOUT = 5;
// timeout-> 일정  시간이 지나 자동으로 아래로 한 칸 이동하는 것

class Tetris
{
	std::array<std::array<bool, BOARD_WIDTH>, TOTAL_HEIGHT> board;
	Tetromino current_tetromino;
	bool move_allow = false;
public:
	Tetris();

	//bool GetNewSpawn() const { return new_spawn; }
	//void SetNewSpawn(bool val) { new_spawn = val; }
	bool GetMoveAllow() const { return move_allow; }
	void SetMoveAllow(bool val) { move_allow = val; }

	void InitNewTetromino(char type, Position spawn_pos);
	bool HandleTetrominoKeyInput(int move_type);
	std::vector<char> ClearLine();
	void AddLine(int num);
	int GetRandomX();
	void Clear();
	bool CheckGameover();
};
