import pygame
from tetris.resources.resource_manager import ResourceManager
from tetris.resources.fonts import *
from tetris.resources.define_colors import *
from tetris.config.define import *

class Rectangle:
    def _get_default_font_size(self) -> int:
        if self.rect.h == 50:
            return MINI_FONT_SIZE
        return DEFAULT_FONT_SIZE

    def __init__(self, 
        screen: pygame.Surface,
        rect: pygame.Rect, 
        rm: ResourceManager,
        use_image: bool = False,
        image: pygame.Surface = None, 
        text: str = "", 
        border_width = 0
        ):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.use_auto_text_size = True
        self.font = rm.fonts.get_font(self._get_default_font_size())
        self.image = None
        self.image_source = None
        self.icon = None
        self.icon_source = None
        if use_image and image is not None: 
            self.set_image(image)
        self.text = text
        self.text_color: tuple[int, int, int] = WHITE
        self.background_color: tuple[int, int, int] = BLACK
        self.border_color: tuple[int, int, int] = WHITE
        self.font_surface = self.font.render(self.text, False, self.text_color)

        self.border_width = border_width
        self.visible = True

    # def get_rect(self) -> pygame.Rect: 
    #     return self.rect
    
    def set_background_color(self, color = tuple[int, int, int]): # r, g, b
        self.background_color = color

    def set_text_color(self, color = tuple[int, int, int]): # r, g, b
        self.text_color = color
        self.font_surface = self.font.render(self.text, False, self.text_color)

    def set_image(self, new_image: pygame.Surface):
        self.image_source = new_image
        scaled_image = pygame.transform.smoothscale(new_image, (self.rect.w, self.rect.h))
        self.image = scaled_image

    def set_icon(self, new_icon: pygame.Surface = None):
        self.icon_source = new_icon
        if new_icon is None:
            self.icon = None
            return

        icon_len = int(min(self.rect.w, self.rect.h) * 0.6)
        if icon_len <= 0:
            self.icon = None
            return

        self.icon = pygame.transform.smoothscale(new_icon, (icon_len, icon_len))

    def set_font(self, new_font: pygame.font.Font):
        self.use_auto_text_size = False
        self.font = new_font
        self.font_surface = self.font.render(self.text, False, self.text_color)
        
    def set_text(self, new_text: str):
        self.text = new_text
        self.font_surface = self.font.render(self.text, False, self.text_color)

    def set_text_size(self, new_size: int):
        self.use_auto_text_size = False
        self.font = self.rm.fonts.get_font(new_size)
        self.font_surface = self.font.render(self.text, False, self.text_color)

    def set_border(self, new_border_width: int): # 0이면 테두리 없음. 0보다 크면 그 두께만큼 테두리 생성
        self.border_width = new_border_width

    def _get_center_pos(self) -> tuple[int, int]: # x, y
        center_x = self.rect.x + self.rect.w // 2
        center_y = self.rect.y + self.rect.h // 2
        return center_x, center_y
    
    def update_rect(self, new_rect: pygame.Rect):
        self.rect = new_rect
        if self.image_source is not None:
            self.set_image(self.image_source)
        if self.icon_source is not None:
            self.set_icon(self.icon_source)
        if self.use_auto_text_size:
            self.font = self.rm.fonts.get_font(self._get_default_font_size())
            self.font_surface = self.font.render(self.text, False, self.text_color)

    def handle_event(self, ev: pygame.event.Event) -> Optional[str]:
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            if self.rect.collidepoint(ev.pos):
                return self.text
            else: return None
    
    def draw(self):
        if self.visible == False: return
        if self.image == None:
            if len(self.background_color) == 4:
                bg_surface = pygame.Surface((self.rect.w, self.rect.h), pygame.SRCALPHA)
                bg_surface.fill(self.background_color)
                self.screen.blit(bg_surface, self.rect.topleft)
            else:
                pygame.draw.rect(self.screen, self.background_color, self.rect)
            if self.border_width > 0:
                pygame.draw.rect(self.screen, self.border_color, self.rect, self.border_width)
        else:
            self.screen.blit(self.image, self.rect)

        if self.icon is not None:
            icon_rect = self.icon.get_rect()
            icon_rect.center = self.rect.center
            self.screen.blit(self.icon, icon_rect)

        font_rect = self.font_surface.get_rect()
        center_x, center_y = self._get_center_pos()
        draw_font_x = center_x - font_rect.w / 2
        draw_font_y = center_y - font_rect.h / 2
        font_rect.x = draw_font_x
        font_rect.y = draw_font_y
        self.screen.blit(self.font_surface, font_rect)        
    
