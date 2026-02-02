from enum import IntEnum

# 레이아웃 값은 전체 화면에 대한 *비율
INFO_HEADER_WIDTH = 0.2
INFO_HEADER_HEIGHT = 0.05

PADDING_WIDTH_RATE = 0.1
BOARD_WIDTH_RATE = 0.4
BOARD_HEIGHT_RATE = 0.9
READY_BUTTON_HEIGHT_RATE = 0.1
# READY_WIDTH는 그리드 rect.w을 따른다.

class RoomState(IntEnum): # 멀티에만 존재
    WAIT = 0
    PLAY = 1

