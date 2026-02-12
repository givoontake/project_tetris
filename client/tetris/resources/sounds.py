import pygame
from pathlib import Path
from tetris.resources.paths import *

class Sounds:
    def __init__(self):
        # BGM 로드
        self.bgms = {}
        for name, path in BGM_PATHS.items():
            pygame.mixer.music.load(path)
            self.bgms[name] = path   # music은 단일 채널이므로 path만 저장

        # 효과음 로드
        self.sound_effects: dict[int, pygame.mixer.Sound] = {}
        for name, path in EFFECT_SOUND_PATHS.items():
            self.sound_effects[name] = self.load_effect_sound(path)

        self.bgm_volume = 0.8
        self.effect_volume = 0.8

    def load_sound_settings(self):
        path = Path("sound_settings.txt")
        if not path.exists():
            print("설정 파일이 존재하지 않아 기본값을 사용합니다.")
            return

        try:
            for line in path.read_text(encoding="utf-8").splitlines():
                s = line.strip()

                # 빈 줄 또는 주석 무시
                if not s or s.startswith("#"):
                    continue

                # '=' 정확히 1개만 허용
                if s.count("=") != 1:
                    continue

                key, val = s.split("=")
                key = key.strip().lower()
                val = val.strip()

                if key not in ("bgm", "effects"):
                    continue

                try:
                    num = int(val)
                except ValueError:
                    continue

                # 0~100 범위 보정
                num = max(0, min(100, num))

                if key == "bgm":
                    self.bgm_volume = num
                elif key == "effects":
                    self.effects = num

        except OSError:
            print("설정 파일을 읽는 중 오류가 발생했습니다. 기본값을 사용합니다.")

    def save_sound_settings(bgm: int, effects: int):
        p = Path("sound_settings.txt")
        bgm = max(0, min(100, int(bgm)))
        effects = max(0, min(100, int(effects)))

        data = f"bgm={bgm}\n" f"effects={effects}\n"
        p.write_text(data, encoding="utf-8")

    def load_effect_sound(self, path: str)-> pygame.mixer.Sound:
        file_path = RESOURCE_ROOT_PATH + path
        return pygame.mixer.Sound(file_path)