# game_loop.py
import pygame
from define import BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT, INITIAL_SCALE, FPS
from change_game_state import*

class GameLoop:
    def __init__(self):
        pygame.init()
        w = int(BASE_SCREEN_WIDTH * INITIAL_SCALE)
        h = int(BASE_SCREEN_HEIGHT * INITIAL_SCALE)
        self.screen = pygame.display.set_mode((w, h), pygame.RESIZABLE)
        self.clock = pygame.time.Clock()

        # 초기 상태 설정
        self.state = SelectModeState(self.screen)
        self.state.init()

    def update(self):
        dt = self.clock.tick(FPS)
        events = pygame.event.get()

        # 전역 이벤트 처리: 리사이즈, Quit
        for ev in events:
            if ev.type == pygame.VIDEORESIZE:
                self.screen = pygame.display.set_mode((ev.w, ev.h), pygame.RESIZABLE)
                # 상태가 훅을 제공하면 레이아웃 갱신을 상태에 위임
                if hasattr(self.state, "on_resize"):
                    self.state.on_resize(ev.w, ev.h, self.screen)
                else:
                    # 하위호환: 최소한 화면 참조만 갱신
                    self.state.screen = self.screen
            elif ev.type == pygame.QUIT:
                pygame.quit()
                exit()

        # 상태별 업데이트(이벤트 전달)
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
