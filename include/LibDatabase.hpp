#ifndef LIBDATABASE_HPP
#define LIBDATABASE_HPP

#include <memory>
#include <string>
#include <vector>

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>

/**
 * @brief Manages SQLite database connections for writing Liberty LUT data.
 *
 * LibDatabase opens or creates a SQLite database file and provides methods
 * to write timing look-up table (LUT) entries into the `lut_entries` table.
 *
 * Responsibilities:
 * - Create the `lut_entries` table (DDL) on first use via initialize()
 * - Write one LUT record per call to writeLutEntry()
 * - Enforce idempotency via INSERT OR IGNORE
 *
 * This class is deliberately scoped to Phase 2 of the aging.db migration:
 * it only manages the lut_entries table. Other tables (scenarios, k_values,
 * etc.) are managed by Python build scripts.
 */
class LibDatabase {
public:
  /**
   * @brief Open (or create) a SQLite database at the given path.
   * @param db_path  Filesystem path to the .db file.
   */
  explicit LibDatabase(const std::string &db_path);
  ~LibDatabase();

  // -----------------------------------------------------------------------
  // Schema / lifecycle
  // -----------------------------------------------------------------------

  /**
   * @brief Create the lut_entries table if it does not already exist.
   *
   * Safe to call multiple times — uses CREATE TABLE IF NOT EXISTS.
   * Sets WAL journal mode and busy_timeout.
   */
  void initialize();

  // -----------------------------------------------------------------------
  // Transaction support (for batched writes)
  // -----------------------------------------------------------------------

  void beginTransaction();
  void commitTransaction();

  // -----------------------------------------------------------------------
  // Write operations
  // -----------------------------------------------------------------------

  /**
   * @brief Insert one LUT record into lut_entries.
   *
   * Uses INSERT OR IGNORE so repeated calls with the same UNIQUE key
   * (pvt_corner, age_seconds, cell_name, output_pin, related_pin, "when", arc_type)
   * are no-ops. `file_path` is deliberately excluded from the UNIQUE constraint
   * because the same .lib data may exist under multiple paths (symlinks, directory
   * mirrors); it is stored as a provenance column but does not participate in
   * semantic deduplication.
   *
   * @param file_path     Source .lib file path (for provenance only — not in UNIQUE).
   * @param library_name  Library name (basename of .lib file).
   * @param pvt_corner    PVT corner string (e.g. "SS_1p08V_125C").
   * @param age_seconds   Aging time in seconds (e.g. 36.0 for 0.01h fresh, 315360000.0 for 10y).
   * @param cell_name     Cell name extracted from .lib.
   * @param output_pin    Output pin name (e.g. "Y").
   * @param related_pin   Input related pin name (e.g. "A").
   * @param timing_sense  Timing sense (e.g. "negative_unate").
   * @param timing_type   Timing type (e.g. "combinational", "rising_edge").
   * @param when           Boolean condition expression distinguishing arcs that
   *                       share the same related_pin (e.g. "A * !B"). Empty
   *                       string for unconditional arcs — stored as NULL.
   * @param arc_type      Arc type: cell_rise, cell_fall, rise_transition, fall_transition.
   * @param rows_n        Number of rows in the LUT.
   * @param cols_n        Number of columns in the LUT.
   * @param index_1       Row index values (size == rows_n).
   * @param index_2       Column index values (size == cols_n).
   * @param values        Flattened LUT values (size == rows_n × cols_n, row-major).
   * @param process       Process number from .lib header (raw integer, e.g. 1).
   *                       Not mapped to SS/FF/TT — that is the caller's responsibility
   *                       via the --pvt CLI argument.
   * @param temperature   Temperature from .lib header.
   * @param voltage       Voltage from .lib header.
   */
  void writeLutEntry(const std::string &file_path, const std::string &library_name,
                     const std::string &pvt_corner, double age_seconds,
                     const std::string &cell_name, const std::string &output_pin,
                     const std::string &related_pin, const std::string &timing_sense,
                     const std::string &timing_type, const std::string &when,
                     const std::string &arc_type, int rows_n, int cols_n,
                     const std::vector<double> &index_1,
                     const std::vector<double> &index_2, const std::vector<double> &values,
                     int process, double temperature, double voltage);

private:
  std::string db_path_;
  std::unique_ptr<SQLite::Database> db_;

  // Prepared statements cached across calls
  std::unique_ptr<SQLite::Statement> insert_stmt_;
  bool initialized_ = false;
};

#endif // LIBDATABASE_HPP