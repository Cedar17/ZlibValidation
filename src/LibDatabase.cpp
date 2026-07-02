#include "LibDatabase.hpp"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <cstring>
#include <vector>

// =========================================================================
// Helpers: convert vector<double> → blob (vector<float>)
// =========================================================================

static std::vector<float> toFloatBlob(const std::vector<double> &src) {
  std::vector<float> out;
  out.reserve(src.size());
  for (double v : src) out.push_back(static_cast<float>(v));
  return out;
}

// =========================================================================
// DDL — only the lut_entries table (the rest is managed by Python)
// NOTE: file_path is deliberately excluded from the UNIQUE constraint.
// The same .lib data may exist under multiple paths (symlinks, directory mirrors),
// so UNIQUE uses semantic keys only. file_path remains as a provenance column.
// =========================================================================

static const char *DDL_LUT_ENTRIES = R"sql(
  CREATE TABLE IF NOT EXISTS lut_entries (
    entry_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    file_path     TEXT NOT NULL,
    library_name  TEXT NOT NULL,
    scenario_id   TEXT,
    pvt_corner    TEXT NOT NULL,
    aged_year     REAL NOT NULL,
    cell_name     TEXT NOT NULL,
    output_pin    TEXT NOT NULL DEFAULT 'Y',
    related_pin   TEXT NOT NULL,
    timing_sense  TEXT,
    timing_type   TEXT,
    "when"        TEXT,
    arc_type      TEXT NOT NULL,
    rows_n        INTEGER NOT NULL,
    cols_n        INTEGER NOT NULL,
    index_1_blob  BLOB NOT NULL,
    index_2_blob  BLOB NOT NULL,
    values_blob   BLOB NOT NULL,
    process       INTEGER,
    temperature   REAL,
    voltage       REAL,
    ingested_at   TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(pvt_corner, aged_year, cell_name, output_pin,
           related_pin, "when", arc_type)
  );
)sql";

static const char *INSERT_SQL = R"sql(
  INSERT OR IGNORE INTO lut_entries
    (file_path, library_name, scenario_id, pvt_corner, aged_year,
     cell_name, output_pin, related_pin, timing_sense, timing_type, "when", arc_type,
     rows_n, cols_n, index_1_blob, index_2_blob, values_blob,
     process, temperature, voltage)
  VALUES (?, ?, ?, ?, ?,
          ?, ?, ?, ?, ?, ?, ?,
          ?, ?, ?, ?, ?,
          ?, ?, ?)
)sql";

// =========================================================================
// Constructor / Destructor
// =========================================================================

LibDatabase::LibDatabase(const std::string &db_path) : db_path_(db_path) {}

LibDatabase::~LibDatabase() = default;

// =========================================================================
// initialize()
// =========================================================================

void LibDatabase::initialize() {
  if (initialized_) return;

  db_ = std::make_unique<SQLite::Database>(
      db_path_,
      SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

  // WAL mode + busy timeout for concurrent access
  db_->exec("PRAGMA journal_mode = WAL");
  db_->exec("PRAGMA busy_timeout = 30000");

  // Create table
  db_->exec(DDL_LUT_ENTRIES);

  // Cache the INSERT statement
  insert_stmt_ = std::make_unique<SQLite::Statement>(*db_, INSERT_SQL);

  initialized_ = true;
}

// =========================================================================
// Transaction support
// =========================================================================

void LibDatabase::beginTransaction() {
  if (!initialized_) {
    throw std::runtime_error("LibDatabase::beginTransaction() called before initialize()");
  }
  db_->exec("BEGIN");
}

void LibDatabase::commitTransaction() {
  if (!initialized_) {
    throw std::runtime_error("LibDatabase::commitTransaction() called before initialize()");
  }
  db_->exec("COMMIT");
}

// =========================================================================
// writeLutEntry()
// =========================================================================

void LibDatabase::writeLutEntry(const std::string &file_path,
                                const std::string &library_name,
                                const std::string &pvt_corner, double aged_year,
                                const std::string &cell_name, const std::string &output_pin,
                                const std::string &related_pin, const std::string &timing_sense,
                                const std::string &timing_type, const std::string &when,
                                const std::string &arc_type, int rows_n, int cols_n,
                                const std::vector<double> &index_1,
                                const std::vector<double> &index_2,
                                const std::vector<double> &values, int process,
                                double temperature, double voltage) {
  if (!initialized_) {
    throw std::runtime_error("LibDatabase::writeLutEntry() called before initialize()");
  }

  auto &stmt = *insert_stmt_;

  // Reset before binding — safe even after a prior exec() failure
  stmt.reset();

  // Convert doubles to float32 blobs
  auto idx1_f = toFloatBlob(index_1);
  auto idx2_f = toFloatBlob(index_2);
  auto vals_f = toFloatBlob(values);

  // Empty vectors would bind NULL to BLOB NOT NULL columns — refuse early
  if (idx1_f.empty()) {
    throw std::runtime_error("Empty index_1 in LUT entry for " + cell_name + "/" + arc_type);
  }
  if (vals_f.empty()) {
    throw std::runtime_error("Empty values in LUT entry for " + cell_name + "/" + arc_type);
  }
  // index_2 may be zero-filled for 1D LUTs — accept empty as 1-column placeholder
  if (idx2_f.empty()) {
    // BIND NULL is rejected by NOT NULL, so write a single zero
    idx2_f.push_back(0.0f);
  }

  // Bind parameters
  stmt.bind(1, file_path);
  stmt.bind(2, library_name);
  stmt.bind(3); // scenario_id → NULL (Python fills later)
  stmt.bind(4, pvt_corner);
  stmt.bind(5, aged_year);
  stmt.bind(6, cell_name);
  stmt.bind(7, output_pin);
  stmt.bind(8, related_pin);
  stmt.bind(9, timing_sense);
  stmt.bind(10, timing_type);
  // Empty when → "" (NOT NULL string), so unconditional arcs still honor the
  // UNIQUE constraint for idempotent re-writes. Binding NULL would let SQLite
  // treat multiple NULLs as non-conflicting and break INSERT OR IGNORE dedup.
  stmt.bind(11, when);
  stmt.bind(12, arc_type);
  stmt.bind(13, rows_n);
  stmt.bind(14, cols_n);
  stmt.bind(15, idx1_f.data(), static_cast<int>(idx1_f.size() * sizeof(float)));
  stmt.bind(16, idx2_f.data(), static_cast<int>(idx2_f.size() * sizeof(float)));
  stmt.bind(17, vals_f.data(), static_cast<int>(vals_f.size() * sizeof(float)));
  stmt.bind(18, process);
  stmt.bind(19, temperature);
  stmt.bind(20, voltage);

  // Execute
  stmt.exec();
}
