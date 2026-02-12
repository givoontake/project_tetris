from tetris.resources.images import *
from tetris.resources.sounds import *
from tetris.resources.fonts import *

class ResourceManager:
    def __init__(self):
        self.images = Images()
        self.sounds = Sounds()
        self.fonts = Fonts()
