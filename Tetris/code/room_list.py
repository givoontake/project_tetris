import pygame
from font_manager import FontManager
from resource_manager import ResourceManager

class RoomList:
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager, fm: FontManager):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.fm = fm

        
        