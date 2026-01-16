import pygame
from resource_manager import ResourceManager
from network import NetworkWorker
from session import Session

class BaseState:
    def __init__(self, screen: pygame.Surface, rm: ResourceManager,
                  net_worker: NetworkWorker, session: Session):
        self.screen = screen
        self.rm = rm
        self.net_worker = net_worker
        self.session = session

    def handle_packets(self):
        pass

    def update(self):
        pass

    def draw(self):
        pass