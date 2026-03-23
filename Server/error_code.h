#pragma once


enum ERROR_CODE // 이거 이넘 클래스 쓰면 static_cast ㅈㄴ 써야해서 코드 더러워짐
{
	SUCCESS = 0, // 얘는 앞에 enum 이름 붙이지 말기(실제로는 에러가 아니니까)
	SERVER_ERROR = 1,
	INVALID_REQUEST = 2,

	ROOM_NOT_FOUND = 10,
	ROOM_FULL = 11,
	ROOM_INGAME = 12,
};