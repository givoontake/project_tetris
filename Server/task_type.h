#pragma once
#include <variant>
#include "enum_class.h"

constexpr int INPUT_TYPE_NUM = 5; // RIGHT, LEFT, DOWN, ROTATE, DROP

struct TaskMove {
	EVENT_TYPE move_type;
};

struct TaskClearLine {
	int line_index;
};

struct TaskAddLine {
	int hole_x;
};

struct TaskFix{
	//int fixed_x;
	//int fixed_y;
};;

struct TaskGameover {
	
};

struct TaskUp {

};

// 이벤트 타입 말고 따로 필요한 내용이 없다면 굳이 구조체로 안만든다

using task_var = std::variant<TaskMove, TaskClearLine, TaskAddLine, TaskFix, TaskGameover, TaskUp>;

struct TaskType {
	EVENT_TYPE event_type;
	task_var task;
};