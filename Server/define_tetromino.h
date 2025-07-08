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
	Position pos[4];
};

struct TetrominoShape {
    std::vector<Tetromino> shapes;
};

const std::unordered_map<int, TetrominoShape> TETROMINO_SHAPES = {
    { I, TetrominoShape{ {
        Tetromino{{ Position{0,1}, Position{1,1}, Position{2,1}, Position{3,1} }},
        Tetromino{{ Position{2,0}, Position{2,1}, Position{2,2}, Position{2,3} }}
    } } },
    { J, TetrominoShape{ {
        Tetromino{{ Position{0,0}, Position{0,1}, Position{1,1}, Position{2,1} }},
        Tetromino{{ Position{1,0}, Position{2,0}, Position{1,1}, Position{1,2} }},
        Tetromino{{ Position{0,1}, Position{1,1}, Position{2,1}, Position{2,2} }},
        Tetromino{{ Position{1,0}, Position{1,1}, Position{0,2}, Position{1,2} }}
    } } },
    { L, TetrominoShape{ {
        Tetromino{{ Position{2,0}, Position{0,1}, Position{1,1}, Position{2,1} }},
        Tetromino{{ Position{1,0}, Position{1,1}, Position{1,2}, Position{2,2} }},
        Tetromino{{ Position{0,1}, Position{1,1}, Position{2,1}, Position{0,2} }},
        Tetromino{{ Position{0,0}, Position{1,0}, Position{1,1}, Position{1,2} }}
    } } },
    { O, TetrominoShape{ {
        Tetromino{{ Position{1,0}, Position{2,0}, Position{1,1}, Position{2,1} }}
    } } },
    { S, TetrominoShape{ {
        Tetromino{{ Position{1,1}, Position{2,1}, Position{0,2}, Position{1,2} }},
        Tetromino{{ Position{1,0}, Position{1,1}, Position{2,1}, Position{2,2} }}
    } } },
    { T, TetrominoShape{ {
        Tetromino{{ Position{1,0}, Position{0,1}, Position{1,1}, Position{2,1} }},
        Tetromino{{ Position{1,0}, Position{1,1}, Position{2,1}, Position{1,2} }},
        Tetromino{{ Position{0,1}, Position{1,1}, Position{2,1}, Position{1,2} }},
        Tetromino{{ Position{1,0}, Position{0,1}, Position{1,1}, Position{1,2} }}
    } } },
    { Z, TetrominoShape{ {
        Tetromino{{ Position{0,1}, Position{1,1}, Position{1,2}, Position{2,2} }},
        Tetromino{{ Position{2,0}, Position{1,1}, Position{2,1}, Position{1,2} }}
    } } }
};