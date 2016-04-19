#pragma once

#include "qsl_global.h"
#include "database.h"
#include <qslcolumn.h>
#include <qslnamespace.h>

#include <QString>

namespace qsl {
namespace qslc {
class Database;

class Column
{	
public:
	Column(const QByteArray &name, const QByteArray &type);
	
	QByteArray name() const { return _name; }
	QByteArray type() const { return _type; }
	uint32_t minsize() const { return _minsize; }
	QByteArray cppType() const { return _ctype; }
	uint8_t constraints() const { return _constraints; }
	
	void setConstraint(QSL::ColumnConstraint constraint) { _constraints |= constraint; }
	void setConstraint(const QByteArray &constraint);
	
private:
	/// The name of the field.
	QByteArray _name;
	/// The type of the field.
	QByteArray _type;
	/// The minimum required size of the type, or the maximum value if not specified.
	uint32_t _minsize = std::numeric_limits<uint32_t>::max();
	/// The type of the field as a C++ typename.
	QByteArray _ctype;
	/// The constraints for the field.
	uint8_t _constraints = QSL::none;
};

class Table
{
public:
	Table(Database *db, const QByteArray &name);
	
	Database* db() const { return _db; }
	QByteArray name() const { return _name; }
	QList<Column> fields() const { return _fields; }
	
	void addField(const Column &field);
	
private:
	/// The database containing this table.
	Database *_db;
	/// The name of the table.
	QByteArray _name;
	/// The primary key of the table.
	QByteArray _pk;
	/// All fields of the table.
	QList<Column> _fields;
};

}
}
