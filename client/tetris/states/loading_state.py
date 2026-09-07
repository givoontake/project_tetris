import pygame
from tetris.resources.resource_manager import ResourceManager
from tetris.net.network import NetworkWorker
from tetris.animation.loading_animation import LoadingAnimation
from tetris.states.base_state import BaseState

class LoadingState(BaseState):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager, net_worker: NetworkWorker):
        self.screen = screen
        self.rm = rm
        self.net_worker = net_worker
        sw, sh = self.screen.get_size()
        self.background = pygame.Surface((sw, sh), pygame.SRCALPHA)
        self.anim = LoadingAnimation(self.screen, self.rm)
        self.elapsed_time = 0

    def update(self, dt_ms, events):
        self.elapsed_time += dt_ms * 1000
        if self.elapsed_time > 100:
            self.elapsed_time =- 100
            self.anim.set_next_images()
            self.draw_flag = True
        pass

    def draw(self):
        self.background.fill(0,0,0,128)
        self.screen.blit(self.background, (0,0))
        self.anim.animation()
        pygame.display.flip()
