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

int FinanceDb::insert(FinanceUnit &financeUnit) {
    std::stringstream sql_builder;
    sql_builder << "INSERT INTO finance VALUES ("
                << "NULL" << ","
                << "\"" << financeUnit.currency << "\","
                << financeUnit.value << ","
                << financeUnit.inc_rel << ","
                << financeUnit.inc_abs << ","
                << "\"" << std::put_time(&financeUnit.date, "%Y-%b-%d %H:%M:%S") << "\""
                << ")";
    auto &&sql = sql_builder.str();
    try {
        std::unique_lock<std::mutex> lock(db_mutex);
        SQLite::Transaction transaction(*db_ptr);
        db_ptr->exec(sql);
        transaction.commit();
        lock.unlock();
    } catch (std::exception &ex) {
        Logger::logger_inst->error("DB insert exception: {}", ex.what());
        return -1;
    }
    return 0;
}

int FinanceDb::add_currency(std::string &currency) {
    try {
        SQLite::Statement query(*db_ptr, "SELECT count(*) FROM finance WHERE currency = ?");
        query.bind(1, currency);
        query.executeStep();
        int count = query.getColumn(0);
        if (count != 0) return 1;
        auto &&_time = time(nullptr);
        auto &&timev = std::localtime(&_time);
        std::stringstream sql_builder;
        sql_builder << "INSERT INTO finance VALUES ("
                    << "NULL" << ","
                    << "\"" << currency << "\","
                    << "NULL" << ","
                    << "NULL" << ","
                    << "NULL" << ","
                    << "\"" << std::put_time(timev, "%Y-%b-%d %H:%M:%S") << "\""
                    << ")";
        auto &&sql = sql_builder.str();
        Logger::logger_inst->info(sql);
        std::unique_lock<std::mutex> lock(db_mutex);
        SQLite::Transaction transaction(*db_ptr);
        db_ptr->exec(sql);
        transaction.commit();
        lock.unlock();

    } catch (std::exception &ex) {
        Logger::logger_inst->error("DB select exception: {}", ex.what());
        return -1;
    }
    return 0;
}

int FinanceDb::add_currency_value(std::string &currency, double value) {
    try {
        SQLite::Statement query(*db_ptr, "SELECT id, value, "
                "CASE WHEN value IS NULL THEN 1 ELSE 0 END"
                " FROM finance WHERE currency = ? ORDER BY date DESC");
        query.bind(1, currency);
        Logger::logger_inst->info(query.getQuery());
        auto &&status = query.executeStep();
        if (!status) return 1;
        int id = query.getColumn(0);
        double cur_value = query.getColumn(1);
        int is_new = query.getColumn(2);
        double relative = 0, absolute = 0;
        if (!is_new) {
            absolute = value - cur_value;
            relative = absolute / cur_value;
        }
        auto &&_time = time(nullptr);
        auto &&timev = std::localtime(&_time);
        std::stringstream sql_builder;
        sql_builder << "(";
        if (!is_new) {
            sql_builder << "NULL,"
                        << "\"" << currency << "\",";
        }
        sql_builder << value << ","
                    << relative << ","
                    << absolute << ","
                    << "\"" << std::put_time(timev, "%Y-%b-%d %H:%M:%S") << "\""
                    << ")";
        auto &&sql_value = sql_builder.str();
        sql_builder.str("");
        if (is_new) {
            sql_builder << "UPDATE finance SET"
                        << "(value, inc_rel, inc_abs, date) = "
                        << sql_value
                        << "WHERE id = "
                        << id;
        } else {
            sql_builder << "INSERT INTO finance VALUES" << sql_value;
        }
        auto &&sql = sql_builder.str();
        Logger::logger_inst->info(sql);
        std::unique_lock<std::mutex> lock(db_mutex);
        SQLite::Transaction transaction(*db_ptr);
        db_ptr->exec(sql);
        transaction.commit();
        lock.unlock();
    } catch (std::exception &ex) {
        Logger::logger_inst->error("DB select exception: {}", ex.what());
        return -1;
    }
    return 0;
}

int FinanceDb::del_currency(std::string &currency) {
    try {
        std::unique_lock<std::mutex> lock(db_mutex);
        SQLite::Statement query(*db_ptr, "DELETE FROM finance WHERE currency = ?");
        query.bind(1, currency);
        Logger::logger_inst->info(query.getQuery());
        SQLite::Transaction transaction(*db_ptr);
        auto &&count = query.exec();
        transaction.commit();
        lock.unlock();
        if (count == 0) return 1;
    }
    catch (std::exception &ex) {
        Logger::logger_inst->error("DB select exception: {}", ex.what());
        return -1;
    }
    return 0;
}

int FinanceDb::currency_list(nlohmann::json &json) {
    try {
        SQLite::Statement query(*db_ptr, "SELECT currency, value, inc_rel, inc_abs, date FROM finance");
        Logger::logger_inst->info(query.getQuery());
        while (query.executeStep()) {
            std::string currency = query.getColumn(0);
            double value = query.getColumn(1);
            double inc_rel = query.getColumn(2);
            double inc_abs = query.getColumn(3);
            std::string date = query.getColumn(4);
            nlohmann::json item = {
                    {"currency",          currency},
                    {"value",             value},
                    {"relative_increase", inc_rel},
                    {"absolute_increase", inc_abs},
                    {"date",              date},
            };
            json.push_back(item);
        }
    }
    catch (std::exception &ex) {
        Logger::logger_inst->error("DB select exception: {}", ex.what());
        return -1;
    }
    return 0;
}

int FinanceDb::currency_history(std::string &curr, nlohmann::json &json) {
    try {
        SQLite::Statement query(*db_ptr, "SELECT value, date FROM finance WHERE currency = ?");
        query.bind(1, curr);
        Logger::logger_inst->info(query.getQuery());
        nlohmann::json result;
        while (query.executeStep()) {
            double value = query.getColumn(0);
            std::string date = query.getColumn(1);
            nlohmann::json item = {
                    {"value", value},
                    {"date",  date},
            };
            result.push_back(item);
        }
        if (result.empty()) return 1;
        json["currency"] = curr;
        json["history"] = result;
    }
    catch (std::exception &ex) {
        Logger::logger_inst->error("DB select exception: {}", ex.what());
        return -1;
    }
    return 0;
}



