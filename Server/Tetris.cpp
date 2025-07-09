#include "Tetris.h"

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
void Tetris::CheckCollision(Tetromino tetromino, MOVE_TYPE type, int x, int y) // (테트로미노 / 키보드 명령 / 상대 좌표 x, 상대 좌표 y)
{
    Tetromino if_tetromino = tetromino;
    switch(type){
    case RIGHT:
        for (auto& t : if_tetromino.pos) {
            ++t.x;
        }
        break;

    case LEFT:
        for (auto& t : if_tetromino.pos) {
            --t.x;
        }
        break;

    case DOWN:
        for (auto& t : if_tetromino.pos) {
            ++t.y;
        }
        break;

    case ROTATE:
        
        break;
    default:
        break;
    }
    for (auto& t : if_tetromino.pos) { // 아래 충돌 전에 판정을 하고, 충돌 후 바뀐 보드에 대해서 또 판정이 필요
        if (t.x + 1 < BOARD_HEIGHT) {

        }
    }
}
