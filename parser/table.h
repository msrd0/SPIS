#pragma once

#include "spis_global.h"
#include "database.h"
#include <spiscolumn.h>
#include <spisnamespace.h>

#include <QString>

namespace spis {
namespace spisc {
class Database;

class Column
{	
public:
	Column(const QByteArray &name, const QByteArray &type, bool qtype = false, QVariant def = QVariant());
	
	QByteArray name() const { return _name; }
	QByteArray type() const { return _type; }
	int minsize() const { return _minsize; }
	QByteArray cppType() const { return _ctype; }
	bool cppReference() const { return _cref; }
	QByteArray cppArgType() const { return (_cref ? "const " + cppType() + "&" : cppType()); }
	uint8_t constraints() const { return _constraints; }
	QVariant def() const { return _def; }
	
	void setConstraint(SPIS::ColumnConstraint constraint) { _constraints |= constraint; }
	int setConstraint(const QByteArray &constraint);
	
private:
	/// The name of the field.
	QByteArray _name;
	/// The type of the field.
	QByteArray _type;
	/// The minimum required size of the type, or the maximum value if not specified.
	int _minsize = -1;
	/// The type of the field as a C++ typename.
	QByteArray _ctype;
	/// Whether the type should be passed via const reference.
	bool _cref;
	/// The constraints for the field.
	uint8_t _constraints = SPIS::none;
	/// The default value of this field.
	QVariant _def;
};

class Table
{
public:
	Table(Database *db, const QByteArray &name);
	
	Database* db() const { return _db; }
	QByteArray name() const { return _name; }
	QList<Column> fields() const { return _fields; }
	QByteArray primaryKey() const { return _pk; }
	
	void setPrimaryKey(const QByteArray &pk) { _pk = pk; }
	
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
