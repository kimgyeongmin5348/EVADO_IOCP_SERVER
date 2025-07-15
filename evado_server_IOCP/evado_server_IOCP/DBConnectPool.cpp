#include "pch.h"
#include "DBConnectPool.h"

DBConnectPool::DBConnectPool()
{
}

DBConnectPool::~DBConnectPool()
{
}

bool DBConnectPool::Connect(int connectionCount)
{

	for (int i = 0; i < connectionCount; i++)
	{
		DBConnect* connection = new DBConnect();
		if (connection->Connect() == false)
			return false;

		_connections.push_back(connection);
	}

	cout << "DB Connect Success" << endl;
	return true;
}

void DBConnectPool::Clear()
{

	for (DBConnect* connection : _connections)
		delete(connection);

	_connections.clear();
}

DBConnect* DBConnectPool::Pop()
{

	if (_connections.empty())
		return nullptr;

	DBConnect* connection = _connections.back();
	_connections.pop_back();
	return connection;
}

void DBConnectPool::Push(DBConnect* connection)
{
	_connections.push_back(connection);
}
