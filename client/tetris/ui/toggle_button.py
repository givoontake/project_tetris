from tetris.net.define_format import *
from tetris.resources.resource_manager import *
from tetris.resources.font_manager import FontManager
from tetris.resources.define_colors import *
from tetris.ui.rectangle import Rectangle

class ToggleButton(Rectangle):
    """활성/비활성 상태를 가지는 작은 토글 버튼."""
    def __init__(self, 
        screen: pygame.Surface,
        rect: pygame.Rect, 
        rm: ResourceManager,
        fm: FontManager,
        image: pygame.Surface = None, 
        text: str = "", 
        ):
        super().__init__(screen, rect, fm, image, text)
        self.rm = rm
        self.active = False

        self.pressed = False

    def handle_event(self, ev: pygame.event.Event) -> bool:
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            if self.rect.collidepoint(ev.pos):
                if self.pressed:
                    pass
                else:
                    self.pressed = True
                    self.rm.button_sound_press.play()
                return True
        return False

    def draw(self):
        if self.pressed:
            color = ORANGE
        else:
            color = GRAY

        self.set_background_color(color)
        super().draw()