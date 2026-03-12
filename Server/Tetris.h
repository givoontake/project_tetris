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
	std::vector<EVENT_TYPE> input_tasks; // 틱에 들어온 입력들
	std::vector<TaskType> send_tasks; // 틱이 끝난 후, 처리된 입력에 따라 서버에 보낼 작업들
	Tetromino current_tetromino;
	TetrisTickCounters tick_counters;
	int pending_garbage_lines = 0; // 멀티 전용 변수, 틱 + 공격으로 추가될 라인 카운트해서 한 번에 증가시키는 용도
	int cleared_lines = 0; // 멀티 전용 변수, 1틱에 클리어된 라인 수로 공격으로 추가될 라인 수 계산용

public:
	Tetris();

	//bool GetNewSpawn() const { return new_spawn; }
	//void SetNewSpawn(bool val) { new_spawn = val; }
	Tetromino GetCurrentTetromino() const { return current_tetromino; }
	Position GetCurrentTetrominoPos() const { return current_tetromino.moved_pos; }
	TetrisTickCounters& GetTickData() { return tick_counters; }
	std::vector<EVENT_TYPE>& GetInputTasks() { return input_tasks; }
	std::vector<TaskType>& GetSendTasks() { return send_tasks; }
	int GetClearedLines() const { return cleared_lines; }
	int GetPendingGarbageLines() const { return pending_garbage_lines; }

	void AddPendingGarbageLines(int val) { pending_garbage_lines += val; }

	void InitNewTetromino(char type, Position spawn_pos);
	EVENT_TYPE HandleTetrominoKeyInput(EVENT_TYPE move_type);
	bool IsValidPosition(const Tetromino& t); // 이건 움직였다고 가정한 값을 넘김
	void FixTetromino();
	void ClearLine();
	void AddGarbageLines();

	int GetRandomHoleX();
	void Clear();
	bool CheckGameover();
	void DebugPrintBoard();
	void SetPendingMove(int move_type) { pending_moves[move_type] = true; }
	bool CheckInputAllow(EVENT_TYPE move_type);
	void TickProcess();
	void ResetTickData();
};
