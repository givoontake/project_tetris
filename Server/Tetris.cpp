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
int Tetris::HandleTetrominoKeyInput(int move_type) //bool 반환은 충돌 성공 시 다음 블록 스폰이 되어야 하는 것을 생각함
{
    PrintMoveType(move_type);
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
        while (true) {
            if (IsValidPosition(if_move_tetromino)) {
                current_tetromino = if_move_tetromino;
                ++if_move_tetromino.moved_pos.y;
            }
            else return DROP;
        }
        break;
    }

	case UP:
        --current_tetromino.moved_pos.y;
        return -1;
        break;

    default:
        return -1;
    }

    if (IsValidPosition(if_move_tetromino)) {
        current_tetromino = if_move_tetromino;
        return move_type;
    }
    else {
        if (move_type == DOWN) return DROP;
        return -1;
    }
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

    //int stacked_top_index = -1;

    //// ✅ 스택의 최상단은 전체 높이(TOTAL_HEIGHT) 기준으로 판정
    //for (int y = 0; y < TOTAL_HEIGHT; ++y) {
    //    if (std::any_of(board[y].begin(), board[y].end(),
    //        [](bool cell) { return cell; })) {
    //        // 처음으로 쌓여 있는 층을 찾으면, 그 층에 제일 높게 쌓인 블록이 존재하는 것
    //        stacked_top_index = y;
    //        break;
    //    }
    //}

    //// 블록이 맵에 1개도 없을 경우
    //if (stacked_top_index > HIDDEN_HEIGHT) {
    //    stacked_top_index = TOTAL_HEIGHT - 1;
    //    for (int y = stacked_top_index; y > stacked_top_index - add_num; --y) {
    //        std::fill(board[y].begin(), board[y].end(), true);
    //        int x = GetRandomX();
    //        board[y][x] = false;
    //    }
    //    return; // 쌓고 리턴
    //}

    //else

    //// ✅ 추가 라인 때문에 맨 위를 뚫으면 사망
    //if (stacked_top_index - add_num < 0) {
    //    // 이러면 이 플레이어는 죽은 것 -> 나중에 네트워크 코드 추가(뮤텍스도 나중에 추가 필요)
    //    return;
    //}

    //// 기존 스택을 위로 밀어올리고, 아래에 garbage line 추가
    //int now_first_index = stacked_top_index;
    //int now_end_index = TOTAL_HEIGHT - 1;
    //int dst_first_index = stacked_top_index - add_num;          // 음수 체크는 위에서 하므로 out_of_index는 안나옴
    //int dst_end_index = TOTAL_HEIGHT - 1 - add_num;

    //std::move(board.begin() + now_first_index,
    //    board.begin() + now_end_index + 1,
    //    board.begin() + dst_first_index);

    //for (int y = now_end_index; y > dst_end_index; --y) { // 옮겨진 부분의 end 컨테이너는 유효 값으로 채워져 있음(헷갈리지 말기)
    //    std::fill(board[y].begin(), board[y].end(), true);
    //    int x = GetRandomX();
    //    board[y][x] = false;
    //}
}

std::vector<char> Tetris::AddGarbageLines(std::vector<char> holes)
{
	std::vector<char> added_holes;
    bool spqwn_flag = false;

	if (holes.empty()) return added_holes;
    for(auto hole_x : holes){
        if (HandleTetrominoKeyInput(DOWN) == DROP) {
            added_holes.emplace_back(FIX);
            spqwn_flag = true;
        }
        else HandleTetrominoKeyInput(UP);
		std::rotate(board.begin(), board.begin() + 1, board.end());
        std::fill(board[TOTAL_HEIGHT-1].begin(), board[TOTAL_HEIGHT-1].end(), true);
		board[TOTAL_HEIGHT-1][hole_x] = false;

        added_holes.emplace_back(hole_x);

        if (CheckGameover()) {
            added_holes.emplace_back(GAMEOVER);
            return added_holes;
        }
	}
    if (spqwn_flag)  added_holes.emplace_back(SPAWN);
    
    return added_holes;
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

