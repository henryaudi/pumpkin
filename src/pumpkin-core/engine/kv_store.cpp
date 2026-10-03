#include "pumpkin-core/engine/kv_store.hpp"

namespace pumpkin::core {

void KvStore::set(const std::string& key, const std::string& value) {
    data_[key] = value;
}

std::optional<std::string> KvStore::get(const std::string& key) const {
    auto it = data_.find(key);
    if (it == data_.end()) {
        /* Key not found, return empty optional */
        return std::nullopt;
    }
    return it->second;
}

bool KvStore::remove(const std::string& key) {
    return data_.erase(key) > 0;
}

std::size_t KvStore::size() const {
    return data_.size();
}

}  // namespace pumpkin::core
