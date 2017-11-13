#ifndef ECHOSERVER_FINANCE_DB_H
#define ECHOSERVER_FINANCE_DB_H

#include <SQLiteCpp/SQLiteCpp.h>
#include <sstream>
#include <iomanip>
#include <mutex>

#include "json/src/json.hpp"

struct FinanceUnit {
    std::string currency;
    float value;
    float inc_rel;
    float inc_abs;
    std::tm date;
};


class FinanceDb {
public:
    FinanceDb() : db_ptr(new SQLite::Database("finance.db", SQLite::OPEN_READWRITE)), db_mutex() {}

    virtual ~FinanceDb() = default;


    static void reset();

    int insert(FinanceUnit &financeUnit);

    int add_currency(std::string &currency);

    int add_currency_value(std::string &currency, double value);

    int del_currency(std::string &currency);

    int currency_history(std::string &currency, nlohmann::json& json);

    int currency_list(nlohmann::json& json);

private:
    SQLite::Database *db_ptr;
    std::mutex db_mutex;
};


#endif