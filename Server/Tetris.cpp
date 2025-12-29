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
    current_tetromino.type = type;
    current_tetromino.moved_pos.x = spawn_pos.x;
	current_tetromino.moved_pos.y = spawn_pos.y;
    current_tetromino.shape_index = 0;
	current_tetromino.default_pos = TETROMINOS.at(type)[0].default_pos;
}

// 좌표 관리는 정의된 테트로미노 절대 좌표 + 키보드로 이동한 상대 좌표를 더해 현재 테트로미노 좌표를 구한다.
// 그러면 회전된 테트로미노 관리가 수월해진다.
EVENT_TYPE Tetris::HandleTetrominoKeyInput(EVENT_TYPE move_type, std::vector<TaskType>& send_pending_tasks) //bool 반환은 충돌 성공 시 다음 블록 스폰이 되어야 하는 것을 생각함
{
    PrintMoveType(move_type);
    Tetromino if_move_tetromino = current_tetromino;
	if(!CheckInputAllow(move_type)) return EVENT_TYPE::NONE;
    switch (move_type) {
    case EVENT_TYPE::RIGHT:
        ++if_move_tetromino.moved_pos.x;
        if (IsValidPosition(if_move_tetromino)) {
            current_tetromino = if_move_tetromino;
            send_pending_tasks.emplace_back(TaskType{ EVENT_TYPE::MOVE, TaskMove{EVENT_TYPE::RIGHT} });
			tick_data.SetRightTick(0);
            return EVENT_TYPE::RIGHT;
        }
        return EVENT_TYPE::NONE;

    case EVENT_TYPE::LEFT:
        --if_move_tetromino.moved_pos.x;
        if (IsValidPosition(if_move_tetromino)) {
            current_tetromino = if_move_tetromino;
            send_pending_tasks.emplace_back(TaskType{ EVENT_TYPE::MOVE, TaskMove{EVENT_TYPE::LEFT} });
			tick_data.SetLeftTick(0);
            return EVENT_TYPE::LEFT;
        }
        return EVENT_TYPE::NONE;

    case EVENT_TYPE::DOWN:
        ++if_move_tetromino.moved_pos.y;
        if (IsValidPosition(if_move_tetromino)) {
            current_tetromino = if_move_tetromino;
            send_pending_tasks.emplace_back(TaskType{ EVENT_TYPE::MOVE, TaskMove{EVENT_TYPE::DOWN} });
			tick_data.SetDownTick(0);
			tick_data.SetDownTimeoutTick(0);
            return EVENT_TYPE::DOWN;
        }
        else {
            send_pending_tasks.emplace_back(TaskType{ EVENT_TYPE::FIX, TaskFix{} });
            return EVENT_TYPE::FIX;
        }

    case EVENT_TYPE::ROTATE: {
        const std::vector<Tetromino>& shapes = TETROMINOS.at(if_move_tetromino.type);
        if (if_move_tetromino.shape_index + 1 < shapes.size()) ++if_move_tetromino.shape_index; // 다음 인덱스가 존재하면
        else if_move_tetromino.shape_index = 0; // 다음 인덱스가 없다면

        if_move_tetromino.default_pos = shapes[if_move_tetromino.shape_index].default_pos; // 회전된 인덱스로 테트로미노 변경
        if (IsValidPosition(if_move_tetromino)) {
            current_tetromino = if_move_tetromino;
            send_pending_tasks.emplace_back(TaskType{ EVENT_TYPE::MOVE, TaskMove{EVENT_TYPE::ROTATE} });
			tick_data.SetRotateTick(0);
            return EVENT_TYPE::ROTATE;
        }
        return EVENT_TYPE::NONE;
    }

    case EVENT_TYPE::DROP: {
        while (true) {
            if (IsValidPosition(if_move_tetromino)) {
                current_tetromino = if_move_tetromino;
                ++if_move_tetromino.moved_pos.y;
            }
            else {
                send_pending_tasks.emplace_back(TaskType{ EVENT_TYPE::FIX, TaskFix{} });
				tick_data.SetDropTick(0);
                return EVENT_TYPE::FIX;
            }
        }
    }
    }
	return EVENT_TYPE::NONE;
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
        if (board[pos.y][pos.x] == true) return false;
    }
    return true;
}

void Tetris::FixTetromino()
{
    Position real_pos[4];

    for (int i = 0; i < 4; ++i) {
        real_pos[i].x = current_tetromino.default_pos[i].x + current_tetromino.moved_pos.x;
        real_pos[i].y = current_tetromino.default_pos[i].y + current_tetromino.moved_pos.y;
    }

    for (auto& pos : real_pos) {
        board[pos.y][pos.x] = true;
    }
}

std::vector<char> Tetris::ClearLine()
{
    std::vector<char> index_lines;

    // ✅ 줄 삭제는 "보이는 영역"만: RESERVE_HEIGHT ~ TOTAL_HEIGHT-1
    for (int y = HIDDEN_HEIGHT; y < TOTAL_HEIGHT; ++y) {
        if (std::all_of(board[y].begin(), board[y].end(), // 한 줄이 모두 true(채워짐)이면
            [](bool cell) { return cell; })) {
            std::fill(board[y].begin(), board[y].end(), false); // 현재 줄을 모두 false로 바꾸고

            // false 줄을 맨 위로 옮기고, 맨 위에서 1줄씩 아래로 당김
            // (숨겨진 0~RESERVE_HEIGHT-1 줄도 같이 아래로 내려오지만,
            //  줄 삭제 시의 "중력"은 전체 스택을 대상으로 적용)
            std::rotate(board.begin(), board.begin() + y, board.begin() + y + 1);
			index_lines.emplace_back(y);
			std::cout << "Cleared line index y = " << y << "\n";
        }
    }

    return index_lines;
}

std::vector<char> Tetris::GetGarbegeLineHoles(int cleard_line_num)
{
    int add_num = 0;
    // 지워진 라인에 따라 증가되는 라인 수가 다름
    switch (cleard_line_num) {
    case 1:
        add_num = 0;
        break;

    case 2:
        add_num = 1;
        break;

    case 3:
        add_num = 2;
        break;

    case 4:
        add_num = 4;
        break;

	default:
		add_num = 0;
    }

    std::vector<char> holes;
    for (int i = 0; i < add_num; ++i) {
        int hole_x = GetRandomHoleX();
        holes.emplace_back(static_cast<int>(hole_x));
	}

    return holes;
}

void Tetris::AddGarbageLines(int line_num, std::vector<TaskType>& send_pending_tasks)
{
	Tetromino copy_tetromino = current_tetromino;

    // 줄 추가작업
    for (size_t i = 0; i < line_num; i++){
        std::rotate(board.begin(), board.begin() + 1, board.end());
        std::fill(board[TOTAL_HEIGHT - 1].begin(), board[TOTAL_HEIGHT - 1].end(), true);
        int hole_x = GetRandomHoleX();
        board[TOTAL_HEIGHT - 1][hole_x] = false;

		TaskType add_line;
		add_line.event_type = EVENT_TYPE::ADDLINE;
        add_line.task = TaskAddLine{ .hole_x = hole_x };
        send_pending_tasks.emplace_back(add_line);

        if (CheckGameover()) {
            TaskType gameover;
            gameover.event_type = EVENT_TYPE::GAMEOVER;
			gameover.task = TaskGameover{};
			return;
        }

        // 테트로미노와 겹치면 안겹치게 테트로미노 위로 올리기
        if (!IsValidPosition(current_tetromino)) {
            ++current_tetromino.moved_pos.y;
            TaskType up_task;
            up_task.event_type = EVENT_TYPE::MOVE;
            up_task.task = TaskMove{EVENT_TYPE::UP};
            send_pending_tasks.emplace_back(up_task);
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
            board[i][j] = false;
        }
    } // 가로 = WIDTH = x / 세로 = HEIGHT = y

	pending_moves.fill(false);
	tick_data.InitTickData();
}

bool Tetris::CheckGameover()
{
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        for (int y = 0; y < HIDDEN_HEIGHT; ++y) {
            if (board[y][x] == true) {
				return true;
            }
        }
    }
    return false;
}

void Tetris::DebugPrintBoard()
{
    // 현재 테트로미노의 실제 보드 상 위치 계산
    std::array<Position, 4> real_tetromino_pos;
    for (int i = 0; i < 4; ++i) {
        real_tetromino_pos[i].x = current_tetromino.default_pos[i].x + current_tetromino.moved_pos.x;
        real_tetromino_pos[i].y = current_tetromino.default_pos[i].y + current_tetromino.moved_pos.y;
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
                std::cout << (board[y][x] ? "■" : "□");  // 고정된 블록 / 빈 칸
        }
        std::cout << "|\n";
    }

    std::cout << "====================================\n";
}

bool Tetris::CheckInputAllow(EVENT_TYPE move_type)
{
    switch (move_type)
    {
    case EVENT_TYPE::LEFT:
        if (tick_data.GetLeftTick() >= MOVE_TIMEOUT_TICK) return true;
        return false;

    case EVENT_TYPE::RIGHT:
        if (tick_data.GetRightTick() >= MOVE_TIMEOUT_TICK) return true;
        return false;

    case EVENT_TYPE::DOWN:
        if (tick_data.GetDownTick() >= MOVE_TIMEOUT_TICK) return true;
        return false;

    case EVENT_TYPE::ROTATE:
        if (tick_data.GetRotateTick() >= ROTATE_TIMEOUT_TICK) return true;
        return false;

    case EVENT_TYPE::DROP:
        if (tick_data.GetDropTick() >= DROP_TIMEOUT_TICK) return true;
        return false;

    default:
        return false;
    }
}

std::vector<TaskType> Tetris::TickProcess()
{
    std::vector<TaskType> send_pending_tasks;

    // 첫 번째로 줄 추가 처리
    if (tick_data.GetGarbageLineTick() >= tick_data.GetGarbageLineTimeout()) {
        tick_data.SetGarbageLineTick(0);
        AddGarbageLines(1, send_pending_tasks);
        for (auto& t : send_pending_tasks) {
            if (t.event_type == EVENT_TYPE::GAMEOVER) {
                return send_pending_tasks;
            }
        }
    }

    // 두 번째로 쌓인 입력 처리
    // 먼저 다운 타임아웃 이벤트 처리
    if (tick_data.GetDownTimeoutTick() >= tick_data.GetDownTimeout()) {
		tick_data.SetDownTimeoutTick(0);
        input_tasks.emplace_back(EVENT_TYPE::DOWN);
    }

    // 중복되는 이벤트 합치기
    for (auto& type : input_tasks) {
        int t = static_cast<int>(type);
        if (t < 0 || t >= INPUT_TYPE_NUM) {
            std::cout << "Tetris::TickProcess() - Invalid input task type: " << t << "\n";
            continue;
        }
        pending_moves[static_cast<int>(t)] = true;
    }

    // 합쳐진 이벤트들 처리
    for (int i = static_cast<int>(EVENT_TYPE::DROP); i >= static_cast<int>(EVENT_TYPE::RIGHT); --i) {
        if (pending_moves[i] == true) {
            EVENT_TYPE t = static_cast<EVENT_TYPE>(i);
			EVENT_TYPE result = HandleTetrominoKeyInput(t, send_pending_tasks);
            if (result == EVENT_TYPE::NONE) {
                continue;
            }
            else if (result == EVENT_TYPE::FIX){
				return send_pending_tasks;
            }
        }
    }

    
	return send_pending_tasks;
}

