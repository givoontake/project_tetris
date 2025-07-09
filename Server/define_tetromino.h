#pragma once
#include <vector>
#include <unordered_map>

constexpr int I = 0;
constexpr int J = 1;
constexpr int L = 2;
constexpr int O = 3;
constexpr int S = 4;
constexpr int T = 5;
constexpr int Z = 6;

struct Position {
    int x, y;
};

struct Tetromino {
    int type;        // I, J, L, O, S, T, Z
    int rotation;    // 0 … (각 타입별 회전 상태 개수 − 1)
    Position pos[4]; // 상대 좌표
};

const std::unordered_map<int, std::vector<Tetromino>> TETROMINO_SHAPES = {
    { I, std::vector<Tetromino>{
        { I, 0, {{0,1}, {1,1}, {2,1}, {3,1}} },
        { I, 1, {{2,0}, {2,1}, {2,2}, {2,3}} }
    } },
    { J, std::vector<Tetromino>{
        { J, 0, {{0,0}, {0,1}, {1,1}, {2,1}} },
        { J, 1, {{1,0}, {2,0}, {1,1}, {1,2}} },
        { J, 2, {{0,1}, {1,1}, {2,1}, {2,2}} },
        { J, 3, {{1,0}, {1,1}, {0,2}, {1,2}} }
    } },
    { L, std::vector<Tetromino>{
        { L, 0, {{2,0}, {0,1}, {1,1}, {2,1}} },
        { L, 1, {{1,0}, {1,1}, {1,2}, {2,2}} },
        { L, 2, {{0,1}, {1,1}, {2,1}, {0,2}} },
        { L, 3, {{0,0}, {1,0}, {1,1}, {1,2}} }
    } },
    { O, std::vector<Tetromino>{
        { O, 0, {{1,0}, {2,0}, {1,1}, {2,1}} }
    } },
    { S, std::vector<Tetromino>{
        { S, 0, {{1,1}, {2,1}, {0,2}, {1,2}} },
        { S, 1, {{1,0}, {1,1}, {2,1}, {2,2}} }
    } },
    { T, std::vector<Tetromino>{
        { T, 0, {{1,0}, {0,1}, {1,1}, {2,1}} },
        { T, 1, {{1,0}, {1,1}, {2,1}, {1,2}} },
        { T, 2, {{0,1}, {1,1}, {2,1}, {1,2}} },
        { T, 3, {{1,0}, {0,1}, {1,1}, {1,2}} }
    } },
    { Z, std::vector<Tetromino>{
        { Z, 0, {{0,1}, {1,1}, {1,2}, {2,2}} },
        { Z, 1, {{2,0}, {1,1}, {2,1}, {1,2}} }
    } }
};
