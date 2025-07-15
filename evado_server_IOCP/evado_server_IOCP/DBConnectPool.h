#pragma once
#include "DBConnect.h"

class DBConnectPool
{
public:
	DBConnectPool();
	~DBConnectPool();

	bool					Connect(int connectionCount);
	void					Clear();

	DBConnect* Pop();
	void					Push(DBConnect* connection);

private:
	vector<DBConnect*>	_connections;
};