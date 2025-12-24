# game_loop.py
import time
import pygame
import queue
from define import *
from change_game_state import *
from network import NetworkWorker
from asset_manager import *

class GameLoop:
    def __init__(self):
        pygame.init()

        # 초기 화면 생성
        w = int(BASE_SCREEN_WIDTH * INITIAL_SCALE)
        h = int(BASE_SCREEN_HEIGHT * INITIAL_SCALE)
        self.screen = pygame.display.set_mode((w, h), pygame.RESIZABLE)
        pygame.display.set_caption("Tetris")
        pygame.key.start_text_input()

        self.am = AssetManager()
        self.am.init()
        self.net_worker = NetworkWorker()
        
        self.state = LoginState(self.screen, self.am, net_worker=self.net_worker)
        self.state.connect()

        self.prev_time = time.perf_counter()
        self.frame_time = 1.0 / FPS  # 초 단위

        # 선택: 바쁜 대기 방지용 아주 짧은 sleep
        self.cpu_idle_time = 0.001 

    def handle_events(self):
        # 공통 이벤트만 우선 거르기
        events = pygame.event.get()

        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit()
                raise SystemExit
            if ev.type == pygame.VIDEORESIZE:
                self.screen = pygame.display.set_mode((ev.w, ev.h), pygame.RESIZABLE)
                if hasattr(self.state, "on_resize"):
                    self.state.on_resize(ev.w, ev.h, self.screen)

        return events
    
    def drain_packets(self):
        q = self.net_worker._pm.queue

        while not q.empty():
            try:
                data = q.get_nowait()
            except queue.Empty:
                data = None

            next_state = self.state.handle_packet(data)            
            if next_state is not self.state:
                self.state = next_state

            if data is None:
                break

    def update(self, dt_ms, events):
        self.state.update(dt_ms, events) 
        # if hasattr(self.state, "init"):
        #     self.state.init()

    def draw(self):
        self.state.draw()
        pygame.display.flip()

    def run(self):
        while True:
            now = time.perf_counter()
            dt = now - self.prev_time  # 초 단위
            self.drain_packets()
            if dt > self.frame_time:
                dt_ms = dt * 1000.0      # ✅ ms 단위로 변환
                events = self.handle_events()
                self.update(dt_ms, events)
                self.draw()
                self.prev_time = now


if __name__ == '__main__':
    GameLoop().run()
