from tetris.resources.resource_manager import *
from tetris.resources.fonts import Fonts
from tetris.resources.define_colors import *
from tetris.resources.define import *
from tetris.ui.rectangle import Rectangle
from tetris.ui.button import ButtonStyle


class ToggleButton(Rectangle):
    """활성 상태를 전환하는 버튼."""
    def __init__(self,
        screen: pygame.Surface,
        rect: pygame.Rect,
        rm: ResourceManager,
        *args
        ):
        text = ""
        border_width = 0
        use_image = True
        button_style = ButtonStyle.DEFAULT
        legacy_image = None

        if len(args) > 0:
            first_arg = args[0]
            if isinstance(first_arg, bool):
                use_image = first_arg
                if len(args) > 1:
                    legacy_image = args[1]
                if len(args) > 2:
                    text = args[2]
                if len(args) > 3:
                    border_width = args[3]
                button_style = self._get_legacy_button_style(rm, legacy_image)
            elif isinstance(first_arg, pygame.Surface) or first_arg is None:
                legacy_image = first_arg
                use_image = legacy_image is not None
                if len(args) > 1:
                    text = args[1]
                if len(args) > 2:
                    border_width = args[2]
                button_style = self._get_legacy_button_style(rm, legacy_image)
            else:
                text = first_arg
                if len(args) > 1:
                    border_width = args[1]
                if len(args) > 2:
                    use_image = args[2]
                if len(args) > 3:
                    button_style = args[3]

        super().__init__(screen, rect, rm, use_image, None, text, border_width)
        self.rm = rm
        self.use_image = use_image
        self.button_style = button_style
        self.reactable = True
        self.pressed = False
        if button_style == ButtonStyle.SMALL:
            self.idle_image = self.rm.images.ui_images[UI_BUTTON2_SKY]
            self.pressed_image = self.rm.images.ui_images[UI_BUTTON2_ORANGE]
        else:
            self.idle_image = self.rm.images.ui_images[UI_BUTTON_SKY]
            self.pressed_image = self.rm.images.ui_images[UI_BUTTON_ORANGE]
        if use_image:
            self.set_image(self.idle_image)

    def _get_legacy_button_style(self, rm: ResourceManager, legacy_image: pygame.Surface) -> ButtonStyle:
        if legacy_image is None:
            return ButtonStyle.DEFAULT
        if legacy_image is rm.images.ui_images[UI_BUTTON2_SKY]:
            return ButtonStyle.SMALL
        return ButtonStyle.DEFAULT

    def handle_event(self, ev: pygame.event.Event) -> bool:
        if self.reactable == False: return
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            if self.rect.collidepoint(ev.pos):
                if self.pressed:
                    pass
                else:
                    self.pressed = True
                    self.rm.sounds.sound_effects[EFFECT_BUTTON_PRESS].play()
                return True
        return False

    def set_pressed(self, is_pressed: bool):
        self.pressed = is_pressed

    def set_reactable(self, is_reactable: bool):
        self.reactable = is_reactable

    def draw(self):
        if self.use_image:
            if self.pressed:
                self.set_image(self.pressed_image)
            else:
                self.set_image(self.idle_image)
        super().draw()
