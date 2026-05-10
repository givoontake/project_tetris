import pygame
from pathlib import Path
from typing import Optional

from tetris.config.define import CELL_SIZE, SHAPES
from tetris.resources.define import BLOCK_SHAPE_KEYS
from tetris.resources.define_colors import BLACK, DARK_GRAY, GOLD, ORANGE, WHITE
from tetris.resources.paths import BLOCK_IMAGE_PATHS
from tetris.resources.resource_manager import ResourceManager
from tetris.ui.scroll_window_base import ScrollWindowBase


class BlockSkinList(ScrollWindowBase):
    CARD_SIZE = 200
    CARD_BORDER_WIDTH = 1
    SELECTED_BORDER_WIDTH = 4

    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.block_keys = list(BLOCK_IMAGE_PATHS.keys())
        self.selected_block_key: Optional[int] = None
        self.card_cols = self._get_card_cols(rect.w)
        self.preview_images = {
            block_key: self.rm.images.block_images[block_key]
            for block_key in self.block_keys
        }
        super().__init__(screen, rect, self.CARD_SIZE)
        self.set_scroll_info()
        self.set_scroll_len()

    def _get_card_cols(self, width: int) -> int:
        cols = width // self.CARD_SIZE
        if cols < 1:
            cols = 1
        return cols

    def get_items_len(self) -> int:
        return (len(self.block_keys) + self.card_cols - 1) // self.card_cols

    def set_selected_block_key(self, block_key: Optional[int]):
        self.selected_block_key = block_key

    def _get_card_rect(self, row_index: int, col_index: int, draw_y: int) -> pygame.Rect:
        x = self.window_x + col_index * self.CARD_SIZE
        return pygame.Rect(x, draw_y, self.CARD_SIZE, self.CARD_SIZE)

    def _get_block_name(self, block_key: int) -> str:
        path = BLOCK_IMAGE_PATHS[block_key]
        return Path(path).stem.upper()

    def _draw_block_name(self, block_key: int, card_rect: pygame.Rect):
        name = self._get_block_name(block_key)
        font = self.rm.fonts.get_font(16)
        surface = font.render(name, True, WHITE)
        if surface.get_width() > card_rect.w - 12:
            font = self.rm.fonts.get_font(12)
            surface = font.render(name, True, WHITE)

        text_rect = surface.get_rect()
        text_rect.centerx = card_rect.centerx
        text_rect.bottom = card_rect.bottom - 14
        self.screen.blit(surface, text_rect)

    def draw_item(self, i: int, draw_text_y: int):
        for col in range(self.card_cols):
            item_index = i * self.card_cols + col
            if item_index >= len(self.block_keys):
                return

            block_key = self.block_keys[item_index]
            card_rect = self._get_card_rect(i, col, draw_text_y)
            card_surface = pygame.Surface((card_rect.w, card_rect.h), pygame.SRCALPHA)
            card_surface.fill((0, 0, 0, 120))
            self.screen.blit(card_surface, card_rect.topleft)

            image = self.preview_images[block_key]
            image_rect = image.get_rect()
            image_rect.center = card_rect.center
            image_rect.y -= 18
            self.screen.blit(image, image_rect)
            self._draw_block_name(block_key, card_rect)

            border_color = ORANGE if block_key == self.selected_block_key else WHITE
            border_width = self.SELECTED_BORDER_WIDTH if block_key == self.selected_block_key else self.CARD_BORDER_WIDTH
            pygame.draw.rect(self.screen, border_color, card_rect, border_width)

    def _get_clicked_block_key(self, pos: tuple[int, int]) -> Optional[int]:
        if not self.window_rect.collidepoint(pos):
            return None

        rel_x = pos[0] - self.window_x
        rel_y = pos[1] - self.window_y
        col = rel_x // self.CARD_SIZE
        row = self.show_start + (rel_y // self.CARD_SIZE)
        item_index = row * self.card_cols + col
        if item_index < 0 or item_index >= len(self.block_keys):
            return None
        return self.block_keys[item_index]

    def handle_event(self, ev: pygame.event.Event) -> Optional[int]:
        if ev.type == pygame.MOUSEWHEEL:
            self.can_scroll = self.window_rect.collidepoint(pygame.mouse.get_pos())

        super().handle_event(ev)

        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
            return self._get_clicked_block_key(ev.pos)
        return None

    def draw(self):
        bg_surface = pygame.Surface((self.window_rect.w, self.window_rect.h), pygame.SRCALPHA)
        bg_surface.fill((0, 0, 0, 96))
        self.screen.blit(bg_surface, self.window_rect.topleft)
        super().draw()
        pygame.draw.rect(self.screen, WHITE, self.window_rect, 2)


class BlockShapePreviewPanel:
    BORDER_WIDTH = 2
    SELECTED_BORDER_WIDTH = 4
    ROWS = 4
    COLS = 2

    def __init__(
        self,
        screen: pygame.Surface,
        rect: pygame.Rect,
        rm: ResourceManager,
        block_texture_keys: dict[str, int],
    ):
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.block_texture_keys = block_texture_keys
        self.selected_shape_key = BLOCK_SHAPE_KEYS[0]
        self.slot_rects: dict[str, pygame.Rect] = {}
        self.scaled_textures: dict[int, pygame.Surface] = {}
        self.set_layout()

    def set_layout(self):
        slot_w = self.rect.w // self.COLS
        slot_h = self.rect.h // self.ROWS
        self.slot_rects.clear()

        for index, shape_key in enumerate(BLOCK_SHAPE_KEYS):
            col = index // self.ROWS
            row = index % self.ROWS
            slot_rect = pygame.Rect(
                self.rect.x + col * slot_w,
                self.rect.y + row * slot_h,
                slot_w,
                slot_h,
            )
            self.slot_rects[shape_key] = slot_rect

    def set_block_texture(self, shape_key: str, block_image_key: int):
        if shape_key in self.block_texture_keys:
            self.block_texture_keys[shape_key] = block_image_key

    def get_selected_block_key(self) -> int:
        return self.block_texture_keys[self.selected_shape_key]

    def _get_shape_cells(self, shape_key: str) -> list[tuple[int, int]]:
        if shape_key == "G":
            return [(0, 0)]
        return SHAPES[shape_key][0]

    def _get_texture(self, block_image_key: int) -> pygame.Surface:
        if block_image_key not in self.scaled_textures:
            self.scaled_textures[block_image_key] = pygame.transform.smoothscale(
                self.rm.images.block_images[block_image_key],
                (CELL_SIZE, CELL_SIZE),
            )
        return self.scaled_textures[block_image_key]

    def _draw_shape(self, shape_key: str, slot_rect: pygame.Rect):
        block_image_key = self.block_texture_keys[shape_key]
        texture = self._get_texture(block_image_key)
        cells = self._get_shape_cells(shape_key)

        xs = [cx for cx, _ in cells]
        ys = [cy for _, cy in cells]
        min_x, max_x = min(xs), max(xs)
        min_y, max_y = min(ys), max(ys)
        shape_w = (max_x - min_x + 1) * CELL_SIZE
        shape_h = (max_y - min_y + 1) * CELL_SIZE
        offx = slot_rect.x + (slot_rect.w - shape_w) / 2 - min_x * CELL_SIZE
        offy = slot_rect.y + (slot_rect.h - shape_h) / 2 - min_y * CELL_SIZE

        for cx, cy in cells:
            px = int(offx + cx * CELL_SIZE)
            py = int(offy + cy * CELL_SIZE)
            self.screen.blit(texture, (px, py))

    def _draw_shape_name(self, shape_key: str, slot_rect: pygame.Rect):
        font = self.rm.fonts.get_font(20)
        surface = font.render(shape_key, True, WHITE)
        text_rect = surface.get_rect()
        text_rect.x = slot_rect.x + 10
        text_rect.y = slot_rect.y + 8
        self.screen.blit(surface, text_rect)

    def handle_event(self, ev: pygame.event.Event) -> Optional[str]:
        if ev.type != pygame.MOUSEBUTTONDOWN or ev.button != 1:
            return None

        for shape_key, slot_rect in self.slot_rects.items():
            if slot_rect.collidepoint(ev.pos):
                self.selected_shape_key = shape_key
                return shape_key
        return None

    def draw(self):
        bg_surface = pygame.Surface((self.rect.w, self.rect.h), pygame.SRCALPHA)
        bg_surface.fill((0, 0, 0, 120))
        self.screen.blit(bg_surface, self.rect.topleft)

        for shape_key in BLOCK_SHAPE_KEYS:
            slot_rect = self.slot_rects[shape_key]
            is_selected = shape_key == self.selected_shape_key
            border_color = ORANGE if is_selected else WHITE
            border_width = self.SELECTED_BORDER_WIDTH if is_selected else self.BORDER_WIDTH
            pygame.draw.rect(self.screen, (0, 0, 0, 80), slot_rect)
            self._draw_shape(shape_key, slot_rect)
            self._draw_shape_name(shape_key, slot_rect)
            pygame.draw.rect(self.screen, border_color, slot_rect, border_width)

        pygame.draw.rect(self.screen, GOLD, self.rect, 2)
