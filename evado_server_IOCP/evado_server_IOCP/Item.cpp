#include "Item.h"

Item::Item(long long id, XMFLOAT3 pos, int item_type, short cash)
    : _item_id(id), _item_position(pos),/* _holder_id(0),*/ _type(item_type), _cash(cash) {
}