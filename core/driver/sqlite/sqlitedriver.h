#pragma once

#include "driver/driver.h"

namespace qsl {
namespace driver {


class SQLiteDriver : public Driver
{
public:
	// just to simplify coding
	typedef std::chrono::system_clock::time_point time_point;
	
	virtual Database *newDatabase() override;
	
	virtual QDate toQDate(const QVariant &date) override;
	virtual QTime toQTime(const QVariant &time) override;
	virtual QDateTime toQDateTime(const QVariant &datetime) override;
	virtual time_point toChronoDate(const QVariant &date) override;
	virtual time_point toChronoTime(const QVariant &time) override;
	virtual time_point toChronoDateTime(const QVariant &datetime) override;
	virtual QVariant fromQDate(const QDate &date) override;
	virtual QVariant fromQTime(const QTime &time) override;
	virtual QVariant fromQDateTime(const QDateTime &datetime) override;
	virtual QVariant fromChronoDate(const time_point &date) override;
	virtual QVariant fromChronoTime(const time_point &time) override;
	virtual QVariant fromChronoDateTime(const time_point &datetime) override;
	
	virtual QString sql_select(QSLTable *tbl, const QSharedPointer<QSLFilter> &filter, uint limit);
};

}
}
