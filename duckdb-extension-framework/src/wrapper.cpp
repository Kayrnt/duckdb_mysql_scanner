#include "duckdb.hpp"
#include "wrapper.hpp"

#include <memory>

#include <string>

#include "duckdb.hpp"

namespace {

auto build_child_list(idx_t n_pairs, const char *const *names, duckdb_logical_type const *types) {
  duckdb::child_list_t<duckdb::LogicalType> members;
  for (idx_t i = 0; i < n_pairs; i++) {
    members.emplace_back(std::string(names[i]), *(duckdb::LogicalType *)types[i]);
  }
  return members;
}

}  // namespace

extern "C" {

// duckdb_logical_type duckdb_create_struct_type(idx_t n_pairs,
//                                               const char **names,
//                                               const duckdb_logical_type *types) {
//   auto *stype = new duckdb::LogicalType;
//   *stype = duckdb::LogicalType::STRUCT(build_child_list(n_pairs, names, types));
//   return reinterpret_cast<duckdb_logical_type>(stype);
// }

duckdb_logical_type duckdb_create_union(idx_t nmembers,
                                        const char **names,
                                        const duckdb_logical_type *types) {
    auto *utype = new duckdb::LogicalType;
    *utype = duckdb::LogicalType::UNION(build_child_list(nmembers, names, types));
    return reinterpret_cast<duckdb_logical_type>(utype);
}

}