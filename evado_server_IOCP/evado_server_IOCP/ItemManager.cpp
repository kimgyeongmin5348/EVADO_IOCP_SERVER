#include "ItemManager.h"
#include "WorkerThread.h"


ItemManager::ItemManager() = default;

ItemManager::~ItemManager() {
    std::lock_guard<std::mutex> lock(_item_mutex);
    for (auto& [id, item] : _items) {
        delete item;
    }
    _items.clear();
}

void ItemManager::SpawnItem(long long id, XMFLOAT3 pos, int item_type, short cash) {
    std::lock_guard<std::mutex> lock(_item_mutex);

    // 기존 아이템 중복 확인
    if (_items.find(id) != _items.end()) {
        std::cerr << "[서버] 아이템 생성 실패: 중복 ID " << id << "\n";
        return;
    }

    Item* new_item = new Item(id, pos, item_type, cash);
    _items[id] = new_item;
    //std::cout << "[서버] 아이템 생성: ID=" << id
    //    << " 위치(" << pos.x << "," << pos.y << "," << pos.z << ")"
    //    << " 타입: " << item_type << ", 값어치 : " << cash << "\n";
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

//Item* ItemManager::GetItem(long long id) {
//    std::lock_guard<std::mutex> lock(_item_mutex);
//    auto it = _items.find(id);
//    return (it != _items.end()) ? it->second : nullptr;
//}

void ItemManager::UpdateItemPosition(long long id, XMFLOAT3 pos, XMFLOAT3 right, XMFLOAT3 look)
{
    std::lock_guard<std::mutex> lock(_item_mutex);

    auto it = _items.find(id);
    if (it == _items.end()) {
        return;
    }

    // 실제 아이템 위치 갱신
    it->second->SetPosition(pos);
    it->second->SetRotate(right, look);

    sc_packet_item_move move_pkt;
    move_pkt.size = sizeof(move_pkt);
    move_pkt.type = SC_P_ITEM_MOVE;
    move_pkt.item_id = id;
    move_pkt.position = pos;
    move_pkt.right = right;
    move_pkt.look = look;

    BroadcastToAll(&move_pkt, -1);
}
