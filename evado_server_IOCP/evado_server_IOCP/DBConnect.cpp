#include "DBConnect.h"

// 이부분 DB생성후 수정해야함!!!!!
bool DBConnect::Connect()
{
	// Allocate environment handle
	_retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &_enviroment);
	if (_retcode != SQL_SUCCESS && _retcode != SQL_SUCCESS_WITH_INFO)
		return false;

	// Set ODBC version environment attribute
	_retcode = SQLSetEnvAttr(_enviroment, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
	if (_retcode != SQL_SUCCESS && _retcode != SQL_SUCCESS_WITH_INFO)
		return false;

	// Allocate connection handle
	_retcode = SQLAllocHandle(SQL_HANDLE_DBC, _enviroment, &_connection);
	if (_retcode != SQL_SUCCESS && _retcode != SQL_SUCCESS_WITH_INFO)
		return false;

	// Set login timeout to 5 seconds
	_retcode = SQLSetConnectAttr(_connection, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);
	if (_retcode != SQL_SUCCESS && _retcode != SQL_SUCCESS_WITH_INFO)
		return false;

	// Connect to the data source
	_retcode = SQLConnect(_connection, (SQLWCHAR*)L"EVADO", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);
	if (_retcode != SQL_SUCCESS && _retcode != SQL_SUCCESS_WITH_INFO)
	{
		HandleError(_retcode);
		return false;
	}

	// Allocate statement handle
	_retcode = SQLAllocHandle(SQL_HANDLE_STMT, _connection, &_statement);
	if (_retcode != SQL_SUCCESS && _retcode != SQL_SUCCESS_WITH_INFO)
	{
		HandleError(_retcode);
		return false;
	}

	return true;
}

void DBConnect::Clear()
{
	if (_connection != SQL_NULL_HANDLE)
	{
		::SQLFreeHandle(SQL_HANDLE_DBC, _connection);
		_connection = SQL_NULL_HANDLE;
	}

	if (_statement != SQL_NULL_HANDLE)
	{
		::SQLFreeHandle(SQL_HANDLE_STMT, _statement);
		_statement = SQL_NULL_HANDLE;
	}
}

bool DBConnect::Execute(const WCHAR* query)
{
	SQLRETURN ret = ::SQLExecDirectW(_statement, (SQLWCHAR*)query, SQL_NTS);
	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		return true;

	HandleError(ret);
	return false;
}

bool DBConnect::Fetch()
{
	SQLRETURN ret = ::SQLFetch(_statement);

	switch (ret)
	{
	case SQL_SUCCESS:
	case SQL_SUCCESS_WITH_INFO:
		return true;
	case SQL_NO_DATA:
		return false;
	case SQL_ERROR:
		HandleError(ret);
		return false;
	default:
		return true;
	}
}

int DBConnect::GetRowCount()
{
	SQLLEN count = 0;
	SQLRETURN ret = ::SQLRowCount(_statement, OUT & count);

	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
		return static_cast<int>(count);

	return -1;
}

void DBConnect::Unbind()
{
	::SQLFreeStmt(_statement, SQL_UNBIND);
	::SQLFreeStmt(_statement, SQL_RESET_PARAMS);
	::SQLFreeStmt(_statement, SQL_CLOSE);
}

bool DBConnect::BindParam(SQLUSMALLINT paramIndex, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN len, SQLPOINTER ptr, SQLLEN* index)
{
	SQLRETURN ret = ::SQLBindParameter(_statement, paramIndex, SQL_PARAM_INPUT, cType, sqlType, len, 0, ptr, 0, index);
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
	{
		HandleError(ret);
		return false;
	}

	return true;
}

bool DBConnect::BindCol(SQLUSMALLINT columnIndex, SQLSMALLINT cType, SQLULEN len, SQLPOINTER value, SQLLEN* index)
{
	SQLRETURN ret = ::SQLBindCol(_statement, columnIndex, cType, value, len, index);
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
	{
		HandleError(ret);
		return false;
	}

	return true;
}

void DBConnect::HandleError(SQLRETURN ret)
{
	if (ret == SQL_SUCCESS)
		return;

	SQLSMALLINT index = 1;
	SQLWCHAR sqlState[MAX_PATH] = { 0 };
	SQLINTEGER nativeErr = 0;
	SQLWCHAR errMsg[MAX_PATH] = { 0 };
	SQLSMALLINT msgLen = 0;
	SQLRETURN errorRet = 0;

	while (true)
	{
		errorRet = ::SQLGetDiagRecW(
			SQL_HANDLE_STMT,
			_statement,
			index,
			sqlState,
			OUT & nativeErr,
			errMsg,
			_countof(errMsg),
			OUT & msgLen
		);

		if (errorRet == SQL_NO_DATA)
			break;

		if (errorRet != SQL_SUCCESS && errorRet != SQL_SUCCESS_WITH_INFO)
			break;

		// TODO : Log
		wcout.imbue(locale("kor"));
		wcout << errMsg << endl;

		index++;
	}
}

bool DBConnect::IsPlayerRegistered(long long id)
{
	wstring query = L"EXEC isPlayerRegistered ?";

	BindParam(1, SQL_C_SBIGINT, SQL_BIGINT, 0, (SQLPOINTER)&id, nullptr);

	Execute(query.c_str());

	SQLCHAR isRegistered{};
	SQLLEN cb_isRegistered{};

	BindCol(1, SQL_BIT, sizeof(isRegistered), &isRegistered, &cb_isRegistered);

	Fetch();
	Unbind();

	return (isRegistered == 1);
}

bool DBConnect::AddPlayerInfoInDatabase(const DB_PLAYER_INFO& info)
{
	wstring query = L"EXEC AddNewPlayer ?,?,?,?,?,?,?,?,?,?,?,?";  // 이거 12개임 db수정후 13개로 고쳐야함.

	// 1. id
	if (!BindParam(1, SQL_C_SBIGINT, SQL_BIGINT, 0, (SQLPOINTER)&info._id, nullptr)) return false;

	// 2~4. position
	if (!BindParam(2, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._position.x, nullptr)) return false;
	if (!BindParam(3, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._position.y, nullptr)) return false;
	if (!BindParam(4, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._position.z, nullptr)) return false;

	// 5~7. look
	if (!BindParam(5, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._look.x, nullptr)) return false;
	if (!BindParam(6, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._look.y, nullptr)) return false;
	if (!BindParam(7, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._look.z, nullptr)) return false;

	// 8~10. right
	if (!BindParam(8, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._right.x, nullptr)) return false;
	if (!BindParam(9, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._right.y, nullptr)) return false;
	if (!BindParam(10, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._right.z, nullptr)) return false;

	// 11. anim state
	uint8_t anim = info._animState;
	if (!BindParam(11, SQL_C_UTINYINT, SQL_TINYINT, 0, (SQLPOINTER)&anim, nullptr)) return false;

	// 12 ~ 13.  hp, cash
	if (!BindParam(12, SQL_C_SHORT, SQL_SMALLINT, 0, (SQLPOINTER)&info._hp, nullptr)) return false;
	//if (!BindParam(13, SQL_C_SHORT, SQL_SMALLINT, 0, (SQLPOINTER)&info._cash, nullptr)) return false;

	Execute(query.c_str());
	Unbind();

	return true;
}

DB_PLAYER_INFO DBConnect::ExtractPlayerInfo(long long id)
{
	DB_PLAYER_INFO info{};
	info._id = id;

	wstring query = L"EXEC ExtractPlayerInfo ?";

	BindParam(1, SQL_C_SBIGINT, SQL_BIGINT, 0, (SQLPOINTER)&id, nullptr);
	Execute(query.c_str());

	SQLLEN cb = 0;

	BindCol(1, SQL_C_FLOAT, sizeof(float), &info._position.x, &cb);
	BindCol(2, SQL_C_FLOAT, sizeof(float), &info._position.y, &cb);
	BindCol(3, SQL_C_FLOAT, sizeof(float), &info._position.z, &cb);

	BindCol(4, SQL_C_FLOAT, sizeof(float), &info._look.x, &cb);
	BindCol(5, SQL_C_FLOAT, sizeof(float), &info._look.y, &cb);
	BindCol(6, SQL_C_FLOAT, sizeof(float), &info._look.z, &cb);

	BindCol(7, SQL_C_FLOAT, sizeof(float), &info._right.x, &cb);
	BindCol(8, SQL_C_FLOAT, sizeof(float), &info._right.y, &cb);
	BindCol(9, SQL_C_FLOAT, sizeof(float), &info._right.z, &cb);

	BindCol(10, SQL_C_UTINYINT, sizeof(uint8_t), &info._animState, &cb);
	BindCol(11, SQL_C_SHORT, sizeof(short), &info._hp, &cb);
	//BindCol(12, SQL_C_SHORT, sizeof(short), &info._cash, &cb);

	Fetch();
	Unbind();

	return info;
}

bool DBConnect::SavePlayerInfo(const DB_PLAYER_INFO& info)
{
	wstring query = L"EXEC SavePlayerInfo ?,?,?,?,?,?,?,?,?,?,?,?";  // 이것도 12개임, db수정후 13개로 고쳐야함

	if (!BindParam(1, SQL_C_SBIGINT, SQL_BIGINT, 0, (SQLPOINTER)&info._id, nullptr)) return false;

	if (!BindParam(2, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._position.x, nullptr)) return false;
	if (!BindParam(3, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._position.y, nullptr)) return false;
	if (!BindParam(4, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._position.z, nullptr)) return false;

	if (!BindParam(5, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._look.x, nullptr)) return false;
	if (!BindParam(6, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._look.y, nullptr)) return false;
	if (!BindParam(7, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._look.z, nullptr)) return false;

	if (!BindParam(8, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._right.x, nullptr)) return false;
	if (!BindParam(9, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._right.y, nullptr)) return false;
	if (!BindParam(10, SQL_C_FLOAT, SQL_REAL, 0, (SQLPOINTER)&info._right.z, nullptr)) return false;

	if (!BindParam(11, SQL_C_UTINYINT, SQL_TINYINT, 0, (SQLPOINTER)&info._animState, nullptr)) return false;
	if (!BindParam(12, SQL_C_SHORT, SQL_SMALLINT, 0, (SQLPOINTER)&info._hp, nullptr)) return false;
	//if (!BindParam(13, SQL_C_SHORT, SQL_SMALLINT, 0, (SQLPOINTER)&info._cash, nullptr)) return false;

	Execute(query.c_str());
	Unbind();

	return true;
}

