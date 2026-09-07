import pygame

from tetris.resources.define_colors import *
from tetris.resources.resource_manager import ResourceManager
from tetris.ui.rectangle import Rectangle


def clamp(value: int, min_value: int, max_value: int) -> int:
    if value < min_value:
        return min_value
    if value > max_value:
        return max_value
    return value


class AdjustSound:
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager, volume_type: str):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.volume_type = volume_type

        self.is_dragging = False
        self.adjusted_volume = rm.sounds.volumes[volume_type]

        self.set_layout()

    def set_layout(self):
        name_x = self.rect.x
        name_y = self.rect.y
        name_w = int(self.rect.w*0.2)
        name_h = self.rect.h

        name_rect = pygame.Rect(name_x, name_y, name_w, name_h)
        self.name = Rectangle(self.screen, name_rect, self.rm, False, None, self.volume_type)
        self.name.set_background_color((0, 0, 0, 0))

        volume_value_rect = name_rect.copy()
        volume_value_rect.x += name_rect.w
        self.volume_value = Rectangle(self.screen, volume_value_rect, self.rm, False, None, str(self.adjusted_volume))
        self.volume_value.set_background_color((0, 0, 0, 0))

        self.sound_bar_frame_rect = volume_value_rect.copy()
        self.sound_bar_frame_rect.x += volume_value_rect.w
        self.sound_bar_frame_rect.w = self.rect.w - name_rect.w - volume_value_rect.w

        valid_box_w = int(self.sound_bar_frame_rect.w*0.9)
        valid_box_h = int(self.rect.h * 0.4)

        valid_box_x = self.sound_bar_frame_rect.x + (self.sound_bar_frame_rect.w - valid_box_w) // 2
        valid_box_y = self.sound_bar_frame_rect.y + (self.sound_bar_frame_rect.h - valid_box_h) // 2
        self.valid_box_rect = pygame.Rect(valid_box_x, valid_box_y, valid_box_w, valid_box_h) # 상호작용만 일어나는 invisible rect

        frame_padding_w = (self.sound_bar_frame_rect.w - valid_box_w) // 2 # 사운드 바의 양 끝 약간의 패딩

        volume_bar_w = self.valid_box_rect.w - frame_padding_w*2
        volume_bar_h = int(self.rect.h * 0.1)
        volume_bar_x = self.valid_box_rect.x + frame_padding_w
        volume_bar_y = self.valid_box_rect.centery - volume_bar_h // 2

        self.volume_bar_rect = pygame.Rect(volume_bar_x, volume_bar_y, volume_bar_w, volume_bar_h) # 전체 볼륨 바 rect
        
        init_adjust_x = int((self.volume_bar_rect.w / 100.0) * self.adjusted_volume)
        adjust_bar_x = valid_box_x + init_adjust_x
        adjust_bar_y = valid_box_y
        adjust_bar_h = valid_box_h
        adjust_bar_w = frame_padding_w*2 # 양 끝 패딩의 2배 길이만큼, 빈 공간 없이 값을 맞췄기 때문에 좌표 계산에 써도 된다.
        
        self.adjust_bar_rect = pygame.Rect(adjust_bar_x, adjust_bar_y, adjust_bar_w, adjust_bar_h) # 사운드 조절 막대를 왼쪽에 배치한다.

        self.set_volume_bar_layout()

    def set_volume_bar_layout(self):
        volume_bar1_w = self.adjust_bar_rect.centerx - self.volume_bar_rect.x
        volume_bar1_h = self.volume_bar_rect.h
        volume_bar1_x = self.volume_bar_rect.x
        volume_bar1_y = self.volume_bar_rect.y
        self.volume_bar1_rect = pygame.Rect(volume_bar1_x, volume_bar1_y, volume_bar1_w, volume_bar1_h)

        volume_bar2_w = self.volume_bar_rect.w - volume_bar1_w
        volume_bar2_h = self.volume_bar_rect.h
        volume_bar2_x = self.adjust_bar_rect.centerx
        volume_bar2_y = self.volume_bar_rect.y

        self.volume_bar2_rect = pygame.Rect(volume_bar2_x, volume_bar2_y, volume_bar2_w, volume_bar2_h)

        # 값 텍스트 반영
        self.volume_value.set_text(str(self.adjusted_volume))

    def init_adjust_bar(self):
        move_x = int((self.volume_bar_rect.w / 100.0) * self.adjusted_volume)
        self.adjust_bar_rect.move(move_x, 0)

    def move_adjust_bar(self, mouse_x: int):
        """
        mouse_x를 valid_box에서의 비율로 변환해 adjusted_volume(0~100)을 계산한다.
        """
        reactable_min_x = self.valid_box_rect.x
        reactable_max_x = self.valid_box_rect.x + self.valid_box_rect.w

        volume_bar_min_x = self.volume_bar_rect.x
        volume_bar_max_x = self.volume_bar_rect.x + self.volume_bar_rect.w

        if reactable_min_x <= mouse_x <= volume_bar_min_x:
            self.adjust_bar_rect.x = reactable_min_x

        elif reactable_min_x < mouse_x < volume_bar_max_x:
            self.adjust_bar_rect.x = mouse_x - self.adjust_bar_rect.w // 2
            
        elif volume_bar_max_x <= mouse_x <= reactable_max_x:
            self.adjust_bar_rect.x = reactable_max_x - self.adjust_bar_rect.w

        else:
            pass
        self.adjusted_volume = int((self.adjust_bar_rect.centerx - volume_bar_min_x) / float(volume_bar_max_x - volume_bar_min_x)*100)
        self.volume_value.set_text(str(self.adjusted_volume))

        self.set_volume_bar_layout()

    def handle_event(self, event: pygame.event.Event):
        """
        - 상호작용 시작은 valid_box_rect 내부에서만
        - 클릭 즉시 값 변경 + 누른 채 이동 시 드래그로 계속 변경
        """
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            mouse_x, mouse_y = event.pos
            if self.valid_box_rect.collidepoint(mouse_x, mouse_y):
                self.is_dragging = True
                self.move_adjust_bar(mouse_x)

        elif event.type == pygame.MOUSEMOTION and self.is_dragging:
            mouse_x, _ = event.pos
            self.move_adjust_bar(mouse_x)

        elif event.type == pygame.MOUSEBUTTONUP and event.button == 1:
            self.is_dragging = False

    def draw(self):
        self.name.draw()
        self.volume_value.draw()

        # 볼륨 표시 바(좌/우)
        pygame.draw.rect(self.screen, GREEN, self.volume_bar1_rect)   # 채워진 쪽
        pygame.draw.rect(self.screen, WHITE, self.volume_bar2_rect)  # 비워진 쪽

        # 슬라이더 블럭
        pygame.draw.rect(self.screen, GRAY, self.adjust_bar_rect)
