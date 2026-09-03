#pragma once
#include <iostream>
#include "Tetromino.h"
#include "TetrisTimers.h"
#include "task_type.h"
constexpr int HIDDEN_HEIGHT = 5;
constexpr int BOARD_WIDTH = 10;
constexpr int BOARD_HEIGHT = 20;
constexpr int TOTAL_HEIGHT = BOARD_HEIGHT + HIDDEN_HEIGHT;

inline void PrintMoveType(EventType type)
{
	switch (type)
	{
	case EventType::RIGHT:
		std::cout << "RIGHT\n";
		break;
	case EventType::LEFT:
		std::cout << "LEFT\n";
		break;
	case EventType::DOWN:
		std::cout << "DOWN\n";
		break;
	case EventType::ROTATE:
		std::cout << "ROTATE\n";
		break;
	case EventType::DROP:
		std::cout << "DROP\n";
		break;
	}
}


class Tetris
{
	std::array<std::array<bool, BOARD_WIDTH>, TOTAL_HEIGHT> board_;
	std::array<bool, INPUT_TYPE_COUNT> pending_moves_; // 1틱에 여러 입력이 들어오는 것을 방지하기 위한 컨테이너
	std::vector<EventType> input_tasks_; // 틱에 들어온 입력들
	std::vector<TaskType> send_tasks_; // 틱이 끝난 후, 처리된 입력에 따라 서버에 보낼 작업들
	Tetromino current_tetromino_;
	TetrisTimers timers_;
	int pending_garbage_line_count_ = 0; // 멀티 전용 변수, 틱 + 공격으로 추가될 라인 카운트해서 한 번에 증가시키는 용도
	int cleared_line_count_ = 0; // 멀티 전용 변수, 1틱에 클리어된 라인 수로 공격으로 추가될 라인 수 계산용

public:
	Tetris();

	//bool GetNewSpawn() const { return new_spawn; }
	//void SetNewSpawn(bool val) { new_spawn = val; }
	std::vector<EventType>& GetInputTasks() { return input_tasks_; }
	std::vector<TaskType>& GetSendTasks() { return send_tasks_; }
	int GetClearedLineCount() const { return cleared_line_count_; }
	void AddPendingGarbageLines(int val) { pending_garbage_line_count_ += val; }

	void InitNewTetromino(char type, Position spawn_pos);
	EventType ProcessMoveInput(EventType move_type, std::chrono::steady_clock::time_point tick_time);
	bool IsValidPosition(const Tetromino& tetromino); // 이건 움직였다고 가정한 값을 넘김
	void FixTetromino();
	void ClearLine();
	void AddGarbageLines();

	int GenerateRandomGarbageHole();
	void Clear();
	bool CheckGameOver();
	void ProcessTick(std::chrono::steady_clock::time_point tick_time);
	void ResetTickData();
};
