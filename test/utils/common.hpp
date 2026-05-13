#ifndef TEST_UTILS_COMMON_HPP
#define TEST_UTILS_COMMON_HPP

#include <memory>
#include <gtest/gtest.h>
#include <SQLiteCpp/SQLiteCpp.h>

struct DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        db = std::make_unique<SQLite::Database>(":memory:", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        db->exec("CREATE TABLE test (id INTEGER PRIMARY KEY, value TEXT)");
    }
    void TearDown() override {
        db.reset();
    }
    std::unique_ptr<SQLite::Database> db;
};

#endif // TEST_UTILS_COMMON_HPP
