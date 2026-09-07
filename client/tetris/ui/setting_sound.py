import pygame

from tetris.ui.setting_base import SettingBase
from tetris.ui.adjust_sound import AdjustSound
from tetris.resources.resource_manager import ResourceManager

class SettingSound(SettingBase):
    def __init__(self, screen: pygame.Surface, rect: pygame.Rect, rm: ResourceManager):
        super().__init__()
        self.screen = screen
        self.rect = rect
        self.rm = rm
        self.sound_settings: list[AdjustSound] = []

        self.set_layout()
    
    def set_layout(self):
        setting_x = self.rect.x
        setting_w = self.rect.w

        self.adjusted_sound_volumes = self.rm.sounds.volumes.copy()
        sound_types = []
        for type, _ in self.adjusted_sound_volumes.items():
            sound_types.append(type)

        sound_num = len(sound_types)
        if sound_num == 0:
            return

        setting_h = self.rect.h // (sound_num * 2 - 1)
        total_h = setting_h * sound_num + setting_h * (sound_num - 1)
        setting_y = self.rect.y + (self.rect.h - total_h) // 2

        for sound_type in sound_types:
            setting_rect = pygame.Rect(setting_x, setting_y, setting_w, setting_h)
            setting = AdjustSound(self.screen, setting_rect, self.rm, sound_type)
            self.sound_settings.append(setting)
            setting_y += setting_h * 2
        return

        setting_x = self.rect.x
        setting_y = self.rect.y
        setting_w = self.rect.w
        setting_h = int(self.rect.h*0.2)

        self.adjusted_sound_volumes = self.rm.sounds.volumes.copy()
        sound_types = []# 존재하는 사운드 이름 얻어오기
        for type, _ in self.adjusted_sound_volumes.items():
            sound_types.append(type)

        for sound_type in sound_types:
            setting_rect = pygame.Rect(setting_x, setting_y, setting_w, setting_h)
            setting = AdjustSound(self.screen, setting_rect, self.rm, sound_type)
            self.sound_settings.append(setting)
            setting_y += setting_h


    def modify_settings(self):
        for setting in self.sound_settings:
            self.adjusted_sound_volumes[setting.volume_type] = setting.adjusted_volume

    def apply_settings(self) -> bool:
        return self.rm.sounds.set_volumes(self.adjusted_sound_volumes)

    def handle_event(self, ev: pygame.event.Event):
        for setting in self.sound_settings:
            setting.handle_event(ev)

        self.modify_settings()

    def draw(self):
        for sound_setting in self.sound_settings:
            sound_setting.draw()
