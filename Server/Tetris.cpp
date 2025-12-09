#include <vector>
#include <algorithm>
#include <random>
#include "Tetris.h"
#include "define_tetromino.h"

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
bool Tetris::HandleTetrominoKeyInput(int move_type) //bool 반환은 충돌 성공 시 다음 블록 스폰이 되어야 하는 것을 생각함
{
    Tetromino if_move_tetromino = current_tetromino;
    switch (move_type) {
    case RIGHT:
        ++if_move_tetromino.moved_pos.x;
        break;

    case LEFT:
        --if_move_tetromino.moved_pos.x;
        break;

    case DOWN:
        ++if_move_tetromino.moved_pos.y;
        break;

    case TIMEOUT:
        ++if_move_tetromino.moved_pos.y;
        break;

    case ROTATE: {
        const std::vector<Tetromino>& shapes = TETROMINOS.at(if_move_tetromino.type);
        if (if_move_tetromino.shape_index + 1 < shapes.size()) ++if_move_tetromino.shape_index; // 다음 인덱스가 존재하면
        else if_move_tetromino.shape_index = 0; // 다음 인덱스가 없다면

        if_move_tetromino.default_pos = shapes[if_move_tetromino.shape_index].default_pos; // 회전된 인덱스로 테트로미노 변경
        break;
    }

    case DROP: {
        // 드랍 판정은 나중에 함
        break;
    }
    }

    std::array<Position, 4> real_tetromino_pos;
    for (int i = 0; i < 4; ++i) {
        real_tetromino_pos[i].x = current_tetromino.default_pos[i].x + current_tetromino.moved_pos.x;
        real_tetromino_pos[i].y = current_tetromino.default_pos[i].y + current_tetromino.moved_pos.y;
    }

    std::array<Position, 4> real_moved_tetromino_pos;
    for (int i = 0; i < 4; ++i) {
        real_moved_tetromino_pos[i].x = if_move_tetromino.default_pos[i].x + if_move_tetromino.moved_pos.x;
        real_moved_tetromino_pos[i].y = if_move_tetromino.default_pos[i].y + if_move_tetromino.moved_pos.y;
    }

    // 좌우 이동 및 회전은 착지 계산이 되면 안된다 -> 옆으로 끼워넣는 동작 등이 가능해야 함.
    // 착지(다운, 타임아웃)만 상태 변경이 가능해야 한다.
    for (auto& pos : real_moved_tetromino_pos) { // 아래 충돌 전에 판정을 하고, 충돌 후 바뀐 보드에 대해서 또 판정이 필요
		if (move_type == DROP) break; // 드랍은 아래에서 따로 판정
        if (pos.x >= BOARD_WIDTH || pos.x < 0)
            return false; // 좌우로 벗어난 상태라면, 원래 위치로 돌아가야 한다.(움직임 인정 x)

        // ✅ 바닥/쌓인 블록 판정 기준을 TOTAL_HEIGHT 로 변경
        if (pos.y >= TOTAL_HEIGHT || board[pos.y][pos.x] == true) {
            // 아래로 움직였다고 가정한 자리에 이미 블록이 있거나 바닥보다 아래라면, 이전 위치에 쌓여야 함.-> 인자로 받은 테트로미노
            if (move_type == DOWN || move_type == TIMEOUT || move_type == DROP) { // 1칸 드랍된 상황에서 잘못된 위치라면, 이전 위치에 쌓여야 함.
                for (auto& p : real_tetromino_pos) {
                    board[p.y][p.x] = true;
                }
                return true;
            }
            return false; // 회전인 경우 키 인정 x
        }
    }

    // 이동한 곳에서 어떤 충돌도 없다면
    current_tetromino = if_move_tetromino;

    // DROP 케이스는 충돌이 날 때까지 판정을 해서 쌓아줘야 함.
    if (move_type == DROP) {
        while (true) {
            ++if_move_tetromino.moved_pos.y; // 똑같이 1칸 증가시킴

            // 판정을 위한 좌표 새로 업데이트
            for (int i = 0; i < 4; ++i) {
                real_tetromino_pos[i].x = current_tetromino.default_pos[i].x + current_tetromino.moved_pos.x;
                real_tetromino_pos[i].y = current_tetromino.default_pos[i].y + current_tetromino.moved_pos.y;
            }

            // 판정을 위한 좌표 새로 업데이트2
            for (int i = 0; i < 4; ++i) {
                real_moved_tetromino_pos[i].x = if_move_tetromino.default_pos[i].x + if_move_tetromino.moved_pos.x;
                real_moved_tetromino_pos[i].y = if_move_tetromino.default_pos[i].y + if_move_tetromino.moved_pos.y;
            }

            for (auto& pos : real_moved_tetromino_pos) {
                // ✅ 여기서도 TOTAL_HEIGHT 기준으로 변경
                if (pos.y >= TOTAL_HEIGHT || board[pos.y][pos.x] == true) { // 드랍이므로 좌우 판정은 필요없다.
                    for (auto& p : real_tetromino_pos) {
                        board[p.y][p.x] = true;
                    }
                    return true;
                }
            }
            current_tetromino = if_move_tetromino; // 드랍 케이스는 충돌이 날 때까지 판정해서 충돌이 나므로 무조건 true를 반환해야 한다.
        }
    }

	move_allow = true;
    return false;
    // 타임아웃은 그냥 이 함수를 외부에서 호출하기 전에 타임을 초기화하고 인자로 타임아웃 넘기면 된다.
}

std::vector<char> Tetris::ClearLine()
{
    std::vector<char> index_lines;

    // ✅ 줄 삭제는 "보이는 영역"만: RESERVE_HEIGHT ~ TOTAL_HEIGHT-1
    for (int y = RESERVE_HEIGHT; y < TOTAL_HEIGHT; ++y) {
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

void Tetris::AddLine(int num)
{
    int add_num = 0;
    // 지워진 라인에 따라 증가되는 라인 수가 다름
    switch (num) {
    case 1:
        return;

    case 2:
        add_num = 1;
        break;

    case 3:
        add_num = 2;
        break;

    case 4:
        add_num = 4;
        break;
    }

    int stacked_top_index = -1;

    // ✅ 스택의 최상단은 전체 높이(TOTAL_HEIGHT) 기준으로 판정
    for (int y = 0; y < TOTAL_HEIGHT; ++y) {
        if (std::any_of(board[y].begin(), board[y].end(),
            [](bool cell) { return cell; })) {
            // 처음으로 쌓여 있는 층을 찾으면, 그 층에 제일 높게 쌓인 블록이 존재하는 것
            stacked_top_index = y;
            break;
        }
    }

    // 블록이 맵에 1개도 없을 경우
    if (stacked_top_index < 0) {
        stacked_top_index = TOTAL_HEIGHT - 1;
        for (int y = stacked_top_index; y > stacked_top_index - add_num; --y) {
            std::fill(board[y].begin(), board[y].end(), true);
            int x = GetRandomX();
            board[y][x] = false;
        }
        return; // 쌓고 리턴
    }

    // ✅ 추가 라인 때문에 맨 위를 뚫으면 사망
    if (stacked_top_index - add_num < 0) {
        // 이러면 이 플레이어는 죽은 것 -> 나중에 네트워크 코드 추가(뮤텍스도 나중에 추가 필요)
        return;
    }

    // 기존 스택을 위로 밀어올리고, 아래에 garbage line 추가
    int now_first_index = stacked_top_index;
    int now_end_index = TOTAL_HEIGHT - 1;
    int dst_first_index = stacked_top_index - add_num;          // 음수 체크는 위에서 하므로 out_of_index는 안나옴
    int dst_end_index = TOTAL_HEIGHT - 1 - add_num;

    std::move(board.begin() + now_first_index,
        board.begin() + now_end_index + 1,
        board.begin() + dst_first_index);

    for (int y = now_end_index; y > dst_end_index; --y) { // 옮겨진 부분의 end 컨테이너는 유효 값으로 채워져 있음(헷갈리지 말기)
        std::fill(board[y].begin(), board[y].end(), true);
        int x = GetRandomX();
        board[y][x] = false;
    }
}

int Tetris::GetRandomX()
{
    // OS/하드웨어 엔트로피에서 시드 생성
    static std::random_device rd;

    // 넣어준 시드값에 의해 생성될 난수가 준비된다.
    static std::mt19937 gen(rd());

    // 0~BOARD_WIDTH - 1 범위에서 균등 분포로 값 출력
    static std::uniform_int_distribution<int> dist(0, BOARD_WIDTH - 1);

    return dist(gen);
}

void Tetris::Clear()
{
    // ✅ 전체 보드(TOTAL_HEIGHT) 초기화
    for (int i = 0; i < TOTAL_HEIGHT; ++i) {
        for (int j = 0; j < BOARD_WIDTH; ++j) {
            board[i][j] = false;
        }
    } // 가로 = WIDTH = x / 세로 = HEIGHT = y

	move_allow = false;
}

bool Tetris::CheckGameover()
{
    for (int i = 0; i < RESERVE_HEIGHT; i++) {
        for (int j = 0; j < BOARD_WIDTH; ++j) {
            if (board[i][j] == true) {
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

