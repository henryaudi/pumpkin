#include "pumpkin-core/engine/kv_store.hpp"

#include <gtest/gtest.h>

#include <string>

namespace pumpkin::core {
namespace {

// Test #1
TEST(KvStore, GetMissingKeyReturnsNothing) {
    KvStore store;
    EXPECT_FALSE(store.get("missing").has_value());
}

// Test #2
TEST(KvStore, SetThenGetReturnsValue) {
    KvStore store;
    store.set("name", "old");
    store.set("name", "new");
    EXPECT_EQ(store.get("name"), "new");
    EXPECT_EQ(store.size(), 1U);
}

// Test #3
TEST(KvStore, RemoveExistingKeyReturnsTrue) {
    KvStore store;
    store.set("name", "pumpkin");
    EXPECT_TRUE(store.remove("name"));
    EXPECT_FALSE(store.get("name").has_value());
}

// Test #4
TEST(KvStore, RemoveMissingKeyReturnsFalse) {
    KvStore store;
    EXPECT_FALSE(store.remove("missing"));
}

// Test #5
TEST(KvStore, SizeCountsKeys) {
    KvStore store;
    EXPECT_EQ(store.size(), 0u);
    store.set("a", "1");
    store.set("b", "2");
    EXPECT_EQ(store.size(), 2u);
    store.remove("a");
    EXPECT_EQ(store.size(), 1u);
}

// Test #6
TEST(KvStore, GetDoesNotCreateKey) {
    KvStore store;
    store.get("missing");
    EXPECT_EQ(store.size(), 0u);
}

// Test #7
TEST(KvStore, KeysAreCaseSensitive) {
    KvStore store;
    store.set("Key", "upper");
    store.set("key", "lower");
    EXPECT_EQ(store.get("Key"), "upper");
    EXPECT_EQ(store.get("key"), "lower");
}

// Test #8
TEST(KvStore, EmptyKeyAndEmptyValueAreAllowed) {
    KvStore store;
    store.set("", "");
    EXPECT_EQ(store.get(""), "");
    EXPECT_EQ(store.size(), 1u);
}

// Test #9
TEST(KvStore, KeysAndValuesCanContainZeroBytes) {
    KvStore store;
    const std::string key("a\0b", 3);
    const std::string value("x\0y", 3);
    store.set(key, value);

    /* Ensure the key is stored as "a\0b" not just "a\0", and the value as "x\0y" not just "x\0" */
    EXPECT_EQ(store.get(key), value);
    EXPECT_FALSE(store.get("a").has_value());
}

}  // namespace

}  // namespace pumpkin::core
