#pragma once
#include <variant>
#include "enum_class.h"

constexpr int INPUT_TYPE_COUNT = 5; // RIGHT, LEFT, DOWN, ROTATE, DROP

struct TaskMove {
	EventType move_type;
};

struct TaskClearLine {
	int line_index;
};

struct TaskAddLine {
	int hole_x;
};

struct TaskFix{
	int fixed_x;
	int fixed_y;
};;

struct TaskGameOver {
	
};

struct TaskUp {

};

struct TaskSpawn {

};

struct TaskGameEnd {
	int winner_id;	
};

// 추가 데이터가 없는 이벤트는 별도 구조체를 사용하지 않는다.

using TaskVar = std::variant<TaskMove, TaskClearLine, TaskAddLine, TaskFix, TaskGameOver, TaskUp, TaskGameEnd, TaskSpawn>;

struct TaskType {
	EventType event_type;
	TaskVar task;
};
