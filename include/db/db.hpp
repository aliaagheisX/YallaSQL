#ifndef YALLASQL_INCLUDE_DB_HPP
#define YALLASQL_INCLUDE_DB_HPP

#include <stdexcept>
#include <string>
#include <mutex>
#include <vector>
#include <ranges>
#include <unordered_map>
#include "logger.hpp"

#include "duckdb/main/connection.hpp"
#include "duckdb/main/database.hpp"
using namespace duckdb;

#include "db/table.hpp"

const std::string DEFAULT_DATASET_PATH = "default_dataset";

class DB {

private:

    static DB* db_;
    std::string path_;
    quill::Logger* logger_;
    static std::mutex mutex_;
    std::unordered_map<std::string, Table*> tables_;

    std::unique_ptr<Connection> con_;
    std::unique_ptr<DuckDB> duckdb_; // In-memory DB

    // const char* disabled_optms = "SET disabled_optimizers = 'late_materialization,compressed_materialization,unused_columns,column_lifetime,statistics_propagation,filter_pushdown,join_order';";
    const char* disabled_optms = "SET disabled_optimizers = 'late_materialization,compressed_materialization,unused_columns,column_lifetime,statistics_propagation,filter_pushdown,common_aggregate';";
    // private singelton constructor
    DB(const std::string path): path_(path) {
        duckdb_ = std::make_unique<DuckDB>(nullptr);
        con_ = std::make_unique<Connection>(*duckdb_);
        // to not optimize too much
        con_->Query(disabled_optms);
        con_->Query("SET scalar_subquery_error_on_multiple_rows=false;");
        // con_->context->EnableProfiling();
        logger_ = YallaSQL::getLogger("./logs/database");
        refreshTables();
    }
    // reload tablesPaths_
    void refreshTables(bool insertInDuck = false);

    // do topoligical sort to recreate DuckDB links
    void reCreateLinkedDuckDB(const std::string &tableName, std::unordered_map<std::string, bool>& created, bool insertInDuck = false);
public:
    // db can't be cloned
    DB(const DB &) = delete;
    // db can't be assigned
    void operator=(const DB &) = delete;
    // set database path
    static void setPath(const std::string& path = DEFAULT_DATASET_PATH, bool insertInDuck = false);
    // control access to db instance
    static DB *getInstance();
    // get path of database
    std::string path() const { return path_; }
    // get connection to duckdb
    Connection& duckdb() const { return *con_; }
    // get table in db
    const Table* getTable(const std::string& tableName) const { 
        if(tables_.find(tableName) == tables_.end())
            throw std::runtime_error("YallaSQL can't find table: " + tableName);

        return tables_.find(tableName)->second; 
    }


    void dropAllDuckTables() {
        // This approach is faster for databases with many tables
        duckdb_ = std::make_unique<DuckDB>(nullptr);
        con_ = std::make_unique<Connection>(*duckdb_);
        // to not optimize too much
        con_->Query(disabled_optms);
        con_->Query("SET scalar_subquery_error_on_multiple_rows=false;");
    }
    ~DB();
};

#endif // YALLASQL_INCLUDE_DB_HPP