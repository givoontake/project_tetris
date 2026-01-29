import pygame
from tetris.resources.resource_manager import ResourceManager
from tetris.config.define import *

class ShutterAnimation:
    def __init__(self, screen: pygame.surface, rm: ResourceManager):
        self.screen = screen
        self.rm = rm
        self.shutter_image = self.rm.shutter_image
        self.shutter_sound = self.rm.shutter_sound
        self.x = 0
        self.y = 0
        self.dy = 0
        self.w = BASE_SCREEN_WIDTH
        self.h = BASE_SCREEN_HEIGHT
        self.dh = 0
        self.animaion_ms = 3000
        self.elapsed_time = 0
        self.is_active: bool = False

    def start_animation(self):
        self.is_active = True
        self.rm.shutter_sound.play()

    def update(self, dt_ms):
        if self.is_active == False: return
        
        delta = 0
        self.elapsed_time += dt_ms
        if self.animaion_ms < self.elapsed_time:
            delta = 1
            self.elapsed_time = self.animaion_ms
            self.is_active = False
        else:
            delta = self.elapsed_time / self.animaion_ms
        
        self.dy = delta*self.h
        self.dh = self.dy
        
    def draw(self):
        if self.is_active == False: return
        image_rect = pygame.Rect(self.x, self.dy, self.w, self.h - self.dh)
        self.screen.blit(self.shutter_image, (0,0), image_rect)