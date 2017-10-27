#ifndef ECHOSERVER_FINANCE_DB_H
#define ECHOSERVER_FINANCE_DB_H

#include <SQLiteCpp/SQLiteCpp.h>
#include <sstream>
#include <iomanip>

struct FinanceUnit {
    std::string currency;
    float value;
    float inc_rel;
    float inc_abs;
    std::tm date;
};


class FinanceDb {
public:
    static void reset();

    void insert(FinanceUnit &financeUnit);

    FinanceDb() : db_ptr(new SQLite::Database("finance.db", SQLite::OPEN_READWRITE)) {}

    virtual ~FinanceDb() = default;

private:
    SQLite::Database *db_ptr;
};


#endif