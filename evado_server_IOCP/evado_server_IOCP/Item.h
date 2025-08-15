#pragma once
#include "Common.h"

class Item {
public:
    Item(long long id, XMFLOAT3 pos, int item_type, short cash);

    void SetPosition(XMFLOAT3 pos) { _item_position = pos; }
    XMFLOAT3 GetPosition() const { return _item_position; }

    long long GetID() const { return _item_id; }

    int GetType() const { return _type; }

    void SetRotate(XMFLOAT3 right, XMFLOAT3 look) { _item_right = right; _item_look = look; }
    XMFLOAT3 GetRight() const { return _item_right; }
    XMFLOAT3 GetLook() const { return _item_look; }

public:

    long long       _item_id;
    XMFLOAT3        _item_position;
    XMFLOAT3        _item_right;
    XMFLOAT3        _item_look;
    int             _type;
    short           _cash;
};


