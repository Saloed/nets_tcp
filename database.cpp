#include <SQLiteCpp/SQLiteCpp.h>
#include <sstream>
#include <iomanip>

#include "logger.h"


struct FinanceUnit {
    std::string currency;
    float value;
    float inc_rel;
    float inc_abs;
    std::tm date;
};


class Finance {
public:
    static void reset() {
        try {
            Logger::logger->info("Resetting database");
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
            Logger::logger->error("DB exception: {}", ex.what());
        }
    }

    static std::tm parse_date(const std::string &date_str) {
        std::tm datetime = {};
        std::istringstream ss(date_str);
        ss >> std::get_time(&datetime, "%Y-%b-%d %H:%M:%S");
        return datetime;
    }

    Finance() : db_ptr(new SQLite::Database("finance.db", SQLite::OPEN_READWRITE)) {}

    virtual ~Finance() = default;

    void insert(FinanceUnit &financeUnit) {
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
            Logger::logger->error("DB insert exception: {}", ex.what());
        }
    }


private:
    SQLite::Database *db_ptr;
};

int main() {
    Finance::reset();
    return 0;
}