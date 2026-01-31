import pygame
import struct

from tetris.net.packet_type import *
from tetris.net.network import NetworkWorker
from tetris.net.session import Session
from tetris.states.single_play import RIGHT, LEFT, ROTATE, DOWN, DROP, UP

class TetrisController:
    def __init__(self, net_worker: NetworkWorker):
        self.net_worker = net_worker

        # 컨트롤 제어 변수들
        self.left_pressed = False
        self.right_pressed = False
        self.down_pressed = False
        self.rotate_pressed = False
        self.drop_pressed = False

        self.first_delay_ms = 300
        self.delay_ms = 50

        self.left_elapsed_time = 0
        self.left_first_over = False
        self.left_first_move = False
        self.right_elapsed_time = 0
        self.right_first_over = False
        self.right_first_move = False
        self.down_elapsed_time = 0
        self.down_first_over = False
        self.down_first_move = False

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.KEYDOWN or ev.type == pygame.KEYUP:
            if ev.key == pygame.K_LEFT:
                # move_type = LEFT
                if ev.type == pygame.KEYDOWN: 
                    self.left_pressed = True
                elif ev.type == pygame.KEYUP: 
                    self.left_pressed = False
                    self.left_first_over = False
                    self.left_first_move = False
                    self.left_elapsed_time = 0

            elif ev.key == pygame.K_RIGHT:
                # move_type = RIGHT
                if ev.type == pygame.KEYDOWN: 
                    self.right_pressed = True
                elif ev.type == pygame.KEYUP: 
                    self.right_pressed = False
                    self.right_first_over = False
                    self.right_first_move = False
                    self.right_elapsed_time = 0

            # 소프트 드랍
            elif ev.key == pygame.K_DOWN:
                # move_type = DOWN
                if ev.type == pygame.KEYDOWN:
                    self.down_pressed = True
                elif ev.type == pygame.KEYUP:
                    self.down_pressed = False
                    self.down_first_over = False
                    self.down_first_move = False
                    self.down_elapsed_time = 0

            # 하드 드랍(스페이스)
            elif ev.key == pygame.K_SPACE:
                if ev.type == pygame.KEYDOWN: self.drop_pressed = True
                elif ev.type == pygame.KEYUP: self.drop_pressed = False
                # move_type = DROP

            # 회전(위)
            elif ev.key == pygame.K_UP:
                if ev.type == pygame.KEYDOWN: self.rotate_pressed = True
                elif ev.type == pygame.KEYUP: self.rotate_pressed = False

    # ------------ 네트워크 연동용 함수 (키 입력 → C2S_MOVE) ------------ # 

    def send_move_handler(self):
        if self.left_pressed:
            if self.left_first_over == False:
                if self.left_first_move == False:
                    self.send_move(LEFT)
                    self.left_first_move = True
                    
                if self.left_elapsed_time >= self.first_delay_ms:
                    self.send_move(LEFT)
                    self.left_first_over = True
                    self.left_elapsed_time = 0

            else:
                if self.left_elapsed_time >= self.delay_ms:
                    self.send_move(LEFT)
                    self.left_elapsed_time = 0
            

        if self.right_pressed:
            if self.right_first_over == False:
                if self.right_first_move == False:
                    self.send_move(RIGHT)
                    self.right_first_move = True

                if self.right_elapsed_time >= self.first_delay_ms:
                    self.send_move(RIGHT)
                    self.right_first_over = True
                    self.right_elapsed_time = 0

            else:
                if self.right_elapsed_time >= self.delay_ms:
                    self.send_move(RIGHT)
                    self.right_elapsed_time = 0

        # 소프트 드랍
        if self.down_pressed:
            if self.down_first_over == False:
                if self.down_first_move == False:
                    self.send_move(DOWN)
                    self.down_first_move = True
                if self.down_elapsed_time >= self.first_delay_ms:
                    self.send_move(DOWN)
                    self.down_first_over = True
                    self.down_elapsed_time = 0

            else:
                if self.down_elapsed_time >= self.delay_ms:
                    self.send_move(DOWN)
                    self.down_elapsed_time = 0
        # 하드 드랍(스페이스)
        if self.rotate_pressed:
            self.send_move(ROTATE)
            self.rotate_pressed = False
        # 회전(위)
        if self.drop_pressed:
            self.send_move(DROP)
            self.left_pressed = False
            self.right_pressed = False
            self.down_pressed = False
            self.rotate_pressed = False
            self.drop_pressed = False

    def send_move(self, move_type):
        size = 2 + 1 + 1
        type = C2S_MOVE
        self.move_type = move_type

        packet_bytes = struct.pack(
            "<hbb",
            size,
            type,
            self.move_type
        )

        # MOVE_NAME = {LEFT: "LEFT", RIGHT: "RIGHT", DOWN: "DOWN", DROP: "DROP", ROTATE: "ROTATE"}
        # print("[C2S_MOVE] Send move_type =", MOVE_NAME.get(self.move_type, self.move_type))

        try:
            self.net_worker.send_packet(packet_bytes)
        except Exception as e:
            print("[TetrisController] send_move() error:", e)

    def clear(self):
        self.left_pressed = False
        self.right_pressed = False
        self.down_pressed = False
        self.rotate_pressed = False
        self.drop_pressed = False

        self.left_elapsed_time = 0
        self.left_first_over = False
        self.left_first_move = False
        self.right_elapsed_time = 0
        self.right_first_over = False
        self.right_first_move = False
        self.down_elapsed_time = 0
        self.down_first_over = False
        self.down_first_move = False

    def update(self, dt_ms):
        if self.left_pressed: self.left_elapsed_time += dt_ms
        if self.right_pressed: self.right_elapsed_time += dt_ms
        if self.down_pressed: self.down_elapsed_time += dt_ms

        self.send_move_handler()
