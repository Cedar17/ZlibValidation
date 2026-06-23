# SQLite 入库去重分析：when 条件导致的静默数据丢失

## 背景

在真实大型 .lib（31 MB, 531 个单元）的端到端测试中，发现
`parse --db` 报告的写入计数与数据库实际条目数存在巨大差距：

```
Wrote 35560 LUT entries to DB    ← C++ 计数器的值
Actual: 7936 entries in DB       ← SELECT COUNT(*) 的真实值
差异: 27624 条 (77.7%) 被静默丢弃
```

## Liberty 文件层级结构（与「"重复"」的根源）

### AOI 单元的时序结构

以 AOI 单元（2-2-2-2 AND-OR-Invert）为例：

```
Y = NOT( (A1 & A2) | (B1 & B2) | (C1 & C2) | (D1 & D2) )
```

该单元有 8 个输入。输出 `Y` 对输入 `A1` 的延迟取决于 **其他 7 个输入的状态**（A2,B1,B2,C1,C2,D1,D2 分别取 0 或 1）。

Liberty 标准允许用 `when` 属性为每个输入组合定义独立的时序弧：

```lib
pin (Y) {
  timing () {
    related_pin : "A1";
    when : "(A2 * B1 * !B2 * C1 * !C2 * D1 * !D2)";  ← 条件 1
    timing_type : combinational;
    timing_sense : negative_unate;
    cell_rise (delay_template_7x7) {
      index_1 ("0.01, 0.0368, 0.1309, ...");
      index_2 ("0.001, 0.00155, 0.003482, ...");
      values ("0.713898, 0.767187, 0.952172, ...");    ← 延迟值 1
    }
    ...
  }
  timing () {
    related_pin : "A1";
    when : "(A2 * B1 * !B2 * C1 * !C2 * ~D1 * D2)";  ← 条件 2
    ...
    cell_rise (...) { values ("0.727307, ..."); }       ← 延迟值 2（不同！）
  }
  ... 共 28 组
}
```

同一 `related_pin=A1` 下有 **28 个不同的 `timing ()` 组**，
每组有不同的 `when` 条件，且 **LUT 值因输入条件不同而不同**。

## 去重链条（数据流分析）

### Step 1: si2dr 解析 → JSON

`json_utils.cpp:generateTimingJson()`（第 112 行）**正确提取了 `when`**：

```cpp
if (attr_name == "related_pin" || attr_name == "timing_type" ||
    attr_name == "timing_sense" || attr_name == "when") {
  timing_json[attr_name] = lib_attr.getString();
}
```

→ JSON 中的每个 `timing_arc` **保留了 `when` 字段**。

### Step 2: JSON → DB writeToDB

`LibFile.cpp:writeToDB()`（第 222-224 行）**没有提取 `when`**：

```cpp
std::string related_pin = arc.value("related_pin", "");
std::string timing_sense = arc.value("timing_sense", "");
std::string timing_type = arc.value("timing_type", "");
// 注意：when 没有被读取
```

### Step 3: UNIQUE 约束

`LibDatabase.cpp` DDL（第 46 行）：

```sql
UNIQUE(file_path, pvt_corner, aged_year, cell_name, output_pin,
       related_pin, arc_type)
```

**不包含 `when`。**

### 效果

对于某 AOI 单元的 28 个 timing 组：

| 组 | `related_pin` | `when` | `arc_type` | UNIQUE 判定 | 结果 |
|---|---|---|---|---|---|
| 1 | A1 | 条件1 | cell_rise | `(…,A1,cell_rise)` | ✅ 入库 |
| 2 | A1 | 条件2 | cell_rise | `(…,A1,cell_rise)` | ❌ **INSERT OR IGNORE** |
| 3 | A1 | 条件3 | cell_rise | `(…,A1,cell_rise)` | ❌ **INSERT OR IGNORE** |
| ... | ... | ... | ... | ... | ... |
| 28 | A1 | 条件28 | cell_rise | `(…,A1,cell_rise)` | ❌ **INSERT OR IGNORE** |

**28 组中只有第 1 组存留，27 组被静默丢弃。**

## 丢失规模

| 指标 | 原值 | 去重后 | 丢失 |
|---|---|---|---|
| total entries（全部 .lib） | 35,560 | 7,936 | 77.7% |
| AOI 单元 entries | 896 | 32 | 96.4% |

## 当前 UNIQUE 约束的正确性

当前约束（隐含阶段）：

```
(file_path, pvt_corner, aged_year, cell_name, output_pin, related_pin, arc_type)
```

| 维度 | 正确性 |
|---|---|
| 不同单元间隔离（跨 cell 丢失） | ✅ 已修复（包含 cell_name, output_pin） |
| 不同老化条件隔离 | ✅ 包含 aged_year, pvt_corner |
| **不同 when 条件下的不同 LUT** | **❌ 未包含 when → 数据丢失** |

## 修复思路

在 UNIQUE 约束和写入逻辑中加入 `when`：

```diff
- UNIQUE(file_path, pvt_corner, aged_year, cell_name, output_pin,
-        related_pin, arc_type)
+ UNIQUE(file_path, pvt_corner, aged_year, cell_name, output_pin,
+        related_pin, when, arc_type)
```

同步修改：
1. **`LibFile.cpp:writeToDB()`** — 从 arc 中提取 `when`
2. **`LibDatabase.cpp` DDL** — `when` 列加入 UNIQUE
3. **`LibDatabase.cpp` INSERT 语句** — `when` 加入 VALUES/列（已存在）
4. **测试** — 更新测试的 DISTINCT 查询列

### 注意事项

- `when` 表达式可能很长（如 `(A2 * B1 * !B2 * C1 * !C2 * D1 * !D2)`），
  作为 TEXT 列会增大索引。SQLite 允许的最大索引条目长度为 ~1KB，对典型 `when` 表达式足够
- 对于无 `when` 条件的单元（多数简单门），`when=""` 作为空字符串，仍保持唯一性
- 此修复不会影响已有数据库的兼容性（当前无正式部署的数据库）

## 验证方法

修复后重新运行：

```bash
./zlibvalidation parse \
  --db /tmp/test_biglib.db \
  --pvt SS_1p35V_125C \
  --aged-year 10 \
  "/path/to/large_lib.lib"

sqlite3 /tmp/test_biglib.db "SELECT COUNT(*) FROM lut_entries"
-- 预期: 35560（全部入库），而非 7936
```

---

撰写日期: 2026-06-23
作者: Code review 分析
关联文件: `src/LibFile.cpp` `src/LibDatabase.cpp` `src/json_utils.cpp` `test/test_libfile_db.cpp`

---

## 修复完成（2026-06-23，TDD 驱动）

已按上述思路修复并验证。实际实现的关键决策：

1. **`when` 列位置**：放在 DDL `timing_type` 与 `arc_type` 之间（非表末尾），与 `generateTimingJson` 的 JSON 解析顺序（related_pin→timing_type→timing_sense→when）对齐。`when` 是 SQL 保留字，DDL/查询中用 `"when"` 双引号转义。
2. **空 `when` 绑定空字符串 `""` 而非 NULL（关键决策）**：SQLite 的 UNIQUE 约束视多个 NULL 为"不冲突"，若绑 NULL，无条件弧（`when=""`）二次写入不会被 `INSERT OR IGNORE` 拦截 → 幂等破坏、数据翻倍。绑 `""` 让 UNIQUE 对无条件弧也生效。此问题由 TDD 的 `IdempotentWrite` 测试捕获。
3. **UNIQUE 列序**：`(file_path, pvt_corner, aged_year, cell_name, output_pin, related_pin, "when", arc_type)`。

### 验证结果（真实生产级 PDK，31MB / 531 cell / 6943 when）

| 指标 | 修复前 | 修复后 |
|---|---|---|
| DB 实际条目 | 7,936 | 35,536 |
| C++ 计数器 | 35,560 | 35,560 |
| 条件弧入库 | 仅每组首个 | 27,772 全保留 |
| distinct when | 极少 | 717 |
| 幂等（二次写入） | — | count 不变 |

### 35560 vs 35536 的 24 条差异（已追溯，非 bug）

JSON 分析确认：24 条全部是 `.lib` 中**某特定单元**对 `(output_pin, related_pin, when='')` 的重复定义。`json_utils` 如实解析两份，UNIQUE 正确去重一份，信息无丢失——符合 aging.db 文档 §10 的 `INSERT OR IGNORE` 幂等设计。

### 范围边界（与 aging.db 迁移计划 Phase 2 对齐）

- 仅 4 种时序 LUT（cell_rise / cell_fall / rise_transition / fall_transition）
- 不含 power、不含 constraint（fall/rise_constraint）、不扩展 cell/pin 标量属性、不新增表
- `library_name = basename_`（scenario_id 形式）、`scenario_id` NULL 钩子均保留（aging.db 设计意图，非 bug）

### TDD 测试

- `test/test_libdb.cpp`：新增 `DifferentWhenSameKeyBothPersisted`（纯 DB 层验证 when 进 UNIQUE）；4 处 `writeLutEntry` 调用补 `when` 实参
- `test/test_libfile_db.cpp`：新增 `WhenConditionPreserved`（ski.lib related_pin=A 有 4 distinct when）、`MultipleWhenArcsAllPersisted`（>=12 行）；`IdempotentWrite`/`ParseAndWriteSmallLib` 的 SELECT/DISTINCT 同步加 `when` 列
