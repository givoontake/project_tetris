# image_loader.py
import pygame
from typing import Optional
from define import *

# class AssetResource: # 데이터 클래스의 타입 지정은 힌트일 뿐 동작과는 연관 없음, 역할은 c++의 구조체와 비슷
#     type: Optional[int] # int이거나 None
#     image: Optional[pygame.Surface] # pygame.Surface 이거나 None

class AssetManager:
    def __init__(self):
        #self.asset = []
        pass

    def load_image(filename: str, width: int, height: int) -> pygame.Surface:
        """
        지정된 파일 경로의 이미지를 로드하고,
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

DEFAULT_RED = 1
DEFAULT_ORANGE = 2
DEFAULT_YELLOW = 3
DEFAULT_GREEN = 4
DEFAULT_BLUE = 5
DEFAULT_INDIGO = 6
DEFAULT_PURPLE = 7

CANDY_RED = 8
CANDY_ORANGE = 9
CANDY_YELLOW = 10  
CANDY_GREEN = 11
CANDY_BLUE = 12
CANDY_INDIGO = 13
CANDY_PURPLE = 14

# 리스트에 알기쉽게 접근하기 위해 정의

am = AssetManager()

ASSET = {
    # --- Default Blocks ---
    DEFAULT_RED:     am.load_image("blocks/default/default_red.png"),
    DEFAULT_ORANGE:  am.load_image("blocks/default/default_orange.png"),
    DEFAULT_YELLOW:  am.load_image("blocks/default/default_yellow.png"),
    DEFAULT_GREEN:   am.load_image("blocks/default/default_green.png"),
    DEFAULT_BLUE:    am.load_image("blocks/default/default_blue.png"),
    DEFAULT_INDIGO:  am.load_image("blocks/default/default_indigo.png"),
    DEFAULT_PURPLE:  am.load_image("blocks/default/default_purple.png"),

    # --- Candy Blocks ---
    CANDY_RED:       am.load_image("blocks/candy/candy_red.png"),
    CANDY_ORANGE:    am.load_image("blocks/candy/candy_orange.png"),
    CANDY_YELLOW:    am.load_image("blocks/candy/candy_yellow.png"),
    CANDY_GREEN:     am.load_image("blocks/candy/candy_green.png"),
    CANDY_BLUE:      am.load_image("blocks/candy/candy_blue.png"),
    CANDY_INDIGO:    am.load_image("blocks/candy/candy_indigo.png"),
    CANDY_PURPLE:    am.load_image("blocks/candy/candy_purple.png"),
}