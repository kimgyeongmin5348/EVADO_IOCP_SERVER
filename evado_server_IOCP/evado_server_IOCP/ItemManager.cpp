#include "ItemManager.h"


ItemManager::ItemManager() = default;

ItemManager::~ItemManager() {
    std::lock_guard<std::mutex> lock(_item_mutex);
    for (auto& [id, item] : _items) {
        delete item;
    }
    _items.clear();
}

void ItemManager::SpawnItem(long long id, XMFLOAT3 pos, int item_type) {
    std::lock_guard<std::mutex> lock(_item_mutex);

    // ���� ������ �ߺ� Ȯ��
    if (_items.find(id) != _items.end()) {
        std::cerr << "[����] ������ ���� ����: �ߺ� ID " << id << "\n";
        return;
    }

    Item* new_item = new Item(id, pos, item_type);
    _items[id] = new_item;
    std::cout << "[����] ������ ����: ID=" << id
        << " ��ġ(" << pos.x << "," << pos.y << "," << pos.z << ")"
        << " Ÿ��: " << item_type << "\n";
}

void ItemManager::DespawnItem(long long id) {
    std::lock_guard<std::mutex> lock(_item_mutex);

    auto it = _items.find(id);
    if (it == _items.end()) {
        std::cerr << "[����] ������ ���� ����: �������� �ʴ� ID " << id << "\n";
        return;
    }

    delete it->second;
    _items.erase(it);
    std::cout << "[����] ������ ����: ID=" << id << "\n";
}

Item* ItemManager::GetItem(long long id) {
    std::lock_guard<std::mutex> lock(_item_mutex);
    auto it = _items.find(id);
    return (it != _items.end()) ? it->second : nullptr;
}

void ItemManager::UpdateItemPosition(long long id, XMFLOAT3 pos) {
    std::lock_guard<std::mutex> lock(_item_mutex);

    auto it = _items.find(id);
    if (it == _items.end()) {
        std::cerr << "[����] ������ ��ġ ������Ʈ ����: ID " << id << "\n";
        return;
    }

    it->second->SetPosition(pos);
    std::cout << "[����] ������ ��ġ ����: ID=" << id
        << " �� ��ġ(" << pos.x << "," << pos.y << "," << pos.z << ")\n";
}