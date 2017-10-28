#include "FinanceDb.h"
#include "logging/logger.h"

std::tm parse_date(const std::string &date_str) {
    std::tm datetime = {};
    std::istringstream ss(date_str);
    ss >> std::get_time(&datetime, "%Y-%b-%d %H:%M:%S");
    return datetime;
}

void FinanceDb::reset() {
    try {
        Logger::logger_inst->info("Resetting database");
        SQLite::Database db("finance.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        db.exec("DROP TABLE IF EXISTS finance");
        SQLite::Transaction transaction(db);
        db.exec("CREATE TABLE finance ("
                        " id INTEGER PRIMARY KEY,"
                        " currency TEXT,"
                        " value REAL,"
                        " inc_rel REAL,"
                        " inc_abs REAL,"
                        " date TEXT"
                        ")");
        transaction.commit();
    } catch (std::exception &ex) {
        Logger::logger_inst->error("DB exception: {}", ex.what());
    }
}

void FinanceDb::insert(FinanceUnit &financeUnit) {
    std::stringstream sql;
    sql << "INSERT INTO finance ("
        << "\"" << financeUnit.currency << "\","
        << financeUnit.value << ","
        << financeUnit.inc_rel << ","
        << financeUnit.inc_abs << ","
        << std::put_time(&financeUnit.date, "%Y-%b-%d %H:%M:%S")
        << ")";
    try {
        SQLite::Transaction transaction(*db_ptr);
        db_ptr->exec(sql.str());
        transaction.commit();
    } catch (std::exception &ex) {
        Logger::logger_inst->error("DB insert exception: {}", ex.what());
    }
}
