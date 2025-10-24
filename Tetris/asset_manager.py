# image_loader.py
import pygame
from typing import Optional
from define import *

# class AssetResource: # 데이터 클래스의 타입 지정은 힌트일 뿐 동작과는 연관 없음, 역할은 c++의 구조체와 비슷
#     type: Optional[int] # int이거나 None
#     image: Optional[pygame.Surface] # pygame.Surface 이거나 None

class AssetManager:
    def __init__(self):
        self.asset = []

    def load_all(self):
        for i in range(len(ASSET)):
            type = ASSET[i][TYPE]
            surface = self.load_image(ASSET[i][PATH], CELL_SIZE, CELL_SIZE)
            self.asset.append([type, surface])

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

