from tetris.resources.define import *

# ===== Resource Root =====
RESOURCE_ROOT_PATH = "tetris/resources/"

# ===== Block Image Paths =====
BLOCK_IMAGE_PATHS = {
    # Default
    DEFAULT_RED:     "blocks/default/default_red.png",
    DEFAULT_ORANGE:  "blocks/default/default_orange.png",
    DEFAULT_YELLOW:  "blocks/default/default_yellow.png",
    DEFAULT_GREEN:   "blocks/default/default_green.png",
    DEFAULT_BLUE:    "blocks/default/default_blue.png",
    DEFAULT_INDIGO:  "blocks/default/default_indigo.png",
    DEFAULT_PURPLE:  "blocks/default/default_purple.png",
    DEFAULT_GRAY:    "blocks/default/default_gray.png",

    # Candy
    CANDY_RED:       "blocks/candy/candy_red.png",
    CANDY_ORANGE:    "blocks/candy/candy_orange.png",
    CANDY_YELLOW:    "blocks/candy/candy_yellow.png",
    CANDY_GREEN:     "blocks/candy/candy_green.png",
    CANDY_BLUE:      "blocks/candy/candy_blue.png",
    CANDY_INDIGO:    "blocks/candy/candy_indigo.png",
    CANDY_PURPLE:    "blocks/candy/candy_purple.png",
}

UI_IMAGE_PATHS = {
    UI_BUTTON_LOGIN_IDLE:  "button/button_login_idle.png",
    UI_BUTTON_LOGIN_HOVER: "button/button_login_hover.png",
    UI_BUTTON_LOGIN_PRESS: "button/button_login_press.png",

    UI_SHUTTER: "backgrounds/shutter.png",
    UI_LOGIN_BUTTON: "button/login_button.png",
    UI_LOGIN_LABEL_FRAME: "labels/login_label_frame.png",
    UI_LOGO: "button/new_tetris_logo.png",
    UI_HOST: "ui/crown.png",
    UI_MAIN_BACKGROUND: "backgrounds/login_background.png",
    UI_BUTTON_BLUE: "button/button_blue.png",
    UI_BUTTON_GREEN: "button/button_green.png",
    UI_BUTTON_ORANGE: "button/button_orange.png",
    UI_BUTTON_RED: "button/button_red.png",
    UI_BUTTON_SKY: "button/button_sky.png",
    UI_TEXT_HOLDER: "button/text_holder.png",
    UI_BUTTON2_BLUE: "button/button2_blue.png",
    UI_BUTTON2_ORANGE: "button/button2_orange.png",
    UI_BUTTON2_SKY: "button/button2_sky.png",
    UI_CHAT_INPUT_HOLDER: "button/chat_input_holder.png",
    UI_CHAT_WINDOW_BACKGROUND: "backgrounds/chat_window_background.png",
    UI_LOBBY_BACKGROUND: "backgrounds/default_background.png",
    UI_POPUP_BACKGROUND: "backgrounds/popup_background.png",
    UI_INGAME_BACKGROUND: "backgrounds/ingame_background.png",
    UI_FRAME11: "frames/gold_frame11.png",
    UI_FRAME12: "frames/gold_frame12.png",
    UI_FRAME21: "frames/gold_frame21.png",
    UI_FRAME51: "frames/gold_frame51.png",
}

# ===== Sound Paths =====
BGM_PATHS = {
    BGM_INGAME: "tetris/resources/sounds/bgm_ingame.mp3",
}

EFFECT_SOUND_PATHS = {
    EFFECT_BUTTON_HOVER: "sounds/button_hover.mp3",
    EFFECT_BUTTON_PRESS: "sounds/button_press.mp3",
    EFFECT_MOVE: "sounds/sound_effect_move.mp3",
    EFFECT_FIX: "sounds/sound_effect_fix.mp3",
    EFFECT_CLEARLINE: "sounds/sound_effect_clearline.mp3",
    EFFECT_ADDLINE: "sounds/sound_effect_addline.mp3",
    EFFECT_SHUTTER: "sounds/shutter_open.mp3",
}
