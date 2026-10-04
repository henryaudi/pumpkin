#include "pumpkin-core/engine/ENG_KvStore.hpp"

namespace pumpkin::core {

void KvStore::set(const std::string& sz_Key, const std::string& sz_Value) {
    m_umap_data[sz_Key] = sz_Value;
}

std::optional<std::string> KvStore::get(const std::string& sz_Key) const {
    auto it_Entry = m_umap_data.find(sz_Key);
    if (it_Entry == m_umap_data.end()) {
        // Key not found: return an empty optional.
        return std::nullopt;
    }
    return it_Entry->second;
}

bool KvStore::remove(const std::string& sz_Key) {
    return m_umap_data.erase(sz_Key) > 0;
}

std::size_t KvStore::size() const {
    return m_umap_data.size();
}

}  // namespace pumpkin::core
