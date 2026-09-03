#include <vector>
#include <algorithm>
#include <random>
#include <variant>
#include "Tetris.h"

Tetris::Tetris()
{
    Clear(); // init처럼 쓰는중
}

void Tetris::InitNewTetromino(char type, Position spawn_pos)
{
    current_tetromino_.type = type;
    current_tetromino_.moved_pos.x = spawn_pos.x;
	current_tetromino_.moved_pos.y = spawn_pos.y;
    current_tetromino_.shape_index = 0;
	current_tetromino_.default_pos = TETROMINOS.at(type)[0].default_pos;
}

// 좌표 관리는 정의된 테트로미노 절대 좌표 + 키보드로 이동한 상대 좌표를 더해 현재 테트로미노 좌표를 구한다.
// 그러면 회전된 테트로미노 관리가 수월해진다.
EventType Tetris::ProcessMoveInput(EventType move_type, std::chrono::steady_clock::time_point tick_time) //bool 반환은 충돌 성공 시 다음 블록 스폰이 되어야 하는 것을 생각함
{
    PrintMoveType(move_type);
    Tetromino if_move_tetromino = current_tetromino_;
	if(!timers_.IsInputAllowed(move_type, tick_time)) return EventType::NONE;
    switch (move_type) {
    case EventType::RIGHT:
        ++if_move_tetromino.moved_pos.x;
        if (IsValidPosition(if_move_tetromino)) {
            current_tetromino_ = if_move_tetromino;
            send_tasks_.emplace_back(TaskType{ EventType::MOVE, TaskMove{EventType::RIGHT} });
			timers_.RecordInput(EventType::RIGHT, tick_time);
            return EventType::RIGHT;
        }
        return EventType::NONE;

    case EventType::LEFT:
        --if_move_tetromino.moved_pos.x;
        if (IsValidPosition(if_move_tetromino)) {
            current_tetromino_ = if_move_tetromino;
            send_tasks_.emplace_back(TaskType{ EventType::MOVE, TaskMove{EventType::LEFT} });
			timers_.RecordInput(EventType::LEFT, tick_time);
            return EventType::LEFT;
        }
        return EventType::NONE;

    case EventType::DOWN:
        ++if_move_tetromino.moved_pos.y;
        if (IsValidPosition(if_move_tetromino)) {
            current_tetromino_ = if_move_tetromino;
            send_tasks_.emplace_back(TaskType{ EventType::MOVE, TaskMove{EventType::DOWN} });
			timers_.RecordInput(EventType::DOWN, tick_time);
			timers_.RestartAutoDown(tick_time);
            return EventType::DOWN;
        }
        else {
            send_tasks_.emplace_back(TaskType{ EventType::FIX, TaskFix{current_tetromino_.moved_pos.x, current_tetromino_.moved_pos.y} });
            return EventType::FIX;
        }

    case EventType::ROTATE: {
        const std::vector<Tetromino>& shapes = TETROMINOS.at(if_move_tetromino.type);
        if (if_move_tetromino.shape_index + 1 < shapes.size()) ++if_move_tetromino.shape_index; // 다음 인덱스가 존재하면
        else if_move_tetromino.shape_index = 0; // 다음 인덱스가 없다면

        if_move_tetromino.default_pos = shapes[if_move_tetromino.shape_index].default_pos; // 회전된 인덱스로 테트로미노 변경
        if (IsValidPosition(if_move_tetromino)) {
            current_tetromino_ = if_move_tetromino;
            send_tasks_.emplace_back(TaskType{ EventType::MOVE, TaskMove{EventType::ROTATE} });
			timers_.RecordInput(EventType::ROTATE, tick_time);
            return EventType::ROTATE;
        }
        return EventType::NONE;
    }

    case EventType::DROP: {
        while (true) {
            if (IsValidPosition(if_move_tetromino)) {
                current_tetromino_ = if_move_tetromino;
                ++if_move_tetromino.moved_pos.y;
            }
            else {
                send_tasks_.emplace_back(TaskType{ EventType::FIX, TaskFix{current_tetromino_.moved_pos.x, current_tetromino_.moved_pos.y} });
				timers_.RecordInput(EventType::DROP, tick_time);
                return EventType::FIX;
            }
        }
    }
    }
	return EventType::NONE;
}

bool Tetris::IsValidPosition(const Tetromino& tetromino)
{
    Position board_positions[4];

    for (int i = 0; i < 4; ++i) {
        board_positions[i].x = tetromino.default_pos[i].x + tetromino.moved_pos.x;
        board_positions[i].y = tetromino.default_pos[i].y + tetromino.moved_pos.y;
    }

    for (auto& pos : board_positions) {
        if (pos.x < 0 || pos.x > BOARD_WIDTH - 1) return false;
        if (pos.y < 0 || pos.y > TOTAL_HEIGHT - 1) return false;
        if (board_[pos.y][pos.x] == true) return false;
    }
    return true;
}

void Tetris::FixTetromino()
{
    Position board_positions[4];

    for (int i = 0; i < 4; ++i) {
        board_positions[i].x = current_tetromino_.default_pos[i].x + current_tetromino_.moved_pos.x;
        board_positions[i].y = current_tetromino_.default_pos[i].y + current_tetromino_.moved_pos.y;
    }

    for (auto& pos : board_positions) {
        board_[pos.y][pos.x] = true;
    }
}

void Tetris::ClearLine()
{
    // ✅ 줄 삭제는 "보이는 영역"만: RESERVE_HEIGHT ~ TOTAL_HEIGHT-1
    for (int y = HIDDEN_HEIGHT; y < TOTAL_HEIGHT; ++y) {
        if (std::all_of(board_[y].begin(), board_[y].end(), // 한 줄이 모두 true(채워짐)이면
            [](bool is_cell_filled) { return is_cell_filled; })) {
            std::fill(board_[y].begin(), board_[y].end(), false); // 현재 줄을 모두 false로 바꾸고

            // false 줄을 맨 위로 옮기고, 맨 위에서 1줄씩 아래로 당김
            // (숨겨진 0~RESERVE_HEIGHT-1 줄도 같이 아래로 내려오지만,
            //  줄 삭제 시의 "중력"은 전체 스택을 대상으로 적용)
            std::rotate(board_.begin(), board_.begin() + y, board_.begin() + y + 1);
            TaskType task;
			task.event_type = EventType::CLEAR_LINE;
			task.task = TaskClearLine{ y };
			send_tasks_.emplace_back(task);
			++cleared_line_count_;
			//std::cout << "Cleared line index y = " << y << "\n";
        }
    }
}

void Tetris::AddGarbageLines() // 팬딩에 add되어야 할 라인 로직 계산 후 채워줌
{
    // 줄 추가작업
    for (size_t i = 0; i < pending_garbage_line_count_; i++){
        std::rotate(board_.begin(), board_.begin() + 1, board_.end());
        std::fill(board_[TOTAL_HEIGHT - 1].begin(), board_[TOTAL_HEIGHT - 1].end(), true);
        int hole_x = GenerateRandomGarbageHole();
        board_[TOTAL_HEIGHT - 1][hole_x] = false;

		TaskType add_line;
		add_line.event_type = EventType::ADD_LINE;
        add_line.task = TaskAddLine{ .hole_x = hole_x };
        send_tasks_.emplace_back(add_line);

        if (CheckGameOver()) return;

        // 테트로미노와 겹치면 안겹치게 테트로미노 위로 올리기
        if (!IsValidPosition(current_tetromino_)) {
            --current_tetromino_.moved_pos.y;
            TaskType up_task;
            up_task.event_type = EventType::MOVE;
            up_task.task = TaskMove{EventType::UP};
            send_tasks_.emplace_back(up_task);
        }
    }
}

int Tetris::GenerateRandomGarbageHole()
{
    // OS/하드웨어 엔트로피에서 시드 생성
    static int prev_x = 0;
    static std::random_device rd;

    // 넣어준 시드값에 의해 생성될 난수가 준비된다.
    static std::mt19937 gen(rd());

    // 0~BOARD_WIDTH - 1 범위에서 균등 분포로 값 출력
    static std::uniform_int_distribution<int> dist(0, BOARD_WIDTH - 1);
	int random_x;
    do {
        random_x = dist(gen);
    } while (random_x == prev_x);  // 이전과 같은 위치를 피한다

    prev_x = random_x;

    return random_x;
}

void Tetris::Clear()
{
    // ✅ 전체 보드(TOTAL_HEIGHT) 초기화
    for (int i = 0; i < TOTAL_HEIGHT; ++i) {
        for (int j = 0; j < BOARD_WIDTH; ++j) {
            board_[i][j] = false;
        }
    } // 가로 = WIDTH = x / 세로 = HEIGHT = y

	pending_moves_.fill(false);
	timers_.Reset();
}

bool Tetris::CheckGameOver()
{
	bool is_game_over = false;
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        for (int y = 0; y < HIDDEN_HEIGHT; ++y) {
            if (board_[y][x] == true) {
				is_game_over = true;
				break;
            }
        }
        if (is_game_over) break;
    }

    if (is_game_over) {
        TaskType task;
        task.event_type = EventType::GAME_OVER;
        task.task = TaskGameOver{};
		send_tasks_.emplace_back(task);
    }

    return is_game_over;
}

void Tetris::ProcessTick(std::chrono::steady_clock::time_point tick_time)
{
    // 첫 번째로 줄 추가 처리
    if (timers_.IsGarbageLineDue(tick_time)) {
        timers_.RestartGarbageLine(tick_time);
        ++pending_garbage_line_count_;
    }

    // 두 번째로 쌓인 입력 처리
    // 먼저 다운 타임아웃 이벤트 처리
    if (timers_.IsAutoDownDue(tick_time)) {
		timers_.RestartAutoDown(tick_time);
        input_tasks_.emplace_back(EventType::DOWN);
    }

    // 중복되는 이벤트 합치기
    for (auto& type : input_tasks_) {
        int input_type_index = static_cast<int>(type);
        if (input_type_index < 0 || input_type_index >= INPUT_TYPE_COUNT) {
            std::cout << "Tetris::ProcessTick() - Invalid input task type: " << input_type_index << "\n";
            continue;
        }
        pending_moves_[static_cast<int>(input_type_index)] = true;
    }

    // 합쳐진 이벤트들 처리
    for (int i = static_cast<int>(EventType::DROP); i >= static_cast<int>(EventType::RIGHT); --i) {
        if (pending_moves_[i] == true) {
            EventType move_type = static_cast<EventType>(i);
			EventType result = ProcessMoveInput(move_type, tick_time);
            if (result == EventType::NONE) {
                continue;
            }
            else if (result == EventType::FIX){ // 바로 여기서 처리해도 될 것 같은데
                FixTetromino();
                timers_.RecordInput(EventType::DOWN, tick_time);
                timers_.RestartAutoDown(tick_time);
                timers_.RecordInput(EventType::DROP, tick_time);

                ClearLine();
                // 게임오버는 룸에서 상태를 변경시키는 이벤트인데, 여기서 수행하면 상태를 변경할 수가 없다..
				CheckGameOver();
            }
        }
    }
}

void Tetris::ResetTickData()
{
	pending_moves_.fill(false);
    input_tasks_.clear();
	send_tasks_.clear();
    pending_garbage_line_count_ = 0;
    cleared_line_count_ = 0;
}

