#include <gtest/gtest.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <vector>

#include "LibDatabase.hpp"

// -----------------------------------------------------------------------
// Helper: unique DB path per test (avoids "database is locked" under -jN)
// -----------------------------------------------------------------------
static std::string uniqueDbPath() {
  const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
  std::string name = info ? info->name() : "default";
  return (std::filesystem::temp_directory_path() / ("test_libdb_" + name + ".db")).string();
}

// -----------------------------------------------------------------------
// Fixture: manages a temporary DB file for LibDatabase unit tests
// -----------------------------------------------------------------------
class LibDatabaseTest : public ::testing::Test {
protected:
  std::string db_path_;
  std::unique_ptr<LibDatabase> db_;

  void SetUp() override {
    db_path_ = uniqueDbPath();
    std::filesystem::remove(db_path_);
    db_ = std::make_unique<LibDatabase>(db_path_);
    db_->initialize();
  }

  void TearDown() override {
    db_.reset();
    std::error_code ec;
    std::filesystem::remove(db_path_, ec);
  }
};

// =======================================================================
// Unit tests for LibDatabase
// =======================================================================

TEST_F(LibDatabaseTest, InitializeCreatesTable) {
  SQLite::Database db(db_path_, SQLite::OPEN_READWRITE);
  SQLite::Statement stmt(db, "SELECT count(*) FROM sqlite_master "
                             "WHERE type='table' AND name='lut_entries'");
  ASSERT_TRUE(stmt.executeStep());
  EXPECT_EQ(stmt.getColumn(0).getInt(), 1);
}

TEST_F(LibDatabaseTest, WriteAndReadSingleEntry) {
  std::vector<double> index_1 = {0.01, 0.02, 0.03};
  std::vector<double> index_2 = {0.1, 0.2, 0.3};
  std::vector<double> values = {
      1.0, 2.0, 3.0,  //
      4.0, 5.0, 6.0,  //
      7.0, 8.0, 9.0   //
  };

  db_->writeLutEntry("path/to/test.lib", "test_library", "SS_1p08V_125C", 0.01,
                     "INVD0", "Y", "A", "negative_unate", "combinational", "!B",
                     "cell_rise", 3, 3, index_1, index_2, values, 1, 125.0, 1.08);

  SQLite::Database db(db_path_, SQLite::OPEN_READWRITE);
  SQLite::Statement q(db,
      "SELECT file_path, library_name, scenario_id, "
      "pvt_corner, aged_year, cell_name, output_pin, "
      "related_pin, timing_sense, timing_type, arc_type, "
      "rows_n, cols_n, "
      "index_1_blob, index_2_blob, values_blob, "
      "process, temperature, voltage "
      "FROM lut_entries");
  ASSERT_TRUE(q.executeStep());

  // Scalar fields
  EXPECT_STREQ(q.getColumn(0).getText(), "path/to/test.lib");
  EXPECT_STREQ(q.getColumn(1).getText(), "test_library");
  EXPECT_TRUE(q.getColumn(2).isNull()); // scenario_id must be NULL
  EXPECT_STREQ(q.getColumn(3).getText(), "SS_1p08V_125C");
  EXPECT_DOUBLE_EQ(q.getColumn(4).getDouble(), 0.01);
  EXPECT_STREQ(q.getColumn(5).getText(), "INVD0");
  EXPECT_STREQ(q.getColumn(6).getText(), "Y");
  EXPECT_STREQ(q.getColumn(7).getText(), "A");
  EXPECT_STREQ(q.getColumn(8).getText(), "negative_unate");
  EXPECT_STREQ(q.getColumn(9).getText(), "combinational");
  EXPECT_STREQ(q.getColumn(10).getText(), "cell_rise");
  EXPECT_EQ(q.getColumn(11).getInt(), 3);
  EXPECT_EQ(q.getColumn(12).getInt(), 3);
  EXPECT_EQ(q.getColumn(16).getInt(), 1);
  EXPECT_DOUBLE_EQ(q.getColumn(17).getDouble(), 125.0);
  EXPECT_DOUBLE_EQ(q.getColumn(18).getDouble(), 1.08);

  // BLOB: index_1 — 3 × float32 = 12 bytes
  {
    const void *blob = q.getColumn(13).getBlob();
    int blob_size = q.getColumn(13).getBytes();
    ASSERT_EQ(blob_size, 3 * static_cast<int>(sizeof(float)));
    const float *f = static_cast<const float *>(blob);
    EXPECT_FLOAT_EQ(f[0], 0.01f);
    EXPECT_FLOAT_EQ(f[1], 0.02f);
    EXPECT_FLOAT_EQ(f[2], 0.03f);
  }
  // BLOB: index_2 — 3 × float32 = 12 bytes
  {
    const void *blob = q.getColumn(14).getBlob();
    int blob_size = q.getColumn(14).getBytes();
    ASSERT_EQ(blob_size, 3 * static_cast<int>(sizeof(float)));
    const float *f = static_cast<const float *>(blob);
    EXPECT_FLOAT_EQ(f[0], 0.1f);
    EXPECT_FLOAT_EQ(f[1], 0.2f);
    EXPECT_FLOAT_EQ(f[2], 0.3f);
  }
  // BLOB: values — 3×3 × float32 = 36 bytes
  {
    const void *blob = q.getColumn(15).getBlob();
    int blob_size = q.getColumn(15).getBytes();
    ASSERT_EQ(blob_size, 9 * static_cast<int>(sizeof(float)));
    const float *f = static_cast<const float *>(blob);
    EXPECT_FLOAT_EQ(f[0], 1.0f);
    EXPECT_FLOAT_EQ(f[4], 5.0f);
    EXPECT_FLOAT_EQ(f[8], 9.0f);
  }
}

TEST_F(LibDatabaseTest, UniqueConstraintPreventsDuplicates) {
  std::vector<double> idx = {0.01, 0.0368, 0.1309};
  std::vector<double> idx2 = {0.00077, 0.0017, 0.00355};
  std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};

  // First insert
  db_->writeLutEntry("f.lib", "lib", "SS_1p08V_125C", 0.01,
                     "CELL", "Y", "A", "", "", "!B", "cell_rise",
                     3, 3, idx, idx2, vals, 1, 25.0, 1.0);

  // Second insert with same UNIQUE key — must be ignored
  std::vector<double> vals2(9, 0.0);
  std::vector<double> idx2b(3, 0.0);
  db_->writeLutEntry("f.lib", "lib", "SS_1p08V_125C", 0.01,
                     "CELL", "Y", "A", "", "", "!B", "cell_rise",
                     3, 3, idx, idx2b, vals2, 1, 25.0, 1.0);

  SQLite::Database db(db_path_, SQLite::OPEN_READWRITE);
  SQLite::Statement cnt(db, "SELECT count(*) FROM lut_entries");
  ASSERT_TRUE(cnt.executeStep());
  EXPECT_EQ(cnt.getColumn(0).getInt(), 1);

  // Original values preserved
  SQLite::Statement q(db, "SELECT index_1_blob FROM lut_entries");
  ASSERT_TRUE(q.executeStep());
  const float *f = static_cast<const float *>(q.getColumn(0).getBlob());
  EXPECT_FLOAT_EQ(f[0], 0.01f);
}

TEST_F(LibDatabaseTest, MultipleArcTypesWritten) {
  std::vector<double> idx1(7, 0.0);
  std::vector<double> idx2(7, 0.0);
  std::vector<double> vals(49, 0.42);

  for (int i = 0; i < 7; ++i) {
    idx1[i] = 0.01 + i * 0.2;
    idx2[i] = 0.001 + i * 0.01;
  }

  const char *ARC_TYPES[] = {"cell_fall", "cell_rise", "fall_transition", "rise_transition"};
  for (const char *at : ARC_TYPES) {
    db_->writeLutEntry("f.lib", "lib", "SS_1p08V_125C", 0.01,
                       "INVD0", "Y", "A", "negative_unate", "combinational", "!B",
                       at, 7, 7, idx1, idx2, vals, 1, 125.0, 1.08);
  }

  SQLite::Database db(db_path_, SQLite::OPEN_READWRITE);
  SQLite::Statement q(db, "SELECT arc_type FROM lut_entries ORDER BY arc_type");
  int count = 0;
  while (q.executeStep()) {
    EXPECT_STREQ(q.getColumn(0).getText(), ARC_TYPES[count]);
    count++;
  }
  EXPECT_EQ(count, 4);
}

// Same UNIQUE key (pvt/year/cell/pin/related_pin/arc_type) but different
// `when` conditions must both persist — the core regression for the silent-dedup
// data-loss bug. Before `when` joined the UNIQUE constraint, the second insert
// was silently dropped by INSERT OR IGNORE.
TEST_F(LibDatabaseTest, DifferentWhenSameKeyBothPersisted) {
  std::vector<double> idx1 = {0.01, 0.02, 0.03};
  std::vector<double> idx2 = {0.1, 0.2, 0.3};
  std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};

  // Identical key, different when condition
  db_->writeLutEntry("f.lib", "lib", "SS_1p08V_125C", 0.01,
                     "CELL", "Y", "A", "negative_unate", "combinational", "!B",
                     "cell_rise", 3, 3, idx1, idx2, vals, 1, 125.0, 1.08);
  db_->writeLutEntry("f.lib", "lib", "SS_1p08V_125C", 0.01,
                     "CELL", "Y", "A", "negative_unate", "combinational", "B",
                     "cell_rise", 3, 3, idx1, idx2, vals, 1, 125.0, 1.08);

  SQLite::Database db(db_path_, SQLite::OPEN_READWRITE);
  SQLite::Statement cnt(db, "SELECT count(*) FROM lut_entries");
  ASSERT_TRUE(cnt.executeStep());
  EXPECT_EQ(cnt.getColumn(0).getInt(), 2);

  // Both when conditions are distinct
  SQLite::Statement q(db,
      "SELECT DISTINCT \"when\" FROM lut_entries ORDER BY \"when\"");
  int distinct_when = 0;
  while (q.executeStep()) {
    distinct_when++;
  }
  EXPECT_EQ(distinct_when, 2);
}

// Same data inserted with different file_paths must deduplicate to one row.
// Before the fix, file_path was part of the UNIQUE constraint, so 3 different
// paths → 3 rows. After the fix, file_path is excluded from the constraint,
// so only the first insert survives (INSERT OR IGNORE).
TEST_F(LibDatabaseTest, DifferentPathsDeduplicated) {
  std::vector<double> idx = {0.01, 0.0368, 0.1309};
  std::vector<double> idx2 = {0.00077, 0.0017, 0.00355};
  std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};

  // Three inserts — identical semantic data, different file paths
  db_->writeLutEntry("data/raw/A.lib", "lib", "SS_1p08V_125C", 0.01,
                     "CELL", "Y", "A", "negative_unate", "combinational", "!B",
                     "cell_rise", 3, 3, idx, idx2, vals, 1, 25.0, 1.0);
  db_->writeLutEntry("mirror/B.lib", "lib", "SS_1p08V_125C", 0.01,
                     "CELL", "Y", "A", "negative_unate", "combinational", "!B",
                     "cell_rise", 3, 3, idx, idx2, vals, 1, 25.0, 1.0);
  db_->writeLutEntry("backup/C.lib", "lib", "SS_1p08V_125C", 0.01,
                     "CELL", "Y", "A", "negative_unate", "combinational", "!B",
                     "cell_rise", 3, 3, idx, idx2, vals, 1, 25.0, 1.0);

  SQLite::Database check(db_path_, SQLite::OPEN_READWRITE);
  SQLite::Statement cnt(check, "SELECT count(*) FROM lut_entries");
  ASSERT_TRUE(cnt.executeStep());
  EXPECT_EQ(cnt.getColumn(0).getInt(), 1);

  // The surviving row should hold the first-inserted file_path
  SQLite::Statement q(check, "SELECT file_path FROM lut_entries");
  ASSERT_TRUE(q.executeStep());
  EXPECT_STREQ(q.getColumn(0).getText(), "data/raw/A.lib");
}