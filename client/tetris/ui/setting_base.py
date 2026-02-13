import pygame

class SettingBase:
    def __init__(self):
        pass

    def modify_settings(self):
        pass

    def apply_settings(self) -> bool:
        pass

    def handle_event(self, ev: pygame.event.Event):
        pass

    def draw(self):
        pass

