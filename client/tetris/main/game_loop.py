import time
import queue
import pygame

from tetris.config.define import *
from tetris.states.login_state import LoginState
from tetris.net.network import NetworkWorker
from tetris.resources.resource_manager import ResourceManager
from tetris.net.session import Session
from tetris.resources.fonts import Fonts

class GameLoop:
    def __init__(self):
        pygame.init()
        pygame.mixer.init()
        pygame.mixer.music.set_volume(0.3)

        # 초기 화면 생성
        w = int(BASE_SCREEN_WIDTH * INITIAL_SCALE)
        h = int(BASE_SCREEN_HEIGHT * INITIAL_SCALE)
        self.screen = pygame.display.set_mode((w, h))
        pygame.display.set_caption("Tetris")
        pygame.key.start_text_input()

        self.rm = ResourceManager()
        self.fm = Fonts()
        self.net_worker = NetworkWorker()
        self.session = Session(self.rm)
        self.session.set_my_session()
        self.state = LoginState(self.screen, self.rm, self.net_worker, self.session)
        self.state.connect()

        self.prev_time = time.perf_counter()
        self.frame_time = 1.0 / FPS  # 초 단위

        self.cpu_idle_time = 0.001 

    def handle_events(self):
        # 공통 이벤트를 먼저 처리한다.
        events = pygame.event.get()

        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit()
                raise SystemExit

        return events
    
    def drain_packets(self):
        q = self.net_worker._pm.queue

        while not q.empty():
            if self.state.next_state is not None:
                break
            try:
                data = q.get_nowait()
            except queue.Empty:
                data = None

            self.state.handle_packet(data)

            if data is None:
                break

    def update(self, dt_ms, events):
        state_events = [] if self.state.is_input_blocked() else events
        next_state = self.state.update(dt_ms, state_events)
        if next_state is not None and next_state is not self.state:
            self.state = next_state
            self.state.start_fade_in()

    def draw(self):
        self.state.draw()
        self.state.draw_fade()
        pygame.display.flip()

    def run(self):
        while True:
            now = time.perf_counter()
            dt = now - self.prev_time  # 초 단위
            self.drain_packets()
            if dt > self.frame_time:
                dt_ms = dt * 1000.0
                events = self.handle_events()
                self.update(dt_ms, events)
                self.draw()
                self.prev_time = now


if __name__ == '__main__':
    GameLoop().run()
