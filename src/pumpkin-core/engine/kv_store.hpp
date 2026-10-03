#ifndef PUMPKIN_CORE_ENGINE_KV_STORE_HPP_
#define PUMPKIN_CORE_ENGINE_KV_STORE_HPP_

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

namespace pumpkin::core {

class KvStore {
public:
    /**
     * @brief Inserts or updates a key-value pair in the store.
     *
     * @details This function will insert the key-value pair into the store if the key does not
     *          exist. If the key already exists, its value will be updated with the new value.
     *
     * @param key   The key to insert or update.
     * @param value The value to associate with the key.
     */
    void set(const std::string& key, const std::string& value);

    /**
     * @brief Looks up the value stored under a key.
     *
     * @details The store is never modified by a lookup: a missing key is not inserted.
     *
     * @param key The key to look up.
     *
     * @return A copy of the value, or std::nullopt if the key does not exist.
     */
    std::optional<std::string> get(const std::string& key) const;

    /**
     * @brief Removes a key and its value from the store.
     *
     * @param key The key to remove.
     *
     * @return true if the key existed and was removed, false if it did not exist.
     */
    bool remove(const std::string& key);

    /**
     * @brief Returns the number of key-value pairs in the store.
     *
     * @return The number of key-value pairs.
     */
    std::size_t size() const;

private:
    std::unordered_map<std::string, std::string> data_;
};

}  // namespace pumpkin::core

#endif  // PUMPKIN_CORE_ENGINE_KV_STORE_HPP_
