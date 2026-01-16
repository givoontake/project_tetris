import pygame
from resource_manager import *
from define_anim_images import *

class LoadingAnimation:
    def __init__(self, screen: pygame.Surface, rm: ResourceManager):
        self.images: list[Base] = [Red(screen, rm), Orange(screen, rm), Yellow(screen, rm),
                                    Green(screen, rm), Blue(screen, rm), Indigo(screen, rm), Purple(screen, rm)]
        self.large_image_offset = -1
        self.screen_width, self.screen_height = screen.get_size()
        
    def animation(self):
        image_width = IMAGE_TYPE.LARGE
        image_height = IMAGE_TYPE.LARGE
        center_x = self.screen_width / 2 - image_width / 2
        center_y = self.screen_height / 2 - image_height / 2
        padding_x = 30
        total_width = len(self.images)*image_width + padding_x*(len(self.images) - 1)
        start_draw_x = center_x - total_width / 2
        draw_y = center_y

        draw_x = start_draw_x
        for image in self.images:
            image.draw(draw_x, draw_y)
            draw_x += image_width + padding_x

    def set_next_images(self):
        self.large_image_offset = (index + 1) % len(self.images)
        index = self.large_image_offset
        for image in self.images:
            image.reset_image_type()

        if index == 0:
            self.images[index].image_type = IMAGE_TYPE.LARGE
            self.images[index + 1].image_type = IMAGE_TYPE.MEDIUM

        elif index == 6:
            self.images[index].image_type = IMAGE_TYPE.LARGE
            self.images[index - 1].image_type = IMAGE_TYPE.MEDIUM

        else:
            self.images[index].image_type = IMAGE_TYPE.LARGE
            self.images[index + 1].image_type = IMAGE_TYPE.MEDIUM
            self.images[index - 1].image_type = IMAGE_TYPE.MEDIUM