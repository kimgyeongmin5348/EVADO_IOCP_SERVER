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

    std::vector<Item*> GetAllItems() {
        std::lock_guard<std::mutex> lock(_item_mutex);
        std::vector<Item*> items;
        for (const auto& [id, item] : _items) {
            items.push_back(item);
        }
        return items;
    }

private:
    std::unordered_map<long long, Item*> _items;
    mutable std::mutex _item_mutex;
};
