import pygame
from tetris.resources.resource_manager import ResourceManager
from tetris.net.network import NetworkWorker
from tetris.net.session import Session

class BaseState:
    def __init__(self, screen: pygame.Surface, rm: ResourceManager, net_worker: NetworkWorker, session: Session):
        self.screen = screen
        self.rm = rm
        self.net_worker = net_worker
        self.session = session
        self.next_state = None

    def queue_state(self, next_state):
        self.next_state = next_state

    def consume_state(self):
        if self.next_state is not None:
            next_state = self.next_state
            self.next_state = None
            return next_state
        return self

    def handle_packets(self):
        pass

    def update(self):
        pass

    def draw(self):
        pass
