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
        pygame.display.set_caption("Tetris")
        self.clock = pygame.time.Clock()
        self.net_worker = NetworkWorker()
        self.session = None
        # 게임 시작과 동시에 서버 연결 시도
        self.state = None
        # self.state.init()

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

    def process_queue(self):

    # 현재 state가 들고 있는 네트워크 객체에서 큐를 찾는다.
        q = None
        net = getattr(self.state, "net", None)
        if net is not None:
            # (선호) 접근자 메서드가 있으면 사용
            get_q = getattr(net, "get_packet_queue", None)
            if callable(get_q):
                q = get_q()
            else:
                # (백업) PacketManager 내부 큐 직접 접근 (현재 프로젝트는 이 경로)
                pm = getattr(net, "_pm", None)
                if pm is not None:
                    q = getattr(pm, "queue", None)

        if q is None:
            return  # 큐가 없으면 할 일 없음

        # 큐 드레인 (비블로킹)
        items = []
        while True:
            try:
                items.append(q.get_nowait())
            except _q.Empty:
                break

        if not items:
            return

        # 상태 업데이트에 패킷 리스트를 인자로 전달 시도: update(dt, events, packets)
        try:
            self.state.update(0, [], items)
        except TypeError:
            # 시그니처가 다르면 on_queue 콜백을 우선 시도
            on_queue = getattr(self.state, "on_queue", None)
            if callable(on_queue):
                for it in items:
                    on_queue(it)
            else:
                # 최후: 상태 객체에 임시로 적재하여 상태 내부에서 소비하도록 함
                buf = getattr(self.state, "queued_items", [])
                buf.extend(items)
                setattr(self.state, "queued_items", buf)


if __name__ == '__main__':
    res = GameLoop().net_worker.connect_to_server()
    GameLoop.state = ConnectState(res)
    GameLoop().run()
