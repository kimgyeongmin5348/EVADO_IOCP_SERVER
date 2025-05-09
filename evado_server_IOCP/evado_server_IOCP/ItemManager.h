#pragma once
#include "Item.h"


class ItemManager {
public:
    ItemManager();
    ~ItemManager();

    void SpawnItem(long long id, XMFLOAT3 pos, int item_type);
    void DespawnItem(long long id);
    Item* GetItem(long long id);
    void UpdateItemPosition(long long id, XMFLOAT3 pos);

private:

    std::unordered_map<long long, Item*> _items;
    std::mutex _item_mutex;
};
