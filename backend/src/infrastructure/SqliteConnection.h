#pragma once

#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace campusfix
{
class SqliteStatement
{
public:
    explicit SqliteStatement(sqlite3_stmt *statement);
    ~SqliteStatement();
    SqliteStatement(const SqliteStatement &) = delete;
    SqliteStatement &operator=(const SqliteStatement &) = delete;
    SqliteStatement(SqliteStatement &&other) noexcept;
    SqliteStatement &operator=(SqliteStatement &&other) noexcept;

    void bindInt(int index, std::int64_t value);
    void bindText(int index, std::string_view value);
    void bindNull(int index);
    bool step();  // true = row, false = done
    std::int64_t columnInt(int index) const;
    std::string columnText(int index) const;
    bool columnIsNull(int index) const;

private:
    sqlite3_stmt *statement_;
};

class SqliteConnection
{
public:
    explicit SqliteConnection(const std::filesystem::path &path, bool create = false);
    ~SqliteConnection();
    SqliteConnection(const SqliteConnection &) = delete;
    SqliteConnection &operator=(const SqliteConnection &) = delete;

    void execute(std::string_view sql);
    SqliteStatement prepare(std::string_view sql);
    std::int64_t scalarInt(std::string_view sql);
    sqlite3 *nativeHandle() const noexcept { return database_; }

private:
    sqlite3 *database_ = nullptr;
};

class SqliteTransaction
{
public:
    explicit SqliteTransaction(SqliteConnection &connection, bool immediate = true);
    ~SqliteTransaction();
    SqliteTransaction(const SqliteTransaction &) = delete;
    SqliteTransaction &operator=(const SqliteTransaction &) = delete;
    void commit();

private:
    SqliteConnection &connection_;
    bool active_ = true;
};
}  // namespace campusfix
