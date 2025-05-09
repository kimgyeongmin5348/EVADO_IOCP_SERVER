#include "Item.h"

Item::Item(long long id, XMFLOAT3 pos, int item_type)
    : _id(id), _position(pos), _holder_id(0), _type(item_type) {
}