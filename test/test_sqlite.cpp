#include <gtest/gtest.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include "utils/common.hpp"

TEST_F(DatabaseTest, BasicCRUD) {
    SQLite::Database &database = *db;

    // Insert data
    int nb = database.exec("INSERT INTO test (value) VALUES ('Hello SQLiteCpp')");
    EXPECT_EQ(nb, 1);

    // Read data
    SQLite::Statement query(database, "SELECT id, value FROM test");
    ASSERT_TRUE(query.executeStep());
    EXPECT_EQ(query.getColumn(0).getInt(), 1);
    EXPECT_STREQ(query.getColumn(1).getText(), "Hello SQLiteCpp");

    // Ensure no more rows
    EXPECT_FALSE(query.executeStep());
}
