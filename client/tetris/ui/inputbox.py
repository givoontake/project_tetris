import pygame
from typing import Optional

from tetris.resources.resource_manager import ResourceManager
from tetris.resources.define import *
from tetris.resources.define_colors import *
from tetris.config.define import *

PLACEHOLDER = (100, 100, 100)


class InputBox:
    def __init__(
        self,
        screen: pygame.Surface,
        rect: pygame.Rect,
        rm: ResourceManager,
        placeholder: str = "",
        max_input_len: int | None = None,  # optional怨?媛숈쓬
        is_password: bool = False,
        allow_korean: bool = True,   # ???쒓? ?덉슜 ?щ?
        use_holder: bool = True,
        holder_key: int = UI_TEXT_HOLDER,
    ):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.placeholder = placeholder
        self.text_h = int(rect.h * 0.7)
        self.text = ""
        self.editing_text = ""
        self.active = False
        self.is_password = is_password
        self.allow_korean = allow_korean
        self.max_input_len = max_input_len
        self.padding = int(self.text_h / 2)
        self.color = GRAY
        self.font = self.rm.fonts.get_font(self.text_h)
        self.use_holder = use_holder
        self.holder_image = self.rm.images.ui_images[holder_key] if use_holder else None

        # 而ㅼ꽌 ?먮㈇
        self.cursor_visible = True
        self.cursor_timer_ms = 0
        self.cursor_blink_ms = 500

        # ???낅젰? ?덈룄???낅젰湲곕? ?듯빐 泥섎━?섏?留??꾩꽦??湲?먮쭔 諛섑솚?섎?濡?吏?곌린???곕줈 泥섎━?댁빞??
        self.backspace_pressed = False
        #self.backspace_repeat_active = False
        self.backspace_repeat_timer = 0
        self.backspace_repeat_time1 = 500
        self.backspace_repeat_time1_active = True
        self.backspace_repeat_time2 = 50
        self.backspace_repeat_time2_active = False

    def _reset_text(self):
        self.text = ""
        self.editing_text = ""

    def _reset_cursor(self):
        self.backspace_repeat_timer = 0
        self.backspace_repeat_time1_active = True
        self.backspace_repeat_time2_active = False
        self.backspace_pressed = False

    def get_inputbox_rect(self) -> pygame.Rect:
        return self.backspace_repeat_time1_active
    
    def extract_text(self) -> str:
        text = self.get_total_text()
        self._reset_text()
        self._reset_cursor()
        return text

    def add_char(self, ch: str):
        if not self.active:
            return
        # 鍮?臾몄옄, ?쒖뼱臾몄옄 ?꾪꽣留?
        if not ch or not ch.isprintable():
            return
        # ?쒓? 鍮꾪뿀?⑹씠硫?ASCII留?諛쏆쓬
        if not self.allow_korean and not ch.isascii():
            return
        # 湲몄씠 珥덇낵 諛⑹?
        if self.max_input_len is not None and len(self.text) >= self.max_input_len:
            return
        
        if not ch.isascii():
            self.backspace_repeat_time1_active = False
            self.backspace_repeat_time2_active = True
        # ?듦낵?덉쑝硫?異붽?
        self.text += ch

    def delete_char(self):
        if not self.active:
            return
        if self.text:
            self.text = self.text[:-1]

    def get_text_width(self, text: str) -> int:
        width, _ = self.font.size(text)
        return width
    
    def get_total_text(self) -> str:
        total_text = ""
        if len(self.text) < self.max_input_len:
            total_text = self.text + self.editing_text
        else:
            total_text = self.text
        
        # if self.is_password:
        #     total_text = "*" * len(total_text)

        return total_text

    def get_render_text(self) -> str:
        text_px_len = None
        render_text = ""
        if len(self.text) < self.max_input_len:
            render_text = self.text + self.editing_text
            text_px_len = self.get_text_width(self.text) + self.get_text_width(self.editing_text)
        else:
            render_text = self.text
            text_px_len = self.get_text_width(self.text)

        box_px_len = self.rect.w - self.padding * 2# 醫뚯슦 湲곕낯 ?⑤뵫
        offset = 0
        
        if self.is_password:
            render_text = "*" * (len(self.text) + len(self.editing_text))
                                      
        while True:            
            if text_px_len <= box_px_len:
                return render_text
            
            else:
                if self.get_text_width(render_text[offset:]) <= box_px_len:
                    return render_text[offset:]
                else:
                    offset += 1
                    if offset >= len(self.text):
                        return ""


    def handle_event(self, ev: pygame.event.Event) -> Optional[str]:
        # 留덉슦???대┃?쇰줈 ?ъ빱??on/off
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            self.active = self.rect.collidepoint(ev.pos)
            if self.active:
                self.color = BLUE
            else:
                self.color = GRAY

        # ===== ?쒓?/?곷Ц 議고빀 以?臾몄옄??(?꾨━酉? =====
        elif ev.type == pygame.TEXTEDITING:
            # ?쒓? ?덉슜???꾨뱶留?議고빀 ?곹깭 ?쒖떆
            if self.active and self.allow_korean:
                self.editing_text = ev.text

        # ===== 理쒖쥌 ?뺤젙??臾몄옄 ?낅젰 =====
        elif ev.type == pygame.TEXTINPUT:
            self.add_char(ev.text)
 
        # ===== ?쒖뼱??泥섎━ (諛깆뒪?섏씠??+ ?뷀꽣) =====
        elif ev.type == pygame.KEYDOWN:
            if ev.key == pygame.K_BACKSPACE:  # TEXTINPUT?쇰줈??臾몄옄 ?꾨땲硫?泥섎━ ?덈맖, ?곕줈 泥섎━ ?꾩슂
                self.delete_char()
                self.backspace_pressed = True

            elif ev.key == pygame.K_RETURN:
                input_text = self.extract_text()
                return input_text
                
        elif ev.type == pygame.KEYUP:
            if ev.key == pygame.K_BACKSPACE:
                self._reset_cursor()

        return None

    def update(self, dt_ms: int):
        # 而ㅼ꽌 ?먮㈇留??좎?
        if self.active:
            self.cursor_timer_ms += dt_ms
            if self.cursor_timer_ms >= self.cursor_blink_ms:
                self.cursor_timer_ms -= self.cursor_blink_ms
                self.cursor_visible = not self.cursor_visible

            if self.backspace_repeat_time1_active and self.backspace_pressed:
                self.backspace_repeat_timer += dt_ms
                if self.backspace_repeat_timer >= self.backspace_repeat_time1:
                    self.backspace_repeat_timer -= self.backspace_repeat_time1
                    self.backspace_repeat_time1_active = False
                    self.backspace_repeat_time2_active = True
                    self.delete_char()

            elif self.backspace_repeat_time2_active and self.backspace_pressed:
                self.backspace_repeat_timer += dt_ms
                if self.backspace_repeat_timer >= self.backspace_repeat_time2:
                    self.backspace_repeat_timer -= self.backspace_repeat_time2
                    self.delete_char()
                
        else:
            self.cursor_visible = False
            self.cursor_timer_ms = 0
            self.backspace_repeat_timer = 0
            self.backspace_repeat_time1_active = True
            self.backspace_repeat_time2_active = False


    def draw(self):
        if self.holder_image is not None:
            holder_image = self.rm.images.scale_image(self.holder_image, self.rect.w, self.rect.h)
            self.screen.blit(holder_image, self.rect)

        show_text = self.get_render_text()
        if not show_text and not self.active:
            txt = self.font.render(self.placeholder, True, PLACEHOLDER)
        else:
            txt = self.font.render(show_text, True, WHITE)

        text_x = self.rect.x + self.padding
        text_y = self.rect.y + (self.rect.height - txt.get_height()) // 2
        text_pos = (text_x, text_y)
        self.screen.blit(txt, text_pos)

        # 而ㅼ꽌
        if self.active and self.cursor_visible:
            cursor_x = text_x + txt.get_width()
            cursor_y = text_y
            cursor_h = self.text_h
            pygame.draw.rect(self.screen, WHITE,(cursor_x, cursor_y, 2, cursor_h))
