#pragma once
#include <iostream>
#include <vector>
#include <array>
#include <unordered_map>

constexpr char I = 0;
constexpr char J = 1;
constexpr char L = 2;
constexpr char O = 3;
constexpr char S = 4;
constexpr char T = 5;
constexpr char Z = 6;

struct Position {
    char x, y;
};

struct Tetromino {
    char type;             // I, J, L, O, S, T, Z
    char shape_index;            // 0 … (각 타입별 회전 상태 개수 − 1)
	Position moved_pos;
    std::array<Position, 4> default_pos;

    void PrintInfo() const
    {
        std::cout << "==== Current Tetromino Info ====\n";
        std::cout << "Type: " << (int)type << "\n";
        std::cout << "Shape Index: " << (int)shape_index << "\n";
        std::cout << "Moved Pos: (" << (int)moved_pos.x << ", "
            << (int)moved_pos.y << ")\n";

        std::cout << "Blocks (Real Positions):\n";
        for (int i = 0; i < 4; ++i) {
            int real_x = default_pos[i].x + moved_pos.x;
            int real_y = default_pos[i].y + moved_pos.y;

            std::cout << "  [" << i << "] default("
                << (int)default_pos[i].x << ", " << (int)default_pos[i].y
                << ") -> real(" << real_x << ", " << real_y << ")\n";
        }

        std::cout << "=========================\n";
    }

};

const std::unordered_map<char, std::vector<Tetromino>> TETROMINOS = {
    { I, {
        { I, 0, {0, 0}, {{ {0,1}, {1,1}, {2,1}, {3,1} }} },
        { I, 1, {0, 0}, {{ {2,0}, {2,1}, {2,2}, {2,3} }} }
    } },
    { J, {
        { J, 0, {0, 0}, {{ {0,0}, {0,1}, {1,1}, {2,1} }} },
        { J, 1, {0, 0}, {{ {1,0}, {2,0}, {1,1}, {1,2} }} },
        { J, 2, {0, 0}, {{ {0,1}, {1,1}, {2,1}, {2,2} }} },
        { J, 3, {0, 0}, {{ {1,0}, {1,1}, {0,2}, {1,2} }} }
    } },
    { L, {
        { L, 0, {0, 0}, {{ {2,0}, {0,1}, {1,1}, {2,1} }} },
        { L, 1, {0, 0}, {{ {1,0}, {1,1}, {1,2}, {2,2} }} },
        { L, 2, {0, 0}, {{ {0,1}, {1,1}, {2,1}, {0,2} }} },
        { L, 3, {0, 0}, {{ {0,0}, {1,0}, {1,1}, {1,2} }} }
    } },
    { O, {
        { O, 0, {0, 0}, {{ {1,0}, {2,0}, {1,1}, {2,1} }} }
    } },
    { S, {
        { S, 0, {0, 0}, {{ {1,1}, {2,1}, {0,2}, {1,2} }} },
        { S, 1, {0, 0}, {{ {1,0}, {1,1}, {2,1}, {2,2} }} }
    } },
    { T, {
        { T, 0, {0, 0}, {{ {1,0}, {0,1}, {1,1}, {2,1} }} },
        { T, 1, {0, 0}, {{ {1,0}, {1,1}, {2,1}, {1,2} }} },
        { T, 2, {0, 0}, {{ {0,1}, {1,1}, {2,1}, {1,2} }} },
        { T, 3, {0, 0}, {{ {1,0}, {0,1}, {1,1}, {1,2} }} }
    } },
    { Z, {
        { Z, 0, {0, 0}, {{ {0,1}, {1,1}, {1,2}, {2,2} }} },
        { Z, 1, {0, 0}, {{ {2,0}, {1,1}, {2,1}, {1,2} }} }
    } }
};

