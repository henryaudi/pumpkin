/**
 ******************************************************************************
 * @file    ENG_KvStoreTest.cpp
 * @author  Shangjie Zheng
 * @brief   Unit tests for the key-value store (KvStore).
 *          This file tests:
 *           + set / get / remove / size behaviour
 *           + Edge cases: empty keys and values, zero bytes, case sensitivity
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

#include "core/pump-core-eng/ENG_KvStore.hpp"

#include <gtest/gtest.h>

#include <string>

namespace pumpkin::core {
namespace {

TEST(KvStore, GetMissingKeyReturnsNothing) {
    KvStore obj_Store;
    EXPECT_FALSE(obj_Store.get("missing").has_value());
}

TEST(KvStore, SetThenGetReturnsValue) {
    KvStore obj_Store;
    obj_Store.set("name", "pumpkin");
    EXPECT_EQ(obj_Store.get("name"), "pumpkin");
}

TEST(KvStore, SetOverwritesExistingValue) {
    KvStore obj_Store;
    obj_Store.set("name", "old");
    obj_Store.set("name", "new");
    EXPECT_EQ(obj_Store.get("name"), "new");
    EXPECT_EQ(obj_Store.size(), 1u);
}

TEST(KvStore, RemoveExistingKeyReturnsTrue) {
    KvStore obj_Store;
    obj_Store.set("name", "pumpkin");
    EXPECT_TRUE(obj_Store.remove("name"));
    EXPECT_FALSE(obj_Store.get("name").has_value());
}

TEST(KvStore, RemoveMissingKeyReturnsFalse) {
    KvStore obj_Store;
    EXPECT_FALSE(obj_Store.remove("missing"));
}

TEST(KvStore, SizeCountsKeys) {
    KvStore obj_Store;
    EXPECT_EQ(obj_Store.size(), 0u);
    obj_Store.set("a", "1");
    obj_Store.set("b", "2");
    EXPECT_EQ(obj_Store.size(), 2u);
    obj_Store.remove("a");
    EXPECT_EQ(obj_Store.size(), 1u);
}

TEST(KvStore, GetDoesNotCreateKey) {
    KvStore obj_Store;
    obj_Store.get("missing");
    EXPECT_EQ(obj_Store.size(), 0u);
}

TEST(KvStore, KeysAreCaseSensitive) {
    KvStore obj_Store;
    obj_Store.set("Key", "upper");
    obj_Store.set("key", "lower");
    EXPECT_EQ(obj_Store.get("Key"), "upper");
    EXPECT_EQ(obj_Store.get("key"), "lower");
}

TEST(KvStore, EmptyKeyAndEmptyValueAreAllowed) {
    KvStore obj_Store;
    obj_Store.set("", "");
    EXPECT_EQ(obj_Store.get(""), "");
    EXPECT_EQ(obj_Store.size(), 1u);
}

TEST(KvStore, KeysAndValuesCanContainZeroBytes) {
    KvStore           obj_Store;
    const std::string sz_Key("a\0b", 3);  // 3 bytes: 'a', '\0', 'b'
    const std::string sz_Value("x\0y", 3);
    obj_Store.set(sz_Key, sz_Value);

    // The key must be all 3 bytes "a\0b", not cut short to "a".
    EXPECT_EQ(obj_Store.get(sz_Key), sz_Value);
    EXPECT_FALSE(obj_Store.get("a").has_value());
}

}  // namespace
}  // namespace pumpkin::core
