# game_loop.py
import pygame
from define import BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT, INITIAL_SCALE, FPS
from change_game_state import *

class GameLoop:
    def __init__(self):
        pygame.init()
        w = int(BASE_SCREEN_WIDTH * INITIAL_SCALE)
        h = int(BASE_SCREEN_HEIGHT * INITIAL_SCALE)
        self.screen = pygame.display.set_mode((w, h), pygame.RESIZABLE)
        pygame.display.set_caption("IOCP Tetris Client")
        self.clock = pygame.time.Clock()

        # 게임 시작과 동시에 서버 연결 시도
        self.state = ConnectState(self.screen)
        self.state.init()

    def update(self):
        dt = self.clock.tick(FPS)
        events = pygame.event.get()

        for ev in events:
            if ev.type == pygame.VIDEORESIZE:
                self.screen = pygame.display.set_mode((ev.w, ev.h), pygame.RESIZABLE)
                if hasattr(self.state, "on_resize"):
                    self.state.on_resize(ev.w, ev.h, self.screen)
                else:
                    self.state.screen = self.screen
            elif ev.type == pygame.QUIT:
                pygame.quit(); raise SystemExit

        next_state = self.state.update(dt, events)
        if next_state is not self.state:
            self.state = next_state
            self.state.init()

    def draw(self):
        self.state.draw()
        pygame.display.flip()

    def run(self):
        while True:
            self.update()
            self.draw()

if __name__ == '__main__':
    GameLoop().run()
