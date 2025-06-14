# game_loop.py
import pygame
from define import BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT, INITIAL_SCALE, FPS
from game_state import GameState
from network import NetworkClient


def run():
    pygame.init()
    w = int(BASE_SCREEN_WIDTH * INITIAL_SCALE)
    h = int(BASE_SCREEN_HEIGHT * INITIAL_SCALE)
    screen = pygame.display.set_mode((w, h), pygame.RESIZABLE)
    gs = GameState(screen)
    clock = pygame.time.Clock()
    net = None

    running = True
    while running:
        state = gs.state

        if state == 'select_mode':
            next_state = gs.select_mode()
        elif state == 'select_play':
            next_state = gs.select_play()
        elif state == 'check_connect':
            if not gs.dev_mode:
                net = NetworkClient()
                gs.network_ok = net.connect()
            else:
                gs.network_ok = True
            if gs.network_ok:
                if gs.players == 1:
                    next_state = 'single_play'
                elif gs.players == 2:
                    next_state = 'multi_play2'
                else:
                    next_state = 'multi_play5'
            else:
                next_state = 'connect_error'
        elif state == 'connect_error':
            next_state = gs.connect_error()
        elif state == 'single_play':
            next_state = gs.single_play()
        elif state == 'multi_play2':
            next_state = gs.multi_play2()
        elif state == 'multi_play5':
            next_state = gs.multi_play5()
        else:
            next_state = 'select_mode'

        if next_state == 'quit':
            running = False
        gs.state = next_state
        clock.tick(FPS)

    pygame.quit()


if __name__ == '__main__':
    run()
