#include "pumpkin-core/engine/kv_store.hpp"
#include <gtest/gtest.h>
#include <string>

namespace pumpkin::core {
namespace {

TEST(KvStore, GetMissingKeyReturnsNothing) {
    KvStore store;
    EXPECT_FALSE(store.get("missing").has_value());
}

TEST(KvStore, SetThenGetReturnsValue) {
    KvStore store;
    store.set("name", "old");
    store.set("name", "new");
    EXPECT_EQ(store.get("name"), "new");
    EXPECT_EQ(store.size(), 1U);
}

TEST(KvStore, RemoveExistingKeyReturnsTrue) {
    KvStore store;
    store.set("name", "pumpkin");
    EXPECT_TRUE(store.remove("name"));
    EXPECT_FALSE(store.get("name").has_value());
}

TEST(KvStore, RemoveMissingKeyReturnsFalse) {
    KvStore store;
    EXPECT_FALSE(store.remove("missing"));
}

TEST(KvStore, SizeCountsKeys) {
    KvStore store;
    EXPECT_EQ(store.size(), 0u);
    store.set("a", "1");
    store.set("b", "2");
    EXPECT_EQ(store.size(), 2u);
    store.remove("a");
    EXPECT_EQ(store.size(), 1u);
}

TEST(KvStore, GetDoesNotCreateKey) {
    KvStore store;
    store.get("missing");
    EXPECT_EQ(store.size(), 0u);
}

TEST(KvStore, KeysAreCaseSensitive) {
    KvStore store;
    store.set("Key", "upper");
    store.set("key", "lower");
    EXPECT_EQ(store.get("Key"), "upper");
    EXPECT_EQ(store.get("key"), "lower");
}

TEST(KvStore, EmptyKeyAndEmptyValueAreAllowed) {
    KvStore store;
    store.set("", "");
    EXPECT_EQ(store.get(""), "");
    EXPECT_EQ(store.size(), 1u);
}

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
