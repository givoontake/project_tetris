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
EventType Tetris::HandleTetrominoKeyInput(EventType move_type) //bool 반환은 충돌 성공 시 다음 블록 스폰이 되어야 하는 것을 생각함
{
    PrintMoveType(move_type);
    Tetromino if_move_tetromino = current_tetromino_;
	if(!CheckInputAllow(move_type)) return EventType::NONE;
    switch (move_type) {
    case EventType::RIGHT:
        ++if_move_tetromino.moved_pos.x;
        if (IsValidPosition(if_move_tetromino)) {
            current_tetromino_ = if_move_tetromino;
            send_tasks_.emplace_back(TaskType{ EventType::MOVE, TaskMove{EventType::RIGHT} });
			tick_counters_.SetRightTick(0);
            return EventType::RIGHT;
        }
        return EventType::NONE;

    case EventType::LEFT:
        --if_move_tetromino.moved_pos.x;
        if (IsValidPosition(if_move_tetromino)) {
            current_tetromino_ = if_move_tetromino;
            send_tasks_.emplace_back(TaskType{ EventType::MOVE, TaskMove{EventType::LEFT} });
			tick_counters_.SetLeftTick(0);
            return EventType::LEFT;
        }
        return EventType::NONE;

    case EventType::DOWN:
        ++if_move_tetromino.moved_pos.y;
        if (IsValidPosition(if_move_tetromino)) {
            current_tetromino_ = if_move_tetromino;
            send_tasks_.emplace_back(TaskType{ EventType::MOVE, TaskMove{EventType::DOWN} });
			tick_counters_.SetDownTick(0);
			tick_counters_.SetDownTimeoutTick(0);
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
			tick_counters_.SetRotateTick(0);
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
				tick_counters_.SetDropTick(0);
                return EventType::FIX;
            }
        }
    }
    }
	return EventType::NONE;
}

bool Tetris::IsValidPosition(const Tetromino& t)
{
    Position real_pos[4]; 

    for (int i = 0; i < 4; ++i) {
        real_pos[i].x = t.default_pos[i].x + t.moved_pos.x;
        real_pos[i].y = t.default_pos[i].y + t.moved_pos.y;
    }

    for (auto& pos : real_pos) {
        if (pos.x < 0 || pos.x > BOARD_WIDTH - 1) return false;
        if (pos.y < 0 || pos.y > TOTAL_HEIGHT - 1) return false;
        if (board_[pos.y][pos.x] == true) return false;
    }
    return true;
}

void Tetris::FixTetromino()
{
    Position real_pos[4];

    for (int i = 0; i < 4; ++i) {
        real_pos[i].x = current_tetromino_.default_pos[i].x + current_tetromino_.moved_pos.x;
        real_pos[i].y = current_tetromino_.default_pos[i].y + current_tetromino_.moved_pos.y;
    }

    for (auto& pos : real_pos) {
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
            TaskType t_type;
			t_type.event_type = EventType::CLEARLINE;
			t_type.task = TaskClearLine{ y };
			send_tasks_.emplace_back(t_type);
			++cleared_lines_;
			//std::cout << "Cleared line index y = " << y << "\n";
        }
    }
}

void Tetris::AddGarbageLines() // 팬딩에 add되어야 할 라인 로직 계산 후 채워줌
{
    // 줄 추가작업
    for (size_t i = 0; i < pending_garbage_lines_; i++){
        std::rotate(board_.begin(), board_.begin() + 1, board_.end());
        std::fill(board_[TOTAL_HEIGHT - 1].begin(), board_[TOTAL_HEIGHT - 1].end(), true);
        int hole_x = GetRandomHoleX();
        board_[TOTAL_HEIGHT - 1][hole_x] = false;

		TaskType add_line;
		add_line.event_type = EventType::ADDLINE;
        add_line.task = TaskAddLine{ .hole_x = hole_x };
        send_tasks_.emplace_back(add_line);

        if (CheckGameover()) return;

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

int Tetris::GetRandomHoleX()
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
	tick_counters_.InitTickData();
}

bool Tetris::CheckGameover()
{
	bool is_gameover = false;
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        for (int y = 0; y < HIDDEN_HEIGHT; ++y) {
            if (board_[y][x] == true) {
				is_gameover = true;
				break;
            }
        }
        if (is_gameover) break;
    }

    if (is_gameover) {
        TaskType t_type;
        t_type.event_type = EventType::GAMEOVER;
        t_type.task = TaskGameover{};
		send_tasks_.emplace_back(t_type);
    }

    return is_gameover;
}

void Tetris::DebugPrintBoard()
{
    // 현재 테트로미노의 실제 보드 상 위치 계산
    std::array<Position, 4> real_tetromino_pos;
    for (int i = 0; i < 4; ++i) {
        real_tetromino_pos[i].x = current_tetromino_.default_pos[i].x + current_tetromino_.moved_pos.x;
        real_tetromino_pos[i].y = current_tetromino_.default_pos[i].y + current_tetromino_.moved_pos.y;
    }

    std::cout << "====== Tetris Board (y: 0 ~ " << (TOTAL_HEIGHT - 1) << ") ======\n";

    for (int y = 0; y < TOTAL_HEIGHT; ++y) {
        std::cout << "|";
        for (int x = 0; x < BOARD_WIDTH; ++x) {

            bool is_current = false;
            for (const auto& p : real_tetromino_pos) {
                if (p.x == x && p.y == y) {
                    is_current = true;
                    break;
                }
            }

            if (is_current)
                std::cout << "▣";          // 현재 떨어지는 테트로미노: 빗금 네모
            else
                std::cout << (board_[y][x] ? "■" : "□");  // 고정된 블록 / 빈 칸
        }
        std::cout << "|\n";
    }

    std::cout << "====================================\n";
}

bool Tetris::CheckInputAllow(EventType move_type)
{
    switch (move_type)
    {
    case EventType::LEFT:
        if (tick_counters_.GetLeftTick() >= MOVE_TIMEOUT_TICK) return true;
        return false;

    case EventType::RIGHT:
        if (tick_counters_.GetRightTick() >= MOVE_TIMEOUT_TICK) return true;
        return false;

    case EventType::DOWN:
        if (tick_counters_.GetDownTick() >= MOVE_TIMEOUT_TICK) return true;
        return false;

    case EventType::ROTATE:
        if (tick_counters_.GetRotateTick() >= ROTATE_TIMEOUT_TICK) return true;
        return false;

    case EventType::DROP:
        if (tick_counters_.GetDropTick() >= DROP_TIMEOUT_TICK) return true;
        return false;

    default:
        return false;
    }
}

void Tetris::TickProcess()
{
    //// 첫 번째로 줄 추가 처리
    //if (tick_data.GetGarbageLineTick() >= tick_data.GetGarbageLineTimeout()) {
    //    tick_data.SetGarbageLineTick(0);
    //    AddGarbageLines(1, send_pending_tasks);
    //    for (auto& t : send_pending_tasks) {
    //        if (t.event_type == EventType::GAMEOVER) {
    //            return send_pending_tasks;
    //        }
    //    }
    //}

    // 첫 번째로 줄 추가 처리
    if (tick_counters_.GetGarbageLineTick() >= tick_counters_.GetGarbageLineTimeout()) {
        tick_counters_.SetGarbageLineTick(0);
        ++pending_garbage_lines_;
    }

    // 두 번째로 쌓인 입력 처리
    // 먼저 다운 타임아웃 이벤트 처리
    if (tick_counters_.GetDownTimeoutTick() >= tick_counters_.GetDownTimeout()) {
		tick_counters_.SetDownTimeoutTick(0);
        input_tasks_.emplace_back(EventType::DOWN);
    }

    // 중복되는 이벤트 합치기
    for (auto& type : input_tasks_) {
        int t = static_cast<int>(type);
        if (t < 0 || t >= INPUT_TYPE_NUM) {
            std::cout << "Tetris::TickProcess() - Invalid input task type: " << t << "\n";
            continue;
        }
        pending_moves_[static_cast<int>(t)] = true;
    }

    // 합쳐진 이벤트들 처리
    for (int i = static_cast<int>(EventType::DROP); i >= static_cast<int>(EventType::RIGHT); --i) {
        if (pending_moves_[i] == true) {
            EventType t = static_cast<EventType>(i);
			EventType result = HandleTetrominoKeyInput(t);
            if (result == EventType::NONE) {
                continue;
            }
            else if (result == EventType::FIX){ // 바로 여기서 처리해도 될 것 같은데
                FixTetromino();
                tick_counters_.SetDownTick(0);
                tick_counters_.SetDownTimeoutTick(0);
                tick_counters_.SetDropTick(0);

                ClearLine();
                // 게임오버는 룸에서 상태를 변경시키는 이벤트인데, 여기서 수행하면 상태를 변경할 수가 없다..
				CheckGameover();
            }
        }
    }
}

void Tetris::ResetTickData()
{
	pending_moves_.fill(false);
    input_tasks_.clear();
	send_tasks_.clear();
    pending_garbage_lines_ = 0;
    cleared_lines_ = 0;
}

