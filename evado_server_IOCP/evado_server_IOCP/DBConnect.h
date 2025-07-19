#pragma once
#include "Common.h"
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <stdint.h>

using namespace std;

struct DB_PLAYER_INFO {
	long long		_id;
	XMFLOAT3		_position;
	XMFLOAT3		_look;
	XMFLOAT3		_right;
	uint8_t			_animState;
	short			_hp;
	//short			_cash;
};

class DBConnect
{
public:
	bool			Connect();
	void			Clear();

	bool			Execute(const WCHAR* query);
	bool			Fetch();
	int				GetRowCount();
	void			Unbind();

public:
	bool			BindParam(SQLUSMALLINT paramIndex, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* index);
	bool			BindCol(SQLUSMALLINT columnIndex, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* index);
	void			HandleError(SQLRETURN ret);

public:
	bool			IsPlayerRegistered(long long id);
	bool			AddPlayerInfoInDatabase(const DB_PLAYER_INFO& info);
	bool			SavePlayerInfo(const DB_PLAYER_INFO& info);
	DB_PLAYER_INFO	ExtractPlayerInfo(long long id);

private:
	SQLHENV			_enviroment = SQL_NULL_HANDLE;
	SQLHDBC			_connection = SQL_NULL_HANDLE;
	SQLHSTMT		_statement = SQL_NULL_HANDLE;
	SQLRETURN		_retcode = SQL_SUCCESS;

};