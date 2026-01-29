# image_loader.py
import pygame
from typing import Optional
from define import *

# class AssetResource: # 데이터 클래스의 타입 지정은 힌트일 뿐 동작과는 연관 없음, 역할은 c++의 구조체와 비슷
#     type: Optional[int] # int이거나 None
#     image: Optional[pygame.Surface] # pygame.Surface 이거나 None

class ResourceManager:
    HOVER_SCALE = 1.05
    PRESS_SCALE = 0.95
    def __init__(self):
        # fonts
        self.block_images = {}
        self.button_images = {}
        self.shutter_image = self.load_image("resource/backgrounds/shutter.png", BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT)
        # sounds
        self.bgm = pygame.mixer.music.load("resource/sounds/bgm_ingame.mp3")
        self.button_sound_hover = pygame.mixer.Sound("resource/sounds/button_hover.mp3")
        self.button_sound_press = pygame.mixer.Sound("resource/sounds/button_press.mp3")
        self.move_sound = pygame.mixer.Sound("resource/sounds/sound_effect_move.mp3")
        self.fix_sound = pygame.mixer.Sound("resource/sounds/sound_effect_fix.mp3")
        self.clearline_sound = pygame.mixer.Sound("resource/sounds/sound_effect_clearline.mp3")
        self.addline_sound = pygame.mixer.Sound("resource/sounds/sound_effect_addline.mp3")
        self.shutter_sound = pygame.mixer.Sound("resource/sounds/shutter_open.mp3")

        self.login_button = self.load_image("resource/button/login_button.png", 200, 100)
        self.login_label_frame = self.load_image("resource/login_label_frame.png", 400, 100)
        self.logo = self.load_image("resource/button/new_tetris_logo.png", 250, 100)

    def init(self):
        self.block_images = {
        # --- Default Blocks ---
        DEFAULT_RED:     self.load_image("resource/blocks/default/default_red.png", CELL_SIZE, CELL_SIZE),
        DEFAULT_ORANGE:  self.load_image("resource/blocks/default/default_orange.png", CELL_SIZE, CELL_SIZE),
        DEFAULT_YELLOW:  self.load_image("resource/blocks/default/default_yellow.png", CELL_SIZE, CELL_SIZE),
        DEFAULT_GREEN:   self.load_image("resource/blocks/default/default_green.png", CELL_SIZE, CELL_SIZE),
        DEFAULT_BLUE:    self.load_image("resource/blocks/default/default_blue.png", CELL_SIZE, CELL_SIZE),
        DEFAULT_INDIGO:  self.load_image("resource/blocks/default/default_indigo.png", CELL_SIZE, CELL_SIZE),
        DEFAULT_PURPLE:  self.load_image("resource/blocks/default/default_purple.png", CELL_SIZE, CELL_SIZE),
        DEFAULT_GRAY:  self.load_image("resource/blocks/default/default_gray.png", CELL_SIZE, CELL_SIZE),

        # --- Candy Blocks ---
        CANDY_RED:       self.load_image("resource/blocks/candy/candy_red.png", CELL_SIZE, CELL_SIZE),
        CANDY_ORANGE:    self.load_image("resource/blocks/candy/candy_orange.png", CELL_SIZE, CELL_SIZE),
        CANDY_YELLOW:    self.load_image("resource/blocks/candy/candy_yellow.png", CELL_SIZE, CELL_SIZE),
        CANDY_GREEN:     self.load_image("resource/blocks/candy/candy_green.png", CELL_SIZE, CELL_SIZE),
        CANDY_BLUE:      self.load_image("resource/blocks/candy/candy_blue.png", CELL_SIZE, CELL_SIZE),
        CANDY_INDIGO:    self.load_image("resource/blocks/candy/candy_indigo.png", CELL_SIZE, CELL_SIZE),
        CANDY_PURPLE:    self.load_image("resource/blocks/candy/candy_purple.png", CELL_SIZE, CELL_SIZE),
        }

        self.button_images = {
        BUTTON_LOGIN_IDLE: self.load_image("resource/button/button_login_idle.png", 300, 100),
        BUTTON_LOGIN_HOVER: self.load_image("resource/button/button_login_hover.png", 300, 100),
        BUTTON_LOGIN_PRESS: self.load_image("resource/button/button_login_press.png", 300, 100),
        }

        self.button_sound_hover.set_volume(0.8)
        self.button_sound_press.set_volume(0.8)
        self.move_sound.set_volume(0.8)
        self.fix_sound.set_volume(0.8)
        self.clearline_sound.set_volume(0.8)
        self.addline_sound.set_volume(0.8)
    
    def load_image(self, filename: str, width: int, height: int) -> pygame.Surface:
        """
        지정된 파일 경로의 이미지를 로드하고
        절대 크기 (width, height)로 스케일링하여 반환한다.

        Args:
            filename (str): 이미지 파일 경로
            width (int): 변환할 가로 픽셀 크기
            height (int): 변환할 세로 픽셀 크기

        Returns:
            pygame.Surface: 스케일링된 이미지 Surface
        """
        # 이미지 로드 (투명도 유지)
        image = pygame.image.load(filename).convert_alpha()

        # 절대 크기로 스케일링
        scaled_image = pygame.transform.smoothscale(image, (int(width), int(height)))
        return scaled_image
    
    def scale_image(self, image: pygame.Surface, width: int, height: int) -> pygame.Surface:
        return pygame.transform.smoothscale(image, (int(width), int(height)))


DEFAULT_RED = 1
DEFAULT_ORANGE = 2
DEFAULT_YELLOW = 3
DEFAULT_GREEN = 4
DEFAULT_BLUE = 5
DEFAULT_INDIGO = 6
DEFAULT_PURPLE = 7
DEFAULT_GRAY = 15

CANDY_RED = 8
CANDY_ORANGE = 9
CANDY_YELLOW = 10  
CANDY_GREEN = 11
CANDY_BLUE = 12
CANDY_INDIGO = 13
CANDY_PURPLE = 14

BUTTON_LOGIN_IDLE = 1001
BUTTON_LOGIN_HOVER = 1002
BUTTON_LOGIN_PRESS = 1003

BUTTON_LOGO_IDLE = 1004
BUTTON_LOGO_HOVER = 1005
BUTTON_LOGO_PRESS = 1006