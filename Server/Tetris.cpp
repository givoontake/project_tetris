#include <vector>
#include <algorithm>
#include <random>
#include "Tetris.h"
#include "define_tetromino.h"

Tetris::Tetris()
{
    for (int i = 0; i < BOARD_HEIGHT; ++i) {
        for (int j = 0; j < BOARD_WIDTH; ++j) {
            board[i][j] = false;
        }
    } // 가로 = WIDTH = x / 세로 = HEIGHT = y
}

// 좌표 관리는 정의된 테트로미노 절대 좌표 + 키보드로 이동한 상대 좌표를 더해 현재 테트로미노 좌표를 구한다.
// 그러면 회전된 테트로미노 관리가 수월해진다.
bool Tetris::HandleTetrominoKeyInput(Tetromino& tetromino, MOVE_TYPE move_type) //bool 반환은 충돌 성공 시 다음 블록 스폰이 되어야 하는 것을 생각함
{
    Tetromino if_move_tetromino = tetromino;
    switch(move_type){
    case RIGHT:
        ++if_move_tetromino.moved_x;
        break;

    case LEFT:
        --if_move_tetromino.moved_x;
        break;

    case DOWN:
        ++if_move_tetromino.moved_y;
        break;

    case TIMEOUT:
        ++if_move_tetromino.moved_y;
        break;

    case ROTATE: {
        const std::vector<Tetromino>& shapes = TETROMINOS.at(if_move_tetromino.type);
        if (if_move_tetromino.shape_index + 1 < shapes.size())++if_move_tetromino.shape_index; // 다음 인덱스가 존재하면
        else if_move_tetromino.shape_index = 0; // 다음 인덱스가 없다면

        if_move_tetromino.default_pos = shapes[if_move_tetromino.shape_index].default_pos; // 회전된 인덱스로 테트로미노 변경
        break;
    }

    case DROP:{
        ++if_move_tetromino.moved_y; // 일단 똑같이 1칸 움직였을 때부터 판정 시작
    }

    }

    std::array<Position, 4> real_tetromino_pos;
    for (int i = 0; i < 4; ++i) {
        real_tetromino_pos[i].x = tetromino.default_pos[i].x + tetromino.moved_x;
        real_tetromino_pos[i].y = tetromino.default_pos[i].y + tetromino.moved_y;
    }

    std::array<Position, 4> real_moved_tetromino_pos;
    for (int i = 0; i < 4; ++i){
        real_moved_tetromino_pos[i].x = if_move_tetromino.default_pos[i].x + if_move_tetromino.moved_x;
        real_moved_tetromino_pos[i].y = if_move_tetromino.default_pos[i].y + if_move_tetromino.moved_y;
    }

    // 좌우 이동 및 회전은 착지 계산이 되면 안된다 -> 옆으로 끼워넣는 동작 등이 가능해야 함.
    // 착지(다운, 타임아웃)만 상태 변경이 가능해야 한다.
    for (auto& pos : real_moved_tetromino_pos) { // 아래 충돌 전에 판정을 하고, 충돌 후 바뀐 보드에 대해서 또 판정이 필요
        if (pos.x >= BOARD_WIDTH || pos.x < 0) return false; // 좌우로 벗어난 상태라면, 원래 위치로 돌아가야 한다.(움직임 인정 x)
        if (pos.y >= BOARD_HEIGHT || board[pos.y][pos.x] == true) { // 아래로 움직였다고 가정한 자리에 이미 블록이 있거나 바닥보다 아래라면, 이전 위치에 쌓여야 함.-> 인자로 받은 테트로미노
            if (move_type == DOWN || move_type == TIMEOUT || move_type == DROP) { // 1칸 드랍된 상황에서 잘못된 위치라면, 이전 위치에 쌓여야 함.
                for (auto& pos : real_tetromino_pos) {
                    board[pos.y][pos.x] = true;
                }
                return true;
            }
            else return false; // 회전인 경우 키 인정 x
        }
    }

    // 이동한 곳에서 어떤 충돌도 없다면
    tetromino = if_move_tetromino;

    // DROP 케이스는 충돌이 날 때까지 판정을 해서 쌓아줘야 함.
    if (move_type == DROP) {
        while (true) {
            ++if_move_tetromino.moved_y; // 똑같이 1칸 증가시킴

            // 판정을 위한 좌표 새로 업데이트
            for (int i = 0; i < 4; ++i) { 
                real_tetromino_pos[i].x = tetromino.default_pos[i].x + tetromino.moved_x;
                real_tetromino_pos[i].y = tetromino.default_pos[i].y + tetromino.moved_y;
            } 

            // 판정을 위한 좌표 새로 업데이트2
            for (int i = 0; i < 4; ++i) {
                real_moved_tetromino_pos[i].x = if_move_tetromino.default_pos[i].x + if_move_tetromino.moved_x;
                real_moved_tetromino_pos[i].y = if_move_tetromino.default_pos[i].y + if_move_tetromino.moved_y;
            }

            for (auto& pos : real_moved_tetromino_pos) {
                if (pos.y >= BOARD_HEIGHT || board[pos.y][pos.x] == true) { // 드랍이므로 좌우 판정은 필요없다. 
                    for (auto& pos : real_tetromino_pos) {
                        board[pos.y][pos.x] = true;
                    }
                    return true;
                }
            }
            tetromino = if_move_tetromino; // 드랍 케이스는 충돌이 날 때까지 판정해서 충돌이 나므로 무조건 true를 반환해야 한다.
        }
    }

    return false;
    // 타임아웃은 그냥 이 함수를 외부에서 호출하기 전에 타임을 초기화하고 인자로 타임아웃 넘기면 된다.
}

int Tetris::ClearLine()
{
    int count = 0;
    for (int y = 0; y < BOARD_HEIGHT; ++y) {
        if (std::all_of(board[y].begin(), board[y].end(), // 한 줄이 모두 true(채워짐)이면
            [](bool cell) { return cell; })) {
            std::fill(board[y].begin(), board[y].end(), false); // 현재 줄을 모두 false로 바꾸고
            std::rotate(board.begin(), board.begin() + y, board.begin() + y + 1); // false 줄을 맨 위로 옮기고, 맨 위에서 1줄씩 아래로 당김
            ++count;
        }
    }
    return count;
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
    for (int y = 0; y < BOARD_HEIGHT; ++y){
        if (std::any_of(board[y].begin(), board[y].end(), [](bool cell){ return cell; })) { // 반환되는 반복자가 end()가 아니라면 존재한다는 뜻, 즉 하나라도 true라면?
            // 처음으로 쌓여 있는 층을 찾으면, 그 층에 제일 높게 쌓인 블록이 존재하는 것
            stacked_top_index = y;
            break;
        }
    }

    if (stacked_top_index < 0) { // 블록이 맵에 1개도 없을 경우
        stacked_top_index = BOARD_HEIGHT - 1;
        for (int y = stacked_top_index; y > stacked_top_index - add_num; --y){
            std::fill(board[y].begin(), board[y].end(), true);
            int x = GetRandomX();
            board[y][x] = false;
        }
        return; // 쌓고 리턴
    }

    if (stacked_top_index - add_num < 0) {
        // 이러면 이 플레이어는 죽은 것 -> 나중에 네트워크 코드 추가(뮤텍스도 나중에 추가 필요)

        return;
    }

    else {
        int now_first_index = stacked_top_index;
        int now_end_index = BOARD_HEIGHT - 1;
        int dst_first_index = stacked_top_index - add_num; // 음수 체크는 위에서 하므로 out_of_index는 안나옴
        int dst_end_index = BOARD_HEIGHT - 1 - add_num;

        std::move(board.begin() + now_first_index, board.end(), board.begin() + dst_first_index);
        for (int y = now_end_index; y > dst_end_index; --y){ // 옮겨진 부분의 end 컨테이너는 유효 값으로 채워져 있음(헷갈리지 말기)
            std::fill(board[y].begin(), board[y].end(), true);
            int x = GetRandomX();
            board[y][x] = false;
        }
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
