#pragma once
#include "Common.h"

class Item {
public:
    Item(long long id, XMFLOAT3 pos, int item_type);

    void SetPosition(XMFLOAT3 pos) { _position = pos; }
    XMFLOAT3 GetPosition() const { return _position; }
    long long GetID() const { return _id; }
    void SetHolder(long long holder) { _holder_id = holder; }
    long long GetHolder() const { return _holder_id; }
    int GetType() const { return _type; }

private:

    long long       _id;
    XMFLOAT3        _position;
    long long       _holder_id = 0; // 0 = 지면에 있음
    int             _type;
};


