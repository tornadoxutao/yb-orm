// -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
#define YBORM_SOURCE

#include <algorithm>
#include "dialect_interbase.h"
#include "util/string_utils.h"
#include "orm/expression.h"

namespace Yb {

using namespace std;
using namespace Yb::StrUtils;

InterbaseDialect::InterbaseDialect()
    : SqlDialect(_T("INTERBASE"), _T("RDB$DATABASE"), true)
{}

const String
InterbaseDialect::select_curr_value(const String &seq_name)
{
    return _T("GEN_ID(") + seq_name + _T(", 0)");
}

const String
InterbaseDialect::select_next_value(const String &seq_name)
{
    return _T("GEN_ID(") + seq_name + _T(", 1)");
}

const String
InterbaseDialect::sql_value(const Value &x)
{
    return x.sql_str();
}

bool
InterbaseDialect::commit_ddl()
{
    return true;
}

const String
InterbaseDialect::type2sql(int t)
{
    switch (t) {
        case Value::INTEGER:    return _T("INTEGER");       break;
        case Value::LONGINT:    return _T("BIGINT");        break;
        case Value::STRING:     return _T("VARCHAR");       break;
        case Value::DECIMAL:    return _T("DECIMAL(16, 6)"); break;
        case Value::DATETIME:   return _T("TIMESTAMP");     break;
        case Value::FLOAT:      return _T("DOUBLE PRECISION"); break;
    }
    throw SqlDialectError(_T("Bad type"));
}

const String InterbaseDialect::create_sequence(const String &seq_name)
{
    return _T("CREATE GENERATOR ") + seq_name;
}

const String
InterbaseDialect::drop_sequence(const String &seq_name)
{
    return _T("DROP GENERATOR ") + seq_name;
}

int
InterbaseDialect::pager_model()
{
    return (int)PAGER_INTERBASE;
}

// schema introspection
bool
InterbaseDialect::table_exists(SqlConnection &conn, const String &table)
{
    Strings t = get_tables(conn);
    for (Strings::iterator i = t.begin(); i != t.end(); ++i)
    {
        if (*i == table)
        {
            return true;
        }
    }
    return false;
}

bool
InterbaseDialect::view_exists(SqlConnection &conn, const String &table)
{
    return false;
}

Strings
InterbaseDialect::get_tables(SqlConnection &conn)
{
    Strings tables;
    auto_ptr<SqlCursor> cursor = conn.new_cursor();
    String query = _T("SELECT R.RDB$RELATION_NAME FROM RDB$RELATIONS R")
                   _T(" WHERE R.RDB$SYSTEM_FLAG = 0 AND R.RDB$VIEW_SOURCE IS NULL");
    cursor->prepare(query);
    Values params;
    SqlResultSet rs = cursor->exec(params);
    for (SqlResultSet::iterator i = rs.begin(); i != rs.end(); ++i) {
        tables.push_back((*i)[0].second.as_string());
    }
    //cout << tables[0];
    return tables;
}

Strings
InterbaseDialect::get_views(SqlConnection &conn)
{
    return Strings();
}

ColumnsInfo
InterbaseDialect::get_columns(SqlConnection &conn, const String &table)
{
    ColumnsInfo col_mass;
    auto_ptr<SqlCursor> cursor = conn.new_cursor();

    String query =
        _T("SELECT RF.RDB$FIELD_NAME NAME, T1.RDB$TYPE_NAME TYPE_NAME,")
        _T(" RF.RDB$DEFAULT_SOURCE DFLT, F.RDB$FIELD_LENGTH SZ,")
        _T(" F.RDB$FIELD_PRECISION PREC, F.RDB$FIELD_SCALE SCALE,")
        _T(" RF.RDB$NULL_FLAG NOT_NULL")
        _T(" FROM RDB$RELATIONS R")
        _T(" JOIN RDB$RELATION_FIELDS RF")
        _T(" ON R.RDB$RELATION_NAME = RF.RDB$RELATION_NAME")
        _T(" JOIN RDB$FIELDS F ON RF.RDB$FIELD_SOURCE = F.RDB$FIELD_NAME")
        _T(" JOIN RDB$TYPES T1 ON F.RDB$FIELD_TYPE = T1.RDB$TYPE")
        _T(" AND T1.RDB$FIELD_NAME = 'RDB$FIELD_TYPE'")
        _T(" WHERE R.RDB$RELATION_NAME='") + table + _T("'")
        _T(" ORDER BY RF.RDB$FIELD_POSITION");
    cursor->prepare(query);
    Values params;
    SqlResultSet rs = cursor->exec(params);
    for (SqlResultSet::iterator i = rs.begin(); i != rs.end(); ++i)
    {
        ColumnInfo x;
        int prec = 0, scale = 0;
        for (Row::const_iterator j = i->begin(); j != i->end(); ++j)
        {
            if (_T("NAME") == j->first)
            {
                x.name = str_to_upper(trim_trailing_space(j->second.as_string()));
                //cout << x.name << endl;
            }
            else if (_T("TYPE_NAME") == j->first)
            {
                x.type = str_to_upper(trim_trailing_space(j->second.as_string()));
                //cout << x.type << endl;
                if (_T("VARYING") == x.type)
                    x.type = _T("VARCHAR");
                else if (_T("LONG") == x.type)
                    x.type = _T("INTEGER");
                else if (_T("INT64") == x.type)
                    x.type = _T("BIGINT");
                else if (_T("DOUBLE") == x.type)
                    x.type = _T("DOUBLE PRECISION");
            }
            else if (_T("DFLT") == j->first)
            {
                if (!j->second.is_null()) {
                    x.default_value = trim_trailing_space(j->second.as_string());
                    if (starts_with(x.default_value, _T("DEFAULT ")))
                        x.default_value = str_substr(x.default_value,
                                str_length(_T("DEFAULT ")));
                }
                //cout <<  x.default_value << endl;
            }
            else if (_T("SZ") == j->first)
            {
                if (!j->second.is_null() && _T("VARCHAR") == x.type)
                    x.size = j->second.as_integer();
            }
            else if (_T("PREC") == j->first)
            {
                if (!j->second.is_null())
                    prec = j->second.as_integer();
            }
            else if (_T("SCALE") == j->first)
            {
                if (!j->second.is_null())
                    scale = -j->second.as_integer();
            }
            else if (_T("NOT_NULL") == j->first)
            {
                if (j->second.is_null())
                    x.notnull = false;
                else
                    x.notnull = j->second.as_integer() != 0;
            }
        }
        if (_T("BIGINT") == x.type && prec && scale) {
            x.type = _T("DECIMAL(") + to_string(prec) + _T(", ")
                + to_string(scale) + _T(")");
        }
        col_mass.push_back(x);
    }
    return col_mass;
}

} // namespace Yb

// vim:ts=4:sts=4:sw=4:et:
