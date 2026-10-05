/**
 ******************************************************************************
 * @file    ENG_KvStore.hpp
 * @author  Shangjie Zheng
 * @brief   Key-value store (KvStore) for one shard: public interface.
 *          This file declares functions to:
 *           + Store, look up and remove key-value pairs
 *           + Count the stored keys
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Shangjie Zheng.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 ******************************************************************************
 */

#ifndef PUMPKIN_ENG_KVSTORE_HPP_
#define PUMPKIN_ENG_KVSTORE_HPP_

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

namespace pumpkin::core {

/**
 * @brief In-memory key-value storage for one shard.
 *
 * @details Keys and values are arbitrary byte strings: they may be empty and
 *          may contain zero bytes.
 *
 * @warning Not thread-safe, on purpose. Each shard is owned by exactly one
 *          worker thread, so the store never needs a lock. Pumpkin scales
 *          across cores by having many KvStores (one per shard), not by
 *          sharing one KvStore between threads.
 */
class KvStore {
public:
    /**
     * @brief Inserts or updates a key-value pair in the store.
     *
     * @details This function will insert the key-value pair into the store if the key does not
     *          exist. If the key already exists, its value will be updated with the new value.
     *
     * @param sz_Key   The key to insert or update.
     * @param sz_Value The value to associate with the key.
     */
    void set(const std::string& sz_Key, const std::string& sz_Value);

    /**
     * @brief Looks up the value stored under a key.
     *
     * @details The store is never modified by a lookup: a missing key is not inserted.
     *
     * @param sz_Key The key to look up.
     *
     * @return A copy of the value, or std::nullopt if the key does not exist.
     */
    std::optional<std::string> get(const std::string& sz_Key) const;

    /**
     * @brief Removes a key and its value from the store.
     *
     * @param sz_Key The key to remove.
     *
     * @return true if the key existed and was removed, false if it did not exist.
     */
    bool remove(const std::string& sz_Key);

    /**
     * @brief Returns the number of key-value pairs in the store.
     *
     * @return The number of key-value pairs.
     */
    std::size_t size() const;

private:
    std::unordered_map<std::string, std::string> m_umap_data;  ///< key -> value
};

}  // namespace pumpkin::core

#endif  // PUMPKIN_ENG_KVSTORE_HPP_
