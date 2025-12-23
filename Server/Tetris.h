#pragma once
#include "Tetromino.h"
#include "TetrisTickData.h"
constexpr int HIDDEN_HEIGHT = 2;
constexpr int BOARD_WIDTH = 10;
constexpr int BOARD_HEIGHT = 20;
constexpr int TOTAL_HEIGHT = BOARD_HEIGHT + HIDDEN_HEIGHT;

constexpr char GAMEOVER = -3;
constexpr char SPAWN = -2;
constexpr char FIX = -1;

constexpr int RIGHT = 0;
constexpr int LEFT = 1;
constexpr int DOWN = 2;
constexpr int ROTATE = 3;
constexpr int DROP = 4;
constexpr int TIMEOUT = 5;
constexpr int UP = 6; // 실제로 받지는 않고 처리상 롤백용

constexpr int MOVE_TYPE_NUM = 7;
// timeout-> 일정  시간이 지나 자동으로 아래로 한 칸 이동하는 것
inline void PrintMoveType(int type)
{
	switch (type)
	{
	case RIGHT:
		std::cout << "RIGHT\n";
		break;
	case LEFT:
		std::cout << "LEFT\n";
		break;
	case DOWN:
		std::cout << "DOWN\n";
		break;
	case ROTATE:
		std::cout << "ROTATE\n";
		break;
	case DROP:
		std::cout << "DROP\n";
		break;
	case TIMEOUT:
		std::cout << "TIMEOUT\n";
		break;
	}
}


class Tetris
{
	std::array<std::array<bool, BOARD_WIDTH>, TOTAL_HEIGHT> board;
	std::array<bool, MOVE_TYPE_NUM> pending_moves; // 1틱에 여러 입력이 들어오는 것을 방지하기 위한 컨테이너
	Tetromino current_tetromino;
	TetrisTickData tick_data;
public:
	Tetris();

	//bool GetNewSpawn() const { return new_spawn; }
	//void SetNewSpawn(bool val) { new_spawn = val; }
	Tetromino GetCurrentTetromino() const { return current_tetromino; }
	Position GetCurrentTetrominoPos() const { return current_tetromino.moved_pos; }
	TetrisTickData& GetTickData() { return tick_data; }

	void InitNewTetromino(char type, Position spawn_pos);
	int HandleTetrominoKeyInput(int move_type);
	bool IsValidPosition(const Tetromino& t); // 이건 움직였다고 가정한 값을 넘김
	void FixTetromino();
	std::vector<char> ClearLine();
	std::vector<char> GetGarbegeLineHoles(int cleard_line_num);
	std::vector<char> AddGarbageLines(std::vector<char> holes);

	int GetRandomHoleX();
	void Clear();
	bool CheckGameover();
	void DebugPrintBoard();
	void SetPendingMove(int move_type) { pending_moves[move_type] = true; }
	std::vector<char> ProcessPendingMoves();
	bool CheckInputAllow(int move_type);
};
