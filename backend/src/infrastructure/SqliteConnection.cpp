#include "infrastructure/SqliteConnection.h"

#include <limits>
#include <stdexcept>
#include <string>

namespace campusfix
{
namespace
{
void checkBind(sqlite3_stmt *statement, int result)
{
    if (result != SQLITE_OK)
    {
        throw std::runtime_error("SQLite bind failed: " +
                                 std::string(sqlite3_errmsg(sqlite3_db_handle(statement))));
    }
}
}  // namespace

SqliteStatement::SqliteStatement(sqlite3_stmt *statement) : statement_(statement) {}

SqliteStatement::~SqliteStatement()
{
    sqlite3_finalize(statement_);
}

SqliteStatement::SqliteStatement(SqliteStatement &&other) noexcept : statement_(other.statement_)
{
    other.statement_ = nullptr;
}

SqliteStatement &SqliteStatement::operator=(SqliteStatement &&other) noexcept
{
    if (this != &other)
    {
        sqlite3_finalize(statement_);
        statement_ = other.statement_;
        other.statement_ = nullptr;
    }
    return *this;
}

void SqliteStatement::bindInt(int index, std::int64_t value)
{
    checkBind(statement_, sqlite3_bind_int64(statement_, index, value));
}

void SqliteStatement::bindText(int index, std::string_view value)
{
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw std::runtime_error("SQLite text parameter is too large");
    }
    checkBind(statement_, sqlite3_bind_text(statement_, index, value.data(),
                                           static_cast<int>(value.size()), SQLITE_TRANSIENT));
}

void SqliteStatement::bindNull(int index)
{
    checkBind(statement_, sqlite3_bind_null(statement_, index));
}

bool SqliteStatement::step()
{
    const int result = sqlite3_step(statement_);
    if (result == SQLITE_ROW)
    {
        return true;
    }
    if (result == SQLITE_DONE)
    {
        return false;
    }
    throw SqliteError(result,"SQLite statement failed: " +
                             std::string(sqlite3_errmsg(sqlite3_db_handle(statement_))));
}

std::int64_t SqliteStatement::columnInt(int index) const
{
    return sqlite3_column_int64(statement_, index);
}

std::string SqliteStatement::columnText(int index) const
{
    const auto *value = sqlite3_column_text(statement_, index);
    return value == nullptr ? std::string{} :
                              std::string(reinterpret_cast<const char *>(value));
}

bool SqliteStatement::columnIsNull(int index) const
{
    return sqlite3_column_type(statement_, index) == SQLITE_NULL;
}

SqliteConnection::SqliteConnection(const std::filesystem::path &path, bool create)
{
    if (sqlite3_libversion_number() < 3035000)
    {
        throw std::runtime_error("SQLite 3.35 or newer is required");
    }
    const std::string utf8Path = path.u8string();
    const int flags = SQLITE_OPEN_READWRITE | (create ? SQLITE_OPEN_CREATE : 0);
    const int result = sqlite3_open_v2(utf8Path.c_str(), &database_, flags, nullptr);
    if (result != SQLITE_OK)
    {
        const std::string message = database_ != nullptr ? sqlite3_errmsg(database_) : "unknown error";
        sqlite3_close(database_);
        database_ = nullptr;
        throw std::runtime_error("Cannot open SQLite database: " + message);
    }
    try
    {
        sqlite3_extended_result_codes(database_, 1);
        if (sqlite3_busy_timeout(database_, 5000) != SQLITE_OK)
        {
            throw std::runtime_error("Cannot configure SQLite busy_timeout");
        }
        // These settings are connection-local and must precede BEGIN.
        execute("PRAGMA foreign_keys=ON");
        if (scalarInt("PRAGMA foreign_keys") != 1)
        {
            throw std::runtime_error("SQLite foreign key enforcement is unavailable");
        }
    }
    catch (...)
    {
        sqlite3_close(database_);
        database_ = nullptr;
        throw;
    }
}

SqliteConnection::~SqliteConnection()
{
    sqlite3_close_v2(database_);
}

void SqliteConnection::execute(std::string_view sql)
{
    char *error = nullptr;
    const std::string ownedSql(sql);
    const int result=sqlite3_exec(database_, ownedSql.c_str(), nullptr, nullptr, &error);
    if (result != SQLITE_OK)
    {
        const std::string message = error != nullptr ? error : sqlite3_errmsg(database_);
        sqlite3_free(error);
        throw SqliteError(result,"SQLite execution failed: " + message);
    }
}

SqliteStatement SqliteConnection::prepare(std::string_view sql)
{
    sqlite3_stmt *statement = nullptr;
    const int result = sqlite3_prepare_v2(database_, sql.data(), static_cast<int>(sql.size()),
                                          &statement, nullptr);
    if (result != SQLITE_OK)
    {
        throw SqliteError(result,"SQLite prepare failed: " + std::string(sqlite3_errmsg(database_)));
    }
    return SqliteStatement(statement);
}

std::int64_t SqliteConnection::scalarInt(std::string_view sql)
{
    auto statement = prepare(sql);
    if (!statement.step())
    {
        throw std::runtime_error("SQLite scalar query returned no row");
    }
    return statement.columnInt(0);
}

SqliteTransaction::SqliteTransaction(SqliteConnection &connection, bool immediate)
    : connection_(connection)
{
    connection_.execute(immediate ? "BEGIN IMMEDIATE" : "BEGIN");
}

SqliteTransaction::~SqliteTransaction()
{
    if (active_)
    {
        try
        {
            connection_.execute("ROLLBACK");
        }
        catch (...)
        {
            // A transaction may already have been rolled back by SQLite.
        }
    }
}

void SqliteTransaction::commit()
{
    connection_.execute("COMMIT");
    active_ = false;
}
}  // namespace campusfix
