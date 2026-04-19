import pygame
from typing import Optional
from tetris.ui.inputbox import InputBox
from tetris.resources.resource_manager import ResourceManager

class LabelFrame:
    def __init__(
        self,
        screen: pygame.Surface,
        image: pygame.Surface,     
        input_rect: pygame.Rect,
        label_rect: pygame.Rect,
        rm: ResourceManager,
        placeholder: str = "",
        max_input_len: int | None = None,  # optional과 같음
        is_password: bool = False,
        allow_korean: bool = True,   # ✅ 한글 허용 여부
    ):
        self.screen = screen
        self.frame_image = image
        self.frame_rect = label_rect
        self.input_box = InputBox(screen, input_rect, rm, placeholder, max_input_len, is_password, allow_korean, False)
        
    def _set_rect_scale(self, rect: pygame.Rect, scale_x: float, scale_y: float) -> pygame.Rect: # x = 가로, y = 세로 스케일
        if scale_x < 0.1 or scale_x > 2.0: # 최소 범위는 너무 작은 스케일을 방지하기 위함. 최대는 큰 스케일 방지를 위한 적당한 값임
            raise ValueError("scale_x must be between 0.1 and 2.0")
        if scale_y < 0.1 or scale_y > 2.0:
            raise ValueError("scale_y must be between 0.1 and 2.0")
        
        temp_rect = rect.copy()
        temp_rect.x = temp_rect.x + temp_rect.x*(1-scale_x)
        temp_rect.y = temp_rect.x + temp_rect.y*(1-scale_y)
        temp_rect.w = temp_rect.w*scale_x
        temp_rect.h = temp_rect.h*scale_y
    
        return temp_rect
    
    def move_pos(self, dx: int, dy: int):
        # sw, sh = self.screen.get_size()
        # if self.frame_rect.x + dx < 0 or self.frame_rect.x + dx + self.frame_rect.w > sw:
        #     raise ValueError("객체는 화면 좌우 외부로 잘릴 수 없습니다.")

        # if self.frame_rect.y + dy < 0 or self.frame_rect.y + dy + self.frame_rect.h > sh:
        #     raise ValueError("객체는 화면 세로 외부로 잘릴 수 없습니다.")
        
        self.frame_rect.x += dx
        self.input_box.rect.x += dx

        self.frame_rect.y += dy
        self.input_box.rect.y += dy

    def handle_event(self, ev: pygame.event.Event) -> Optional[str]:
        return self.input_box.handle_event(ev)
    
    def update(self, dt_ms: int):
        self.input_box.update(dt_ms)
    
    def draw(self):
        if self.frame_image is not None:
            self.screen.blit(self.frame_image, self.frame_rect)
        self.input_box.draw()

        
