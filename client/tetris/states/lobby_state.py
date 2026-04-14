import pygame
import struct
from typing import Optional, cast

from tetris.models.dataclass import *
from tetris.config.define import *
from tetris.net.packet_types import *

from tetris.net.session import Session
from tetris.net.network import NetworkWorker
from tetris.net.info_types import *
from tetris.resources.resource_manager import *
from tetris.resources.fonts import *
from tetris.resources.define_colors import *
from tetris.resources.define import *

from tetris.ui.rectangle import Rectangle
from tetris.ui.button import Button
from tetris.ui.inputbox import InputBox
from tetris.ui.popupbox import PopupBox
from tetris.ui.input_window import InputWindow
from tetris.ui.room_list import RoomList
from tetris.ui.chat_window import ChatWindow
from tetris.ui.my_info import Profile
from tetris.ui.fast_matching_window import FastMatchingWindow
from tetris.ui.create_room_window import CreateRoomWindow
from tetris.ui.setting_window import SettingWindow
from tetris.ui.user_taps import UserTabs, USER_TAB_NAME, FRIEND_TAB_NAME
from tetris.states.single_play_state import SinglePlayState
from tetris.states.multi_play_state import MultiPlayState
from tetris.states.ranking_state import RankingState
from tetris.states.base_state import BaseState
from tetris.net.packet_manager import *
from tetris.net.error_types import *
from tetris.net.info_types import *
from tetris.animation.shutter_animaion import ShutterAnimation

MENU_WIDTH = 200
MENU_HEIGHT = 100
RANKING_MENU_TEXT = "\uB7AD\uD0B9"

class LobbyState(BaseState):
    def __init__(self, screen: pygame.Surface, rm: ResourceManager,
                 net_worker: NetworkWorker, session: Session, is_animation: bool = False):
        super().__init__(screen, rm, net_worker, session)
        self.top_menus: list[Button] = []
        self.room_list = RoomList(screen, pygame.Rect(50, 150, 1000, 400), rm)
        self.btn_refresh = Button(screen, pygame.Rect(950, 150, 100, 50), rm, None, "새로고침") # 참가 버튼처럼 취급하려면 room_info에 net_worker가 넘어가야 하는데.. 그러기 싫어서 여기에 둠
        input_box_rect = pygame.Rect(50, 810, 1000, 30)
        self.chat_input_box = InputBox(screen, input_box_rect, self.rm,
                                       "채팅을 입력하세요", MAX_CHAT_INPUT, is_password=False, allow_korean=True)
        self.chat_window = ChatWindow(screen, pygame.Rect(50, 600, 1000, 200), self.chat_input_box.font)
        self.my_info_rect = Profile(screen, pygame.Rect(1050, 600, 300, 300), rm, session)
        self.user_tabs = UserTabs(screen, pygame.Rect(1050, 150, 300, 400), rm, [USER_TAB_NAME, FRIEND_TAB_NAME])

        self.open_shutter = ShutterAnimation(screen, rm)
        self.is_animation = is_animation
        
        self.error_popup = None
        self.fast_matching_window = None
        self.room_create_window = None
        self.setting_window = None
        self.exit_popup = None
        self.save_success_popup = None
        self.input_pw_window = None
        self.reactable = True

        self.info_popup = None
        self.friend_request_popup = None
        self.friend_requester_id = None

        self.friend_ev_btn = None
        self.friend_ev_target_id = None

        self.join_room_gen = None
    
        self.set_layout()

        if is_animation:
            self.open_shutter.start_animation()

    def set_layout(self):
        draw_x, draw_y = 50, 0
        logo_img = self.rm.images.ui_images[UI_LOGO]
        logo_w, logo_h = logo_img.get_size()
        logo_rect = pygame.Rect(draw_x, draw_y, logo_w, logo_h)
        self.logo = Rectangle(self.screen, logo_rect, self.rm, logo_img, "")
        
        draw_x += logo_rect.w
        menu_texts: list[str] = ["빠른시작", "방만들기", "상점", "설정", "", "게임종료"]
        for menu_text in menu_texts:
            menu_rect = pygame.Rect(draw_x, draw_y, MENU_WIDTH, MENU_HEIGHT) 
            menu = Button(self.screen, menu_rect, self.rm, None, menu_text)
            self.top_menus.append(menu)
            draw_x += MENU_WIDTH
        self.top_menus[4].button.set_text(RANKING_MENU_TEXT)

        self.request_room_list()
        self.request_lobby_user_list()
        self.request_friend_list()

    # def on_resize(self, w, h, screen):
    #     self.screen = screen
    #     self.set_layout()

    def request_room_list(self):
        self.room_list.clear()
        packet = self.net_worker.builder.build_request_room_list_pkt()
        self.net_worker.send_packet(packet)

    def request_lobby_user_list(self):
        self.user_tabs.clear(USER_TAB_NAME)
        packet = self.net_worker.builder.build_request_lobby_user_list_pkt()
        self.net_worker.send_packet(packet)

    def request_friend_list(self):
        self.user_tabs.clear(FRIEND_TAB_NAME)
        packet = self.net_worker.builder.build_request_friend_list_pkt()
        self.net_worker.send_packet(packet)

    def handle_packet(self, data: RecvPacketStruct):
        if data:
            if data.type == S2C_ERROR:
                error_data = cast(S2C_ERROR_PACKET, data)
                error_message = ERROR_MESSAGES[error_data.error_code]
                self.error_popup = PopupBox(self.screen, self.rm, error_message, ["확인"])
                self.reactable = False

            elif data.type == S2C_INFO:
                info_data = cast(S2C_INFO_PACKET, data)
                info_message = INFO_MESSAGES[info_data.info_type]
                self.info_popup = PopupBox(self.screen, self.rm, info_message, ["확인"])
                self.reactable = False

            elif data.type == S2C_LOBBY_USER_INFO or \
                    data.type == S2C_FRIEND_INFO or \
                    data.type == S2C_ADD_FRIEND or \
                    data.type == S2C_DELETE_FRIEND or \
                    data.type == S2C_REQUEST_FRIEND:

                result = self.user_tabs.handle_packet(data)

                if data.type == S2C_REQUEST_FRIEND and result is not None:
                    self.friend_requester_id = result.requester_id
                    fr_message = f"{result.requester_nickname} 님이 친구 요청을 보냈습니다."
                    self.friend_request_popup = PopupBox(self.screen,self.rm, fr_message,["수락", "닫기"])
                    self.reactable = False

            elif data.type == S2C_MESSAGE:
                message_data = cast(S2C_MESSAGE_PACKET, data)
                self.chat_window.add_new_message(message_data.user_name, message_data.message)

            elif data.type == S2C_ROOM_INFO:
                info_data = cast(S2C_ROOM_INFO_PACKET, data)
                self.room_list.add_room(info_data)

            elif data.type == S2C_ADD_OPEN_ROOM:
                open_data = cast(S2C_ADD_OPEN_ROOM_PACKET, data)
                if open_data.max_user == 1:
                    self.queue_state(SinglePlayState(self.screen, self.rm, self.net_worker,
                                                     self.session, open_data.room_name))
                elif open_data.max_user == 2 or open_data.max_user == 5:
                    self.queue_state(MultiPlayState(self.screen, self.rm, self.net_worker,
                                                    self.session, open_data.room_name, open_data.max_user))

            elif data.type == S2C_ADD_LOCK_ROOM:
                lock_data = cast(S2C_ADD_LOCK_ROOM_PACKET, data)
                if lock_data.max_user == 1:
                    self.queue_state(SinglePlayState(self.screen, self.rm, self.net_worker,
                                                     self.session, lock_data.room_name, lock_data.room_password))
                elif lock_data.max_user == 2 or lock_data.max_user == 5:
                    self.queue_state(MultiPlayState(self.screen, self.rm, self.net_worker,
                                                    self.session, lock_data.room_name, lock_data.max_user, lock_data.room_password))
        return self

    def handle_event(self, ev: pygame.event.Event):
        if ev.type == pygame.QUIT:
            pygame.quit()
            raise SystemExit
        
        event = None
        if self.reactable:
            for menu in self.top_menus:
                if menu.handle_event(ev): # 이벤트 함수의 반환값 형태 통일이 필요할 것 같긴 한데..
                    event = menu.button.text
                    break
            
            # 리스트로 만들어놔서 각 버튼마다 이름이 없어서 텍스트로 접근
            if event is not None:
                if event == "빠른시작":
                    self.fast_matching_window = FastMatchingWindow(self.screen, self.rm, self.net_worker)
                    self.reactable = False

                elif event == "방만들기":
                    self.room_create_window = CreateRoomWindow(self.screen, self.rm, self.net_worker, self.session)
                    self.reactable = False
                    
                # 나중에 메뉴별 상태 만들고 동작 추가
                elif event == "게임종료":
                    popup_texts = ["게임종료", "계속하기"]
                    self.exit_popup = PopupBox(self.screen, self.rm, "종료하시겠습니까?", popup_texts)
                    self.reactable = False

                elif event == "설정":
                    sw, sh = self.screen.get_size()
                    setting_window_w = sw // 4
                    setting_window_h = sh // 3
                    setting_window_x = sw // 2 - setting_window_w // 2
                    setting_window_y = sh // 2 - setting_window_h // 2
                    setting_window_rect = pygame.Rect(setting_window_x, setting_window_y, setting_window_w, setting_window_h)
                    self.setting_window = SettingWindow(self.screen, setting_window_rect, self.rm)
                    self.reactable = False
                elif event == RANKING_MENU_TEXT:
                    self.queue_state(RankingState(self.screen, self.rm, self.net_worker, self.session))
                return

            index = self.room_list.handle_event(ev)
            if index != None:
                room = self.room_list.show_rooms[index]
                self.join_room_gen = room.data.room_gen
                buttons_text = ["참가", "취소"]
                if room.data.is_private: 
                    self.input_pw_window = InputWindow(self.screen, self.rm, buttons_text) 
                    self.reactable = False
                else:
                    packet = self.net_worker.builder.build_join_room_pkt(self.join_room_gen, None)
                    self.net_worker.send_packet(packet)
                    self.join_room_gen = None
            
            if self.btn_refresh.handle_event(ev):
                self.room_list.clear()
                packet = self.net_worker.builder.build_request_room_list_pkt()
                self.net_worker.send_packet(packet)
                    
            self.chat_window.handle_event(ev) # 보여주기만 하므로 반환값은 없음
            message = self.chat_input_box.handle_event(ev)
            if message != None:
                packet = self.net_worker.builder.build_message_pkt(message)
                self.net_worker.send_packet(packet)
            
            if hasattr(ev, "pos"): # 마우스 이벤트만 넘긴다. 외부 클릭시 None이 넘어오고 그것을 토대로 버튼을 제거해야 한다
                if self.friend_ev_btn: # 버튼이 있다면 버튼 먼저
                    if self.friend_ev_btn.handle_event(ev):
                        if self.friend_ev_btn.button.text == "친구추가":
                            packet = self.net_worker.builder.build_request_friend_pkt(self.friend_ev_target_id)
                            self.net_worker.send_packet(packet)
                        elif self.friend_ev_btn.button.text == "친구삭제":
                            packet = self.net_worker.builder.build_delete_friend_pkt(self.friend_ev_target_id)
                            self.net_worker.send_packet(packet)
                        self.friend_ev_btn = None
                        self.friend_ev_target_id = None
                        return
                    if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1:
                        if not self.friend_ev_btn.button.rect.collidepoint(ev.pos):
                            self.friend_ev_btn = None
                            self.friend_ev_target_id = None
                
                friend_event = self.user_tabs.handle_event(ev) # 버튼이 없으면 탭으로 (버튼이 탭 위에 있음)
                if friend_event == USER_TAB_NAME:
                    self.request_lobby_user_list()
                    return
                if friend_event == FRIEND_TAB_NAME:
                    self.request_friend_list()
                    return
                if friend_event != None:
                    btn_rect = pygame.Rect(friend_event.pos[0], friend_event.pos[1], 100, 50)
                    self.friend_ev_target_id = friend_event.target_id
                    if friend_event.ev_type == "add":
                        btn_text = "친구추가"

                    elif friend_event.ev_type == "delete":
                        btn_text = "친구삭제"

                    self.friend_ev_btn = Button(self.screen, btn_rect, self.rm, None, btn_text)

        else:
            if self.error_popup:
                if self.error_popup.handle_event(ev):
                    self.error_popup = None
                    self.reactable = True

            elif self.fast_matching_window:
                if self.fast_matching_window.handle_event(ev): 
                    self.fast_matching_window = None
                    self.reactable = True

            elif self.room_create_window:
                rcw_event = self.room_create_window.handle_event(ev)
                if rcw_event == "만들기" or rcw_event == "취소":
                    self.room_create_window = None
                    self.reactable = True

            elif self.exit_popup:
                ep_event = self.exit_popup.handle_event(ev)
                if ep_event == "게임종료":
                    packet = self.net_worker.builder.build_disconnect_pkt()
                    self.net_worker.send_packet(packet)
                    pygame.quit()
                    raise SystemExit
                
                elif ep_event == "계속하기":
                    self.exit_popup = None
                    self.reactable = True

            elif self.setting_window:
                sw_event = self.setting_window.handle_event(ev)
                if sw_event == "성공":
                    self.save_success_popup = PopupBox(self.screen, self.rm, "저장에 성공했습니다.", ["확인"])
                    self.setting_window = None
            
                elif sw_event == "취소":                    
                    self.reactable = True
                    self.setting_window = None

            elif self.save_success_popup:
                ssp_event = self.save_success_popup.handle_event(ev)
                if ssp_event == "확인":
                    self.reactable = True
                    self.save_success_popup = None

            elif self.input_pw_window:
                ipw_event = self.input_pw_window.handle_event(ev)
                if ipw_event != None:
                    if ipw_event == "참가": # 내부 즉시 전송은 패스워드를 입력할 경우 복잡해진다
                        packet = self.net_worker.builder.build_join_room_pkt(self.join_room_gen, self.input_pw_window.input.extract_text())
                        self.net_worker.send_packet(packet)

                    # 참가/취소는 send 유무의 차이
                    self.reactable = True
                    self.input_pw_window = None
                    self.join_room_gen = None

            elif self.info_popup:
                if self.info_popup.handle_event(ev) == "확인":
                    self.info_popup = None
                    self.reactable = True

            elif self.friend_request_popup:
                fr_event = self.friend_request_popup.handle_event(ev)
                if fr_event != None:
                    if fr_event == "수락":
                        packet = self.net_worker.builder.build_accept_friend_pkt(self.friend_requester_id)
                        self.net_worker.send_packet(packet)

                    self.friend_request_popup = None
                    self.friend_requester_id = None
                    self.reactable = True

            return

    def update(self, dt_ms, events):
        if self.is_animation and self.open_shutter.is_active: 
            self.open_shutter.update(dt_ms)
            return
        
        self.chat_input_box.update(dt_ms)
                    
        for ev in events:
            self.handle_event(ev)

        if self.room_create_window:
            self.room_create_window.update(dt_ms)

        if self.input_pw_window:
            self.input_pw_window.update(dt_ms)
        
        return self.consume_state()

    def draw(self):
        self.screen.fill(BLACK)
        self.logo.draw()
        
        for menu in self.top_menus:
            menu.draw()

        self.room_list.draw()
        self.btn_refresh.draw()
        self.chat_window.draw()
        self.chat_input_box.draw()
        self.my_info_rect.draw()
        self.user_tabs.draw()

        if self.error_popup: self.error_popup.draw()
        if self.fast_matching_window: self.fast_matching_window.draw()
        if self.room_create_window: self.room_create_window.draw()
        if self.setting_window: self.setting_window.draw()
        if self.exit_popup: self.exit_popup.draw()
        if self.save_success_popup: self.save_success_popup.draw()
        if self.is_animation and self.open_shutter.is_active: self.open_shutter.draw()
        if self.input_pw_window: self.input_pw_window.draw()
        if self.info_popup: self.info_popup.draw()
        if self.friend_request_popup: self.friend_request_popup.draw()
        if self.friend_ev_btn: self.friend_ev_btn.draw()
