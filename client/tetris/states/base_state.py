import pygame
from tetris.resources.resource_manager import ResourceManager
from tetris.net.network import NetworkWorker
from tetris.net.session import Session

class BaseState:
    FADE_DURATION_MS = 500.0

    def __init__(self, screen: pygame.Surface, rm: ResourceManager, net_worker: NetworkWorker, session: Session):
        self.screen = screen
        self.rm = rm
        self.net_worker = net_worker
        self.session = session
        self.next_state = None
        self.fade_mode = None
        self.fade_elapsed_ms = 0.0
        self.fade_surface = pygame.Surface(self.screen.get_size())
        self.fade_surface.fill((0, 0, 0))

    def queue_state(self, next_state):
        if self.next_state is None:
            self.next_state = next_state
            self.start_fade_out()

    def consume_state(self):
        if self.next_state is not None and self.fade_mode == "out" and self.fade_elapsed_ms >= self.FADE_DURATION_MS:
            next_state = self.next_state
            self.next_state = None
            return next_state
        return self

    def start_fade_in(self):
        self.fade_mode = "in"
        self.fade_elapsed_ms = 0.0

    def start_fade_out(self):
        self.fade_mode = "out"
        self.fade_elapsed_ms = 0.0

    def update_fade(self, dt_ms: float):
        if self.fade_mode is None:
            return

        self.fade_elapsed_ms = min(self.fade_elapsed_ms + dt_ms, self.FADE_DURATION_MS)

        if self.fade_mode == "in" and self.fade_elapsed_ms >= self.FADE_DURATION_MS:
            self.fade_mode = None

    def is_input_blocked(self) -> bool:
        return self.fade_mode is not None

    def draw_fade(self):
        if self.fade_mode is None:
            return

        if self.fade_surface.get_size() != self.screen.get_size():
            self.fade_surface = pygame.Surface(self.screen.get_size())
            self.fade_surface.fill((0, 0, 0))

        progress = self.fade_elapsed_ms / self.FADE_DURATION_MS
        if self.fade_mode == "in":
            alpha = int(255 * (1.0 - progress))
        else:
            alpha = int(255 * progress)

        self.fade_surface.set_alpha(alpha)
        self.screen.blit(self.fade_surface, (0, 0))

    def handle_packets(self):
        pass

    def update(self):
        pass

    def draw(self):
        pass
