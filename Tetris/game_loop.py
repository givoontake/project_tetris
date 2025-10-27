# game_loop.py
import time
import pygame
import queue
from define import BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT, INITIAL_SCALE, FPS
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

        self.am = AssetManager()
        self.am.init()
        self.net_worker = NetworkWorker()
        self.my_session = Session(self.am.block_asset)
        
        # 시작 시 서버 연결 시도 → ConnectState로 진입
        is_connect = self.net_worker.connect_to_server()
        self.state = ConnectState(self.screen, is_connect=is_connect, net_worker = self.net_worker)
        self.state.init()

        self.prev_time = time.perf_counter()
        self.frame_time = 1.0 / FPS 

        # 선택: 바쁜 대기 방지용 아주 짧은 sleep
        self.cpu_idle_time = 0.001 

    def handle_events(self):
        """공통 이벤트 처리(리사이즈/종료 등)."""
        events = pygame.event.get()

            # 🔍 [디버그 출력: 현재 상태 + 이벤트 리스트]
        for ev in events:
            if ev.type == pygame.MOUSEBUTTONDOWN:
                btn_name = {1: "L", 2: "M", 3: "R"}.get(ev.button, ev.button)
                print(f"  DOWN → button={btn_name}, pos={getattr(ev, 'pos', None)}")
                print(f"\n[STATE] {type(self.state).__name__}")

        for ev in events:
            if ev.type == pygame.QUIT:
                pygame.quit()
                raise SystemExit
            if ev.type == pygame.VIDEORESIZE:
                self.screen = pygame.display.set_mode((ev.w, ev.h), pygame.RESIZABLE)
                if hasattr(self.state, "on_resize"):
                    self.state.on_resize(ev.w, ev.h, self.screen)

        return events

    def update(self, dt, events):
        q = self.net_worker._pm.queue
        max_process = 30
        process_count = 0

        # --- 패킷 큐 드레인 (없으면 data=None으로 한 번만 업데이트) ---
        while process_count < max_process:
            try:
                data = q.get_nowait()
            except queue.Empty:
                data = None
            next_state = self.state.update(events, data) # 큐가 비어있어도 1번은 업데이트 하도록
            if next_state is not self.state:
                self.state = next_state
                if hasattr(self.state, "init"):
                    self.state.init()
                break # 상태가 변경되면 해당 프레임에서 처리 끝 (하나의 이벤트 스냅샷은 하나의 상태에 쓰이도록)
            process_count += 1
            if data is None:
                break

    def draw(self):
        self.state.draw()
        pygame.display.flip()

    def run(self):
        while True:
            # --- 시간 계산 ---
            now = time.perf_counter()
            dt = now - self.prev_time

            # --- 렌더링 (타이머 기반 고정 간격) ---
            if dt > self.frame_time:
                events = self.handle_events()
                self.update(dt, events)
                self.draw()
                self.prev_time = now


if __name__ == '__main__':
    GameLoop().run()