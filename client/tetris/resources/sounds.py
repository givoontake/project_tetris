import pygame
from pathlib import Path
from tetris.resources.paths import *

class Sounds:
    FILE_PATH = "tetris/config/sound_settings.txt"
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

        self.volumes = {"마스터": 100, "배경음": 80, "효과음": 80}
        self.load_sound_settings()
        self.set_volumes(self.volumes)

    def set_volumes(self, new_volumes: dict[str, int]) -> bool: # self.volume 읽고 값만 바꾸면 된다.
        if len(new_volumes) != len(self.volumes):
            print("오류가 발생하여 소리 설정에 실패했습니다. -> 인자 수 불일치")
            return False
        for _ , new_volume in new_volumes.items():
            if new_volume > 100 or new_volume < 0:
                print("오류가 발생하여 소리 설정에 실패했습니다. -> 잘못된 설정 값")
                return False
        
        self.volumes = new_volumes
        master_volume = self.volumes["마스터"] / 100.0
        bgm_volume = (self.volumes["배경음"] / 100.0)*master_volume
        effect_volume = (self.volumes["효과음"] / 100.0)*master_volume

        pygame.mixer.music.set_volume(bgm_volume)
            
        for _ , effect in self.sound_effects.items():
            effect.set_volume(effect_volume)

        self.save_sound_settings()

        return True

    def load_sound_settings(self):
        path = Path(self.FILE_PATH)
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

                if key not in ("마스터", "배경음", "효과음"):
                    continue

                try:
                    volume = int(val)
                except ValueError:
                    continue

                # 0~100 범위 보정
                volume = max(0, min(100, volume))

                self.volumes[key] = volume

        except OSError:
            print("설정 파일을 읽는 중 오류가 발생했습니다. 기본값을 사용합니다.")

    def save_sound_settings(self):
        path = Path(self.FILE_PATH)
        master = max(0, min(100, int(self.volumes["마스터"])))
        bgm = max(0, min(100, int(self.volumes["배경음"])))
        effect = max(0, min(100, self.volumes["효과음"]))

        data = f"마스터={master}\n" f"배경음={bgm}\n" f"효과음={effect}\n"
        path.write_text(data, encoding="utf-8")

    def load_effect_sound(self, path: str)-> pygame.mixer.Sound:
        file_path = RESOURCE_ROOT_PATH + path
        return pygame.mixer.Sound(file_path)