#include <gtest/gtest.h>
#include <SQLiteCpp/SQLiteCpp.h>
#include <filesystem>
#include <memory>

#include "LibDatabase.hpp"
#include "LibFile.hpp"

// -----------------------------------------------------------------------
// Helper: unique DB path per test (avoids "database is locked" under -jN)
// -----------------------------------------------------------------------
static std::string uniqueDbPath() {
  const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
  std::string name = info ? info->name() : "default";
  return (std::filesystem::temp_directory_path() / ("test_libfile_" + name + ".db")).string();
}

// =======================================================================
// Integration test: LibFile writeToDB with a real .lib file
// =======================================================================
class LibFileWriteToDBTest : public ::testing::Test {
protected:
  std::string db_path_;

  void SetUp() override {
    db_path_ = uniqueDbPath();
    std::filesystem::remove(db_path_);
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove(db_path_, ec);
  }
};

TEST_F(LibFileWriteToDBTest, ParseAndWriteSmallLib) {
  std::string lib_path = std::string(TEST_DATA_DIR) + "/tcbn65lpbc.ski.lib";
  ASSERT_TRUE(std::filesystem::exists(lib_path))
      << "Test .lib file not found: " << lib_path;

  LibFile libfile(lib_path, "test_parse.log");
  libfile.parse();

  // Write to DB
  libfile.writeToDB(db_path_, "SS_1p08V_125C", 0.01);

  // Verify entries in DB
  SQLite::Database db(db_path_, SQLite::OPEN_READWRITE);
  SQLite::Statement q(db,
      "SELECT cell_name, output_pin, related_pin, arc_type, "
      "rows_n, cols_n, timing_sense, timing_type, "
      "process, temperature, voltage, library_name, file_path "
      "FROM lut_entries ORDER BY cell_name, related_pin, arc_type");

  int entry_count = 0;
  while (q.executeStep()) {
    entry_count++;

    std::string arc_type = q.getColumn(3).getText();
    int rows_n = q.getColumn(4).getInt();
    int cols_n = q.getColumn(5).getInt();

    EXPECT_EQ(rows_n, 7);
    EXPECT_EQ(cols_n, 7);
    EXPECT_TRUE(arc_type == "cell_rise" || arc_type == "cell_fall" ||
                arc_type == "rise_transition" || arc_type == "fall_transition");

    // Expected PVT from the .lib header
    EXPECT_DOUBLE_EQ(q.getColumn(9).getDouble(), 0.0);   // temperature
    EXPECT_DOUBLE_EQ(q.getColumn(10).getDouble(), 1.32);  // voltage
    EXPECT_EQ(q.getColumn(8).getInt(), 1);                // process

    // library_name = basename
    EXPECT_STREQ(q.getColumn(11).getText(), "tcbn65lpbc.ski");
    // file_path = original path
    EXPECT_STREQ(q.getColumn(12).getText(), lib_path.c_str());
  }

  // Must have some entries, and count divisible by 4 (4 arc types)
  EXPECT_GT(entry_count, 0);
  EXPECT_EQ(entry_count % 4, 0);
}

TEST_F(LibFileWriteToDBTest, ScenarioIdIsNull) {
  std::string lib_path = std::string(TEST_DATA_DIR) + "/tcbn65lpbc.ski.lib";
  LibFile libfile(lib_path, "test_null.log");
  libfile.parse();
  libfile.writeToDB(db_path_, "SS_1p08V_125C", 0.01);

  SQLite::Database db(db_path_, SQLite::OPEN_READWRITE);
  SQLite::Statement q(db, "SELECT count(*) FROM lut_entries WHERE scenario_id IS NOT NULL");
  ASSERT_TRUE(q.executeStep());
  EXPECT_EQ(q.getColumn(0).getInt(), 0);
}

TEST_F(LibFileWriteToDBTest, ValuesBlobRoundTrip) {
  std::string lib_path = std::string(TEST_DATA_DIR) + "/tcbn65lpbc.ski.lib";
  LibFile libfile(lib_path, "test_rt.log");
  libfile.parse();
  libfile.writeToDB(db_path_, "SS_1p08V_125C", 0.01);

  SQLite::Database db(db_path_, SQLite::OPEN_READWRITE);
  SQLite::Statement q(db, "SELECT rows_n, cols_n, values_blob FROM lut_entries LIMIT 1");
  ASSERT_TRUE(q.executeStep());

  int rows_n = q.getColumn(0).getInt();
  int cols_n = q.getColumn(1).getInt();
  const void *blob = q.getColumn(2).getBlob();
  int blob_bytes = q.getColumn(2).getBytes();

  EXPECT_EQ(blob_bytes, rows_n * cols_n * static_cast<int>(sizeof(float)));

  // All values should be positive and plausible (timing delays)
  const float *f = static_cast<const float *>(blob);
  for (int i = 0; i < rows_n * cols_n; ++i) {
    EXPECT_GT(f[i], 0.0f) << " at index " << i;
    EXPECT_LT(f[i], 1.0e6f) << " at index " << i;
  }
}

TEST_F(LibFileWriteToDBTest, IdempotentWrite) {
  std::string lib_path = std::string(TEST_DATA_DIR) + "/tcbn65lpbc.ski.lib";
  LibFile libfile(lib_path, "test_idem.log");
  libfile.parse();

  // Write twice
  libfile.writeToDB(db_path_, "SS_1p08V_125C", 0.01);
  libfile.writeToDB(db_path_, "SS_1p08V_125C", 0.01);

  // Verify no duplicate UNIQUE combinations
  SQLite::Database db(db_path_, SQLite::OPEN_READWRITE);
  SQLite::Statement distinct(db,
      "SELECT count(*) FROM ("
      "  SELECT DISTINCT file_path, pvt_corner, aged_year, arc_type, related_pin "
      "  FROM lut_entries)");
  ASSERT_TRUE(distinct.executeStep());
  int distinct_count = distinct.getColumn(0).getInt();

  SQLite::Statement total(db, "SELECT count(*) FROM lut_entries");
  ASSERT_TRUE(total.executeStep());
  int total_count = total.getColumn(0).getInt();

  EXPECT_EQ(distinct_count, total_count);
  EXPECT_GT(total_count, 0);
}