import pygame
from tetris.resources.resource_manager import *
from enum import IntEnum

LARGE_SIZE = 50
MEDIUM_SIZE = 40
SMALL_SIZE = 30

class IMAGE_TYPE(IntEnum):
    LARGE = 1
    MEDIUM = 2
    SMALL = 3

# 파이썬의 함수들은 기본적으로 가상함수이다.
class Base:
    def __init__(self, screen: pygame.Surface, rm: ResourceManager):
        self.screen = screen
        self.rm = rm
        self.image_type: IMAGE_TYPE = IMAGE_TYPE.LARGE
        self.large_image: Optional[pygame.Surface] = None
        self.medium_image: Optional[pygame.Surface] = None
        self.small_image: Optional[pygame.Surface] = None
       
    def draw(self, x: int, y: int):
        draw_x = x
        draw_y = y
        if self.image_type == IMAGE_TYPE.LARGE:
            self.screen.blit(self.large_image, (draw_x, draw_y))

        elif self.image_type == IMAGE_TYPE.MEDIUM:
            offset = LARGE_SIZE - MEDIUM_SIZE / 2
            self.screen.blit(self.large_image, (draw_x + offset, draw_y + offset))

        elif self.image_type == IMAGE_TYPE.SMALL:
            offset = LARGE_SIZE - SMALL_SIZE / 2
            self.screen.blit(self.large_image, (draw_x + offset, draw_y + offset))

    def reset_image_type(self):
        self.image_type = IMAGE_TYPE.SMALL

class Red(Base):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager):
        super().__init__(screen, rm)
        self.image = self.rm.block_images[DEFAULT_RED]

        self.large_image = pygame.transform.smoothscale(self.image, (LARGE_SIZE, LARGE_SIZE))
        self.medium_image = pygame.transform.smoothscale(self.image, (MEDIUM_SIZE, MEDIUM_SIZE))
        self.small_image = pygame.transform.smoothscale(self.image, (SMALL_SIZE, SMALL_SIZE))

    def draw(self, x: int, y: int):
        super().draw(x, y)

class Orange(Base):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager):
        super().__init__(screen, rm)
        self.image = self.rm.block_images[DEFAULT_ORANGE]

        self.large_image = pygame.transform.smoothscale(self.image, (LARGE_SIZE, LARGE_SIZE))
        self.medium_image = pygame.transform.smoothscale(self.image, (MEDIUM_SIZE, MEDIUM_SIZE))
        self.small_image = pygame.transform.smoothscale(self.image, (SMALL_SIZE, SMALL_SIZE))

    def draw(self, x: int, y: int):
        super().draw(x, y)

class Yellow(Base):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager):
        super().__init__(screen, rm)
        self.image = self.rm.block_images[DEFAULT_YELLOW]

        self.large_image = pygame.transform.smoothscale(self.image, (LARGE_SIZE, LARGE_SIZE))
        self.medium_image = pygame.transform.smoothscale(self.image, (MEDIUM_SIZE, MEDIUM_SIZE))
        self.small_image = pygame.transform.smoothscale(self.image, (SMALL_SIZE, SMALL_SIZE))

    def draw(self, x: int, y: int):
        super().draw(x, y)

class Green(Base):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager):
        super().__init__(screen, rm)
        self.image = self.rm.block_images[DEFAULT_GREEN]

        self.large_image = pygame.transform.smoothscale(self.image, (LARGE_SIZE, LARGE_SIZE))
        self.medium_image = pygame.transform.smoothscale(self.image, (MEDIUM_SIZE, MEDIUM_SIZE))
        self.small_image = pygame.transform.smoothscale(self.image, (SMALL_SIZE, SMALL_SIZE))

    def draw(self, x: int, y: int):
        super().draw(x, y)

class Blue(Base):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager):
        super().__init__(screen, rm)
        self.image = self.rm.block_images[DEFAULT_BLUE]

        self.large_image = pygame.transform.smoothscale(self.image, (LARGE_SIZE, LARGE_SIZE))
        self.medium_image = pygame.transform.smoothscale(self.image, (MEDIUM_SIZE, MEDIUM_SIZE))
        self.small_image = pygame.transform.smoothscale(self.image, (SMALL_SIZE, SMALL_SIZE))

    def draw(self, x: int, y: int):
        super().draw(x, y)

class Indigo(Base):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager):
        super().__init__(screen, rm)
        self.image = self.rm.block_images[DEFAULT_INDIGO]

        self.large_image = pygame.transform.smoothscale(self.image, (LARGE_SIZE, LARGE_SIZE))
        self.medium_image = pygame.transform.smoothscale(self.image, (MEDIUM_SIZE, MEDIUM_SIZE))
        self.small_image = pygame.transform.smoothscale(self.image, (SMALL_SIZE, SMALL_SIZE))

    def draw(self, x: int, y: int):
        super().draw(x, y)

class Purple(Base):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager):
        super().__init__(screen, rm)
        self.image = self.rm.block_images[DEFAULT_PURPLE]

        self.large_image = pygame.transform.smoothscale(self.image, (LARGE_SIZE, LARGE_SIZE))
        self.medium_image = pygame.transform.smoothscale(self.image, (MEDIUM_SIZE, MEDIUM_SIZE))
        self.small_image = pygame.transform.smoothscale(self.image, (SMALL_SIZE, SMALL_SIZE))

    def draw(self, x: int, y: int):
        super().draw(x, y)

