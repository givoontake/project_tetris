#pragma once
#include "Tetromino.h"
#include "TetrisTickData.h"
#include "task_type.h"
constexpr int HIDDEN_HEIGHT = 5;
constexpr int BOARD_WIDTH = 10;
constexpr int BOARD_HEIGHT = 20;
constexpr int TOTAL_HEIGHT = BOARD_HEIGHT + HIDDEN_HEIGHT;

inline void PrintMoveType(EVENT_TYPE type)
{
	switch (type)
	{
	case EVENT_TYPE::RIGHT:
		std::cout << "RIGHT\n";
		break;
	case EVENT_TYPE::LEFT:
		std::cout << "LEFT\n";
		break;
	case EVENT_TYPE::DOWN:
		std::cout << "DOWN\n";
		break;
	case EVENT_TYPE::ROTATE:
		std::cout << "ROTATE\n";
		break;
	case EVENT_TYPE::DROP:
		std::cout << "DROP\n";
		break;
	}
}


class Tetris
{
	std::array<std::array<bool, BOARD_WIDTH>, TOTAL_HEIGHT> board;
	std::array<bool, INPUT_TYPE_NUM> pending_moves; // 1틱에 여러 입력이 들어오는 것을 방지하기 위한 컨테이너
	std::vector<EVENT_TYPE> input_tasks;
	Tetromino current_tetromino;
	TetrisTickData tick_data;
public:
	Tetris();

	//bool GetNewSpawn() const { return new_spawn; }
	//void SetNewSpawn(bool val) { new_spawn = val; }
	Tetromino GetCurrentTetromino() const { return current_tetromino; }
	Position GetCurrentTetrominoPos() const { return current_tetromino.moved_pos; }
	TetrisTickData& GetTickData() { return tick_data; }
	std::vector<EVENT_TYPE>& GetInputTasks() { return input_tasks; }

	void InitNewTetromino(char type, Position spawn_pos);
	EVENT_TYPE HandleTetrominoKeyInput(EVENT_TYPE move_type, std::vector<TaskType>& send_pending_tasks);
	bool IsValidPosition(const Tetromino& t); // 이건 움직였다고 가정한 값을 넘김
	void FixTetromino();
	std::vector<char> ClearLine();
	std::vector<char> GetGarbegeLineHoles(int cleard_line_num);
	void AddGarbageLines(int line_num, std::vector<TaskType>& send_pending_tasks);

	int GetRandomHoleX();
	void Clear();
	bool CheckGameover();
	void DebugPrintBoard();
	void SetPendingMove(int move_type) { pending_moves[move_type] = true; }
	bool CheckInputAllow(EVENT_TYPE move_type);
	std::vector<TaskType> TickProcess();
	void ClearTasks() { input_tasks.clear(); }
	void ClearPendingMoves() { pending_moves.fill(false); }
};
