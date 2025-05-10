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

    // 기존 아이템 중복 확인
    if (_items.find(id) != _items.end()) {
        std::cerr << "[서버] 아이템 생성 실패: 중복 ID " << id << "\n";
        return;
    }

    Item* new_item = new Item(id, pos, item_type);
    _items[id] = new_item;
    std::cout << "[서버] 아이템 생성: ID=" << id
        << " 위치(" << pos.x << "," << pos.y << "," << pos.z << ")"
        << " 타입: " << item_type << "\n";
}

void ItemManager::DespawnItem(long long id) {
    std::lock_guard<std::mutex> lock(_item_mutex);

    auto it = _items.find(id);
    if (it == _items.end()) {
        std::cerr << "[서버] 아이템 삭제 실패: 존재하지 않는 ID " << id << "\n";
        return;
    }

    delete it->second;
    _items.erase(it);
    std::cout << "[서버] 아이템 삭제: ID=" << id << "\n";
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
        std::cerr << "[서버] 아이템 위치 업데이트 실패: ID " << id << "\n";
        return;
    }

    it->second->SetPosition(pos);
    std::cout << "[서버] 아이템 위치 갱신: ID=" << id
        << " 새 위치(" << pos.x << "," << pos.y << "," << pos.z << ")\n";
}