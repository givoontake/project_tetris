import pygame
from tetris.config.define import *
from tetris.resources.paths import *

class Images:
    HOVER_SCALE = 1.05
    PRESS_SCALE = 0.95
    def __init__(self):
        # ---- Block Images ----
        self.block_images = {
            key: self.load_image(path, CELL_SIZE, CELL_SIZE)
            for key, path in BLOCK_IMAGE_PATHS.items()
        }

        # ---- UI Images ----
        # 즉 key: value for 변수 in iterable 구조이고 조건을 통과한 것들만 딕셔너리에 저장되며, 조건문에 사용된 변수는 위에서도 사용 가능하다.
        # 어차피 절차적으로 보면 아래 반복, 조건문이 먼저 실행되기 때문
        self.ui_images = { # 딕셔너리 컴프리헨션 -> 오른쪽부터 실행된다. 
            key: self.load_image(path, 300, 100)
            for key, path in UI_IMAGE_PATHS.items()
            if key in (UI_BUTTON_LOGIN_IDLE, UI_BUTTON_LOGIN_HOVER, UI_BUTTON_LOGIN_PRESS)
        }

        # ---- 기존 딕셔너리에 추가 ----
        self.ui_images.update({
            UI_SHUTTER: self.load_image(UI_IMAGE_PATHS[UI_SHUTTER], BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT),
            UI_LOGIN_BUTTON: self.load_image(UI_IMAGE_PATHS[UI_LOGIN_BUTTON], 200, 100),
            UI_LOGIN_LABEL_FRAME: self.load_image(UI_IMAGE_PATHS[UI_LOGIN_LABEL_FRAME], 400, 100),
            UI_LOGO: self.load_image(UI_IMAGE_PATHS[UI_LOGO], 250, 100),
        })


    def load_image(self, file_path: str, width: int, height: int) -> pygame.Surface:
        # 이미지 로드 (투명도 유지)
        path = RESOURCE_ROOT_PATH + file_path
        image = pygame.image.load(path).convert_alpha()

        # 절대 크기로 스케일링
        scaled_image = pygame.transform.smoothscale(image, (int(width), int(height)))
        return scaled_image
    
    def scale_image(self, image: pygame.Surface, width: int, height: int) -> pygame.Surface:
        return pygame.transform.smoothscale(image, (int(width), int(height)))