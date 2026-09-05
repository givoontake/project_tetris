#include <immintrin.h>
#include "GameThreadManager.h"
#include "FivePlayerRoom.h"
#include "TetrisServer.h"
#include "ServerThreadManager.h"
#include "GamePhaseContext.h"
#include "LobbyThreadManager.h"
#include "MultiRoom.h"
#include "SingleRoom.h"
#include "TwoPlayerRoom.h"
#include "lobby_tasks.h"
#include "room_packets.h"

GameThreadManager::GameThreadManager(TetrisServer& tetris_server, TickWaitPolicy tick_policy)
    : tetris_server_(tetris_server), tick_wait_policy_(tick_policy)
{
}

void GameThreadManager::Start()
{
    thread_objects_.reserve(THREAD_COUNT);
    threads_.reserve(THREAD_COUNT);
    for (int i = 0; i < THREAD_COUNT; ++i) {
        thread_objects_.emplace_back(std::make_unique<GameThread>(*this));
        thread_objects_.back()->Start();
        threads_.emplace_back(&ServerThread::Run, thread_objects_.back().get());
    }
}

int GameThreadManager::GetAvailableThreadCount() const
{
    int available_thread_count = 0;
    for (const auto& game_thread : thread_objects_) {
        if (game_thread->state_.load() == GameThreadState::AVAILABLE) ++available_thread_count;
    }
    return available_thread_count;
}

std::vector<GameThread*> GameThreadManager::SelectThreads()
{
    std::vector<GameThread*> selected_game_threads;
    selected_game_threads.reserve(THREAD_COUNT);
    for (const auto& game_thread : thread_objects_) {
        GameThreadState expected_state = GameThreadState::AVAILABLE;
        if (game_thread->state_.compare_exchange_strong(expected_state, GameThreadState::PROCESSING))
            selected_game_threads.emplace_back(game_thread.get());
    }
    return selected_game_threads;
}

bool GameThreadManager::StartGamePhase(long long tick_time_ms)
{
    if (!tetris_server_.IsRunning()) return false;
    const std::vector<GameThread*> selected_game_threads = SelectThreads();
    if (selected_game_threads.size() < MIN_AVAILABLE_THREAD_COUNT) {
        for (GameThread* game_thread : selected_game_threads)
            game_thread->state_.store(GameThreadState::AVAILABLE);
        return false;
    }

    // 일부 게임 스레드 작업이 완료되지 않아도 다음 틱으로 넘어갈 수 있으므로 게임 처리 시작에 항상 새 GamePhaseContext를 생성한다.
	const auto phase_context = std::make_shared<GamePhaseContext>(tick_time_ms);
    {
        std::lock_guard<std::mutex> lock(tick_mutex_);
        for (GameThread* game_thread : selected_game_threads) {
            game_thread->phase_context_ = phase_context;
            game_thread->phase_.store(GamePhase::ROOM_PROCESS);
        }
    }
    tick_cv_.notify_all();
    return true;
}

void GameThreadManager::Enqueue(std::unique_ptr<RoomLifecycleTask> task)
{
	lifecycle_tasks_.Enqueue(std::move(task));
}

int GameThreadManager::TryJoinRoom(SessionKey session_key, int room_gen, const std::string& room_password, int matching_max_player_count)
{
	auto* session = tetris_server_.FindSession(session_key);
	if (!session || session->GetModeState() != ModeState::LOBBY) return ErrorCode::INVALID_REQUEST;
	auto room = FindRoomByGen(room_gen);
	if (!room) return ErrorCode::ROOM_NOT_FOUND;
	if (room->GetMaxPlayerCount() == 1) return ErrorCode::INVALID_REQUEST;
	if (!tetris_server_.GetThreadManager().GetLobbyThreadManager().BeginRoomTransition(session_key)) return ErrorCode::INVALID_REQUEST;
	auto task = std::make_unique<RoomLifecycleTask>(RoomLifecycleTaskType::JOIN_ROOM, session_key);
	task->room_gen = room_gen;
	task->room_password = room_password;
	task->matching_max_player_count = matching_max_player_count;
	Enqueue(std::move(task));
	return SUCCESS;
}

void GameThreadManager::ProcessTask(std::unique_ptr<RoomLifecycleTask> task)
{
	if (!task) return;
	if (task->task_type == RoomLifecycleTaskType::DELETE_ROOM) {
		DeleteRoom(task->room_index);
		return;
	}
	if (task->task_type == RoomLifecycleTaskType::LOBBY_TRANSITION_RESULT) {
		auto room = tetris_server_.GetRoomByIndex(task->room_index);
		if (!room || room->GetRoomGen() != task->room_gen) return;
		RoomProcessState expected_state = RoomProcessState::COMPLETE;
		while (!room->processing_state_.compare_exchange_weak(expected_state, RoomProcessState::PROCESSING)) {
			if (!tetris_server_.IsRunning()) return;
			expected_state = RoomProcessState::COMPLETE;
			_mm_pause();
		}
		room->CompleteLobbyTransition(task->session_key, task->result, task->exit_type);
		room->processing_state_.store(RoomProcessState::COMPLETE);
		return;
	}
	if (task->task_type == RoomLifecycleTaskType::JOIN_ROOM) {
		int result = ErrorCode::ROOM_NOT_FOUND;
		auto room = FindRoomByGen(task->room_gen);
		if (room) {
			RoomProcessState expected_state = RoomProcessState::COMPLETE;
			while (!room->processing_state_.compare_exchange_weak(expected_state, RoomProcessState::PROCESSING)) {
				if (!tetris_server_.IsRunning()) return;
				expected_state = RoomProcessState::COMPLETE;
				_mm_pause();
			}

			auto* session = tetris_server_.FindSession(task->session_key);
			if (session && session->GetLifeState() == LifeState::ACTIVE) {
				while (!session->TryStartTaskProcessing()) {
					if (!tetris_server_.IsRunning()) {
						room->processing_state_.store(RoomProcessState::COMPLETE);
						return;
					}
					_mm_pause();
				}
				auto multi_room = std::dynamic_pointer_cast<MultiRoom>(room);
				if (multi_room && session->MatchesSessionKey(task->session_key) && session->GetModeState() == ModeState::LOBBY)
					result = multi_room->AddPlayer(*session, task->session_key, task->room_password);

				auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task->session_key);
				lobby_task->result = result;
				lobby_task->matching_max_player_count = task->matching_max_player_count;
				tetris_server_.EnqueueLobbyTask(std::move(lobby_task));
				if (result == SUCCESS) room->ActivatePlayer(task->session_key);
				session->CompleteTaskProcessing();
				room->processing_state_.store(RoomProcessState::COMPLETE);
				return;
			}
			room->processing_state_.store(RoomProcessState::COMPLETE);
		}

		auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task->session_key);
		lobby_task->result = result;
		lobby_task->matching_max_player_count = task->matching_max_player_count;
		tetris_server_.EnqueueLobbyTask(std::move(lobby_task));
		return;
	}

	auto* session = tetris_server_.FindSession(task->session_key);
	if (!session || session->GetLifeState() != LifeState::ACTIVE) {
		auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task->session_key);
		lobby_task->result = ErrorCode::INVALID_REQUEST;
		tetris_server_.EnqueueLobbyTask(std::move(lobby_task));
		return;
	}
	while (!session->TryStartTaskProcessing()) {
		if (!tetris_server_.IsRunning()) return;
		_mm_pause();
	}
	if (!session->MatchesSessionKey(task->session_key) || session->GetLifeState() != LifeState::ACTIVE || session->GetModeState() != ModeState::LOBBY) {
		session->CompleteTaskProcessing();
		auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task->session_key);
		lobby_task->result = ErrorCode::INVALID_REQUEST;
		tetris_server_.EnqueueLobbyTask(std::move(lobby_task));
		return;
	}

	int result = ErrorCode::INVALID_REQUEST;
	switch (task->task_type) {
	case RoomLifecycleTaskType::CREATE_PUBLIC:
		result = CreatePublicRoom(task->packet.data(), *session, task->session_key);
		break;
	case RoomLifecycleTaskType::CREATE_PRIVATE:
		result = CreatePrivateRoom(task->packet.data(), *session, task->session_key);
		break;
	default:
		break;
	}
	auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task->session_key);
	lobby_task->result = result;
	tetris_server_.EnqueueLobbyTask(std::move(lobby_task));
	if (result == SUCCESS) {
		const RoomSnapshot room_snapshot = session->GetRoomSnapshot();
		auto room = tetris_server_.GetRoomByIndex(room_snapshot.room_index);
		if (room) room->CompleteRoomInitialization();
	}
	session->CompleteTaskProcessing();
}

int GameThreadManager::CreatePublicRoom(char* packet, Session& session, SessionKey session_key)
{
	if (!packet) return ErrorCode::INVALID_REQUEST;
	C2S_ADD_PUBLIC_ROOM_PACKET* public_p = reinterpret_cast<C2S_ADD_PUBLIC_ROOM_PACKET*>(packet);
	PublicRoomInitData data;
	constexpr size_t MIN_ROOM_NAME_LENGTH = 4;
	if (public_p->max_player_count == 1) {
		data.max_player_count = public_p->max_player_count;
	}

	else if (public_p->max_player_count == 2 || public_p->max_player_count == 5) {
		data.max_player_count = public_p->max_player_count;
	}

	else return ErrorCode::INVALID_REQUEST;
	data.room_name = tetris_server_.CharBufToString(public_p->room_name, sizeof(public_p->room_name));
	if (data.room_name.size() < MIN_ROOM_NAME_LENGTH) return ErrorCode::INVALID_REQUEST;
	data.room_gen = tetris_server_.GenerateRoomGen();

	SP<TetrisRoom> new_room;

	for (int i = 0; i < MAX_ROOM_COUNT; ++i) {
		if (!tetris_server_.GetRoomByIndex(i)) {
			data.room_index = i;
			{
				if (data.max_player_count == 1) new_room = std::make_shared<SingleRoom>(&tetris_server_, data);
				else if (data.max_player_count == 2) new_room = std::make_shared<TwoPlayerRoom>(&tetris_server_, data);
				else new_room = std::make_shared<FivePlayerRoom>(&tetris_server_, data);
				if (tetris_server_.TryAddRoom(i, new_room)) {
					if (!new_room->AddHostSession(session, session_key)) {
						tetris_server_.TryRemoveRoom(i, new_room);
						return ErrorCode::INVALID_REQUEST;
					}
					tetris_server_.GetActiveRoomManager().AddRoom(data.room_gen, new_room);
					new_room->SendCreateRoom(session);
					return SUCCESS;
				}
			}
		}
	}

	// 나중에 방 못찾으면 추후 처리 필요
	return ErrorCode::SERVER_ERROR;
}

int GameThreadManager::CreatePrivateRoom(char* packet, Session& session, SessionKey session_key)
{
	if (!packet) return ErrorCode::INVALID_REQUEST;
	C2S_ADD_PRIVATE_ROOM_PACKET* private_p = reinterpret_cast<C2S_ADD_PRIVATE_ROOM_PACKET*>(packet);
	PrivateRoomInitData data;
	constexpr size_t MIN_ROOM_NAME_LENGTH = 4;
	constexpr size_t MIN_ROOM_PASSWORD_LENGTH = 4;
	if (private_p->max_player_count == 1) {
		data.max_player_count = private_p->max_player_count;
	}

	else if (private_p->max_player_count == 2 || private_p->max_player_count == 5) {
		data.max_player_count = private_p->max_player_count;
	}

	else return ErrorCode::INVALID_REQUEST;
	data.room_name = tetris_server_.CharBufToString(private_p->room_name, sizeof(private_p->room_name));
	data.room_password = tetris_server_.CharBufToString(private_p->room_password, sizeof(private_p->room_password));
	if (data.room_name.size() < MIN_ROOM_NAME_LENGTH || data.room_password.size() < MIN_ROOM_PASSWORD_LENGTH) return ErrorCode::INVALID_REQUEST;
	data.room_gen = tetris_server_.GenerateRoomGen();

	SP<TetrisRoom> new_room;

	for (int i = 0; i < MAX_ROOM_COUNT; ++i) {
		if (!tetris_server_.GetRoomByIndex(i)) {
			data.room_index = i;
			{
				if (data.max_player_count == 1) new_room = std::make_shared<SingleRoom>(&tetris_server_, data);
				else if (data.max_player_count == 2) new_room = std::make_shared<TwoPlayerRoom>(&tetris_server_, data);
				else new_room = std::make_shared<FivePlayerRoom>(&tetris_server_, data);
				if (tetris_server_.TryAddRoom(i, new_room)) {
					if (!new_room->AddHostSession(session, session_key)) {
						tetris_server_.TryRemoveRoom(i, new_room);
						return ErrorCode::INVALID_REQUEST;
					}
					tetris_server_.GetActiveRoomManager().AddRoom(data.room_gen, new_room);
					new_room->SendCreateRoom(session);
					return SUCCESS;
				}
			}
		}
	}
	return ErrorCode::SERVER_ERROR;
}

void GameThreadManager::ProcessPacket(char* packet, Session& session)
{
	if (!packet) return;
	const RoomSnapshot room_snapshot = session.GetRoomSnapshot();
	auto room = tetris_server_.GetRoomByIndex(room_snapshot.room_index);
	if (!room) {
		tetris_server_.SendError(session, ErrorCode::INVALID_REQUEST);
		return;
	}
	room->HandlePacket(packet, session);
}

void GameThreadManager::DeleteRoom(int room_index)
{
	auto room = tetris_server_.GetRoomByIndex(room_index);
	if (!room) return;
	tetris_server_.GetActiveRoomManager().RemoveRoom(room->GetRoomGen(), room);
	tetris_server_.TryRemoveRoom(room_index, room);
}

std::shared_ptr<TetrisRoom> GameThreadManager::FindRoomByGen(int room_gen)
{
	return tetris_server_.GetActiveRoomManager().FindRoomByGen(room_gen);
}

void GameThreadManager::Close()
{
    for (auto& thread_object : thread_objects_)
        thread_object->Close();
}

void GameThreadManager::Join()
{
    for (auto& thread : threads_)
        thread.join();
}
