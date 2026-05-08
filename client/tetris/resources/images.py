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
            UI_HOST: self.load_image(UI_IMAGE_PATHS[UI_HOST], 100, 100),
            UI_MAIN_BACKGROUND: self.load_image(UI_IMAGE_PATHS[UI_MAIN_BACKGROUND], BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT),
            UI_BUTTON_BLUE: self.load_image(UI_IMAGE_PATHS[UI_BUTTON_BLUE], 300, 100),
            UI_BUTTON_GREEN: self.load_image(UI_IMAGE_PATHS[UI_BUTTON_GREEN], 300, 100),
            UI_BUTTON_ORANGE: self.load_image(UI_IMAGE_PATHS[UI_BUTTON_ORANGE], 300, 100),
            UI_BUTTON_RED: self.load_image(UI_IMAGE_PATHS[UI_BUTTON_RED], 300, 100),
            UI_BUTTON_SKY: self.load_image(UI_IMAGE_PATHS[UI_BUTTON_SKY], 300, 100),
            UI_TEXT_HOLDER: self.load_image(UI_IMAGE_PATHS[UI_TEXT_HOLDER], 400, 50),
            UI_BUTTON2_BLUE: self.load_image(UI_IMAGE_PATHS[UI_BUTTON2_BLUE], 150, 100),
            UI_BUTTON2_GREEN: self.load_image(UI_IMAGE_PATHS[UI_BUTTON2_GREEN], 150, 100),
            UI_BUTTON2_ORANGE: self.load_image(UI_IMAGE_PATHS[UI_BUTTON2_ORANGE], 150, 100),
            UI_BUTTON2_SKY: self.load_image(UI_IMAGE_PATHS[UI_BUTTON2_SKY], 150, 100),
            UI_CHAT_INPUT_HOLDER: self.load_image(UI_IMAGE_PATHS[UI_CHAT_INPUT_HOLDER], 400, 50),
            UI_CHAT_WINDOW_BACKGROUND: self.load_image(UI_IMAGE_PATHS[UI_CHAT_WINDOW_BACKGROUND], 1000, 200),
            UI_LOBBY_BACKGROUND: self.load_image(UI_IMAGE_PATHS[UI_LOBBY_BACKGROUND], BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT),
            UI_POPUP_BACKGROUND: self.load_image(UI_IMAGE_PATHS[UI_POPUP_BACKGROUND], 600, 400),
            UI_WINDOW_BACKGROUND: self.load_image(UI_IMAGE_PATHS[UI_WINDOW_BACKGROUND], 500, 500),
            UI_INGAME_BACKGROUND: self.load_image(UI_IMAGE_PATHS[UI_INGAME_BACKGROUND], BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT),
            UI_FRAME11: self.load_image(UI_IMAGE_PATHS[UI_FRAME11], 500, 500),
            UI_FRAME12: self.load_image(UI_IMAGE_PATHS[UI_FRAME12], 500, 800),
            UI_FRAME21: self.load_image(UI_IMAGE_PATHS[UI_FRAME21], 300, 150),
            UI_FRAME51: self.load_image(UI_IMAGE_PATHS[UI_FRAME51], 300, 150),
            UI_REFRESH_ICON: self.load_image(UI_IMAGE_PATHS[UI_REFRESH_ICON], 50, 50),
        })


    def load_image(self, file_path: str, width: int, height: int) -> pygame.Surface:
        # 이미지 로드 (투명도 유지)
        path = resource_path(file_path)
        image = pygame.image.load(path).convert_alpha()

        # 절대 크기로 스케일링
        scaled_image = pygame.transform.smoothscale(image, (int(width), int(height)))
        return scaled_image
    
    def scale_image(self, image: pygame.Surface, width: int, height: int) -> pygame.Surface:
        return pygame.transform.smoothscale(image, (int(width), int(height)))
