#include "generator.h"

#include "parser/database.h"
#include "parser/table.h"

#include <QDateTime>
#include <QFile>
#include <QMetaType>
#include <QSet>

bool spis::spisc::generate(Database *db, const QString &filename, const QDir &dir, bool qtype)
{
	if (!db)
		return false;
	QFile out(dir.absoluteFilePath("db_" + db->name() + ".h"));
	if (!out.open(QIODevice::WriteOnly))
	{
		fprintf(stderr, "Unable to write file %s: %s\n", qPrintable(out.fileName()), qPrintable(out.errorString()));
		return false;
	}
	out.write("/*\n");
	out.write("##############################################################################\n"); // 78
	out.write("## This file was generated by spisc - DO NOT EDIT!!!                        ##\n");
	out.write("##                                                                          ##\n");
	out.write("## — File: " + filename.toUtf8() + "\n");
	out.write("## — Database: " + db->name() + "\n");
	out.write("##                                                                          ##\n");
	out.write("## SPIS " + QByteArray::number(SPIS_MAJOR) + "." + QByteArray::number(SPIS_MINOR) + "." + QByteArray::number(SPIS_PATCH));
	QByteArray rem = "                                                            ";
#if SPIS_MAJOR < 10
	rem += " ";
#endif
#if SPIS_MINOR < 10
	rem += " ";
#endif
#if SPIS_PATCH < 10
	rem += " ";
#endif
	out.write(rem + "##\n");
	out.write("##############################################################################\n");
	out.write("*/\n");
	out.write("#pragma once\n\n");
	// spis version check
	out.write("#include <spis_global.h>\n");
	out.write("#if SPIS_MAJOR != " + QByteArray::number(SPIS_MAJOR) + "\n");
	out.write("#  error The SPIS major version does not match (compiled with SPIS " SPIS_VERSION_STR ")\n");
	out.write("#endif\n");
#if SPIS_MAJOR == 0
	out.write("#if SPIS_MINOR != " + QByteArray::number(SPIS_MINOR) + "\n");
	out.write("#  error The SPIS minor version does not match (compiled with SPIS " SPIS_VERSION_STR ")\n");
	out.write("#  error Note that this is only required with SPIS version 0.X.X\n");
	out.write("#endif\n");
#endif
	out.write("#if SPIS_VERSION != 0x" + QByteArray::number(SPIS_VERSION, 16) + "\n");
	out.write("#  warning The SPIS version does not match. Consider recompiling.\n");
	out.write("#endif\n\n");
	// spis headers
	out.write("#include <driver/db.h>\n");
	out.write("#include <driver/driver.h>\n");
	out.write("#include <pk.h>\n");
	out.write("#include <spisdb.h>\n");
	out.write("#include <spisquery.h>\n");
	out.write("#include <spistable.h>\n");
	out.write("#include <spisvariant.h>\n\n");
	// qt headers
	out.write("#include <QSqlError>\n");
	out.write("#include <QSqlQuery>\n\n");
	
	// qtype macros
	if (qtype)
		out.write("#define DB_" + db->name().toUpper() + "_QTYPE\n\n");
	else
		out.write("#define DB_" + db->name().toUpper() + "_NO_QTYPE\n\n");
	
	out.write("namespace spis {\n");
	out.write("namespace db {\n\n");
	
	
	out.write("class " + db->name() + " : public SPISDB\n");
	out.write("{\n");
	for (Table *t : db->tables())
		out.write("  friend class " + t->name() + "_q;\n");
	out.write("public:\n");
	for (Table *t : db->tables())
	{
		out.write("  class " + t->name() + "_t;\n");
		out.write("  class " + t->name() + "_q;\n");
	}
	out.write("private:\n");
	out.write("  static constexpr const char* _charset = \"" + db->charset() + "\";\n");
	out.write("  static constexpr const bool _usevar = " + QByteArray(db->usevar() ? "true" : "false") + ";\n");
	out.write("public:\n");
	out.write("  virtual const char* charset() const override { return _charset; }\n");
	out.write("  virtual bool usevar() const override { return _usevar; }\n\n");
	
	QByteArray list_t = qtype ? "QList" : "std::vector";
	QHash<Table*, QByteArray> pkTypes;
	QHash<Table*, QByteArray> pkCppTypes;
	
	for (Table *t : db->tables())
	{
		if (t->primaryKey().isEmpty())
			fprintf(stderr, "%s: WARNING: No primary key found in table %s, some features may not be available\n", qPrintable(filename), t->name().data());
		else
		{
			out.write("private:\n");
			for (Column &f : t->fields())
				if ((f.constraints() & SPIS::primarykey) == SPIS::primarykey)
				{
					pkTypes.insert(t, f.type());
					pkCppTypes.insert(t, f.cppType());
					out.write("  PrimaryKey<" + f.cppType() + "> _tbl_" + t->name() + "_pk;\n");
				}
		}
		
		out.write("public:\n");
		out.write("  class " + t->name() + "_t\n");
		out.write("  {\n");
		out.write("    friend class " + db->name() + ";\n");
		out.write("    friend class " + t->name() + "_q;\n");
		out.write("  private:\n");
		out.write("    SPISTable *_parent;\n");
		
		// ctor for select
		out.write("  public:\n");
		out.write("    " + t->name() + "_t(SPISTable *parent");
		for (Column &f : t->fields())
		{
			out.write(", ");
			if (f.type() == "password")
				out.write("const QByteArray&");
			else if (f.type() == "date" || f.type() == "time" || f.type() == "datetime")
				out.write("const QVariant&");
			else
				out.write(f.cppArgType());
			out.write(" " + f.name());
		}
		out.write(")\n");
		out.write("      : _parent(parent)\n");
		for (Column &f : t->fields())
		{
			if (f.type() == "password")
				out.write("      , _" + f.name() + "(PasswordEntry{" + f.name() + "})\n");
			else if (f.type() == "date" || f.type() == "time" || f.type() == "datetime")
				out.write("      , _" + f.name() + "(parent->db()->driver()->to" + (qtype ? "Q" : "Chrono")
						  + (f.type()=="time" ? "Time" : (f.type()=="date" ? "Date" : "DateTime"))
						  + "(" + f.name() + "))\n");
			else
				out.write("      , _" + f.name() + "(" + f.name() + ")\n");
		}
		out.write("    {\n");
		out.write("      Q_ASSERT(parent);\n");
		out.write("    }\n");
		
		// ctor for select from selectresult (for the _q type)
		out.write("  private:\n");
		out.write("    " + t->name() + "_t(SPIS_MAYBE_UNUSED " + db->name() + " *db, SPISTable *parent, driver::SelectResult *result, const QString &prefix = QString())\n");
		out.write("      : _parent(parent)\n");
		for (Column &f : t->fields())
		{
			if (f.type() == "password")
			{
				out.write("      , _" + f.name() + "(PasswordEntry{result->value(prefix + QStringLiteral(\"" + f.name() + "\")).toByteArray()})\n");
				continue;
			}
			
			if (f.type() == "date" || f.type() == "time" || f.type() == "datetime")
			{
				out.write("      , _" + f.name() + "(parent->db()->driver()->to" + (qtype ? "Q" : "Chrono") +
						  (f.type() == "date" ? "Date" : (f.type() == "time" ? "Time" : "DateTime")) +
						  "(result->value(prefix + QStringLiteral(\"" + f.name() + "\"))))\n");
				continue;
			}
			
			if (f.type().startsWith('&'))
			{
				const QByteArray table = f.type().mid(1, f.type().indexOf('.') - 1);
				out.write("      , _" + f.name() + "(db, &db->_tbl_" + table + ", result, prefix + QStringLiteral(\"spis_fkey_" + f.name() + "_\"))\n");
				continue;
			}
			
			
			out.write("      , _" + f.name() + "(result->value(prefix + QStringLiteral(\"" + f.name() + "\"))\n");
			if (f.type() == "int")
			{
				if (f.minsize() >= 0 && f.minsize() < 64)
				{
					out.write("#if INT_MAX >= " + QByteArray::number((((qulonglong)1) << (f.minsize() - 1)) - 1) + "\n");
					out.write("          .toInt()\n");
					out.write("#else\n");
				}
				out.write("          .toLongLong()\n");
				if (f.minsize() >= 0 && f.minsize() < 64)
					out.write("#endif\n");
			}
			else if (f.type() == "uint")
			{
				if (f.minsize() >= 0 && f.minsize() < 64)
				{
					out.write("#if UINT_MAX >= " + QByteArray::number((((qulonglong)1) << f.minsize()) - 1) + "\n");
					out.write("          .toUInt()\n");
					out.write("#else\n");
				}
				out.write("          .toULongLong()\n");
				if (f.minsize() >= 0 && f.minsize() < 64)
					out.write("#endif\n");
			}
			else if (f.type() == "double")
			{
				if (f.minsize() >= 0 && f.minsize() <= 4)
					out.write("          .toFloat()\n");
				else
					out.write("          .toDouble()\n");
			}
			else if (f.type() == "bool")
				out.write("          .toBool()\n");
			else if (f.type() == "char" || f.type() == "text" || f.type() == "byte" || f.type() == "blob" || f.type() == "password")
			{
				if (qtype)
				{
					if (f.type() == "char" || f.type() == "text")
						out.write("          .toString()\n");
					else
						out.write("          .toByteArray()\n");
				}
				else
					out.write("          .toByteArray().data()\n");
			}
			out.write("        )\n");
		}
		out.write("    {\n");
		out.write("      Q_ASSERT(parent);\n");
		out.write("    }\n");
		
		// ctor for insert
		out.write("  public:\n");
		out.write("    " + t->name() + "_t(");
		int i = 0;
		for (Column &f : t->fields())
		{
			if ((f.constraints() & SPIS::primarykey) != 0)
				continue;
			if (i > 0)
				out.write(", ");
			out.write(f.cppArgType() + " " + f.name());
			i++;
		}
		out.write(")\n");
		out.write("      : _parent(0)\n");
		i = 0;
		for (Column &f : t->fields())
		{
			if ((f.constraints() & SPIS::primarykey) != 0)
				continue;
			out.write("      , _" + f.name() + "(" + f.name() + ")\n");
		}
		out.write("    {\n");
		out.write("    }\n\n");
		
		// variables & getters
		for (Column &f : t->fields())
		{
			out.write("  private:\n");
			out.write("    static SPISColumn col_" + f.name() + "()\n");
			out.write("    {\n");
			out.write("      static SPISColumn col(\"" + f.name() + "\", \"" + f.type() + "\", " + QByteArray::number(f.minsize()) + ", " + QByteArray::number(f.constraints()) + ", ");
			if (!f.def().isValid() || f.def().isNull())
				out.write("QVariant()");
			else
			{
				auto type = f.def().type();
				if (type == QMetaType::QDate)
				{
					QDate d = f.def().toDate();
					out.write("spisvariant(QDate(" + QByteArray::number(d.year()) + "," + QByteArray::number(d.month()) + "," + QByteArray::number(d.day()) + "))");
				}
				else if (type == QMetaType::QTime)
				{
					QTime t = f.def().toTime();
					out.write("spisvariant(QTime(" + QByteArray::number(t.hour()) + "," + QByteArray::number(t.minute()) + "," + QByteArray::number(t.second()) + "))");
				}
				else if (type == QMetaType::QDateTime)
				{
					QDateTime dt = f.def().toDateTime();
					QDate d = dt.date();
					QTime t = dt.time();
					out.write("spisvariant(QDateTime(QDate(" + QByteArray::number(d.year()) + "," + QByteArray::number(d.month()) + "," + QByteArray::number(d.day())
							  + "),QTime(" + QByteArray::number(t.hour()) + "," + QByteArray::number(t.minute()) + "," + QByteArray::number(t.second()) + ")))");
				}
				else if (type == QMetaType::Int || type == QMetaType::Long || type == QMetaType::LongLong || type == QMetaType::Short)
					out.write("spisvariant((qlonglong)" + QByteArray::number(f.def().toLongLong()) + "L)");
				else if (type == QMetaType::UInt || type == QMetaType::ULong || type == QMetaType::ULongLong || type == QMetaType::UShort)
					out.write("spisvariant((qulonglong)" + QByteArray::number(f.def().toULongLong()) + "L)");
				else if (type == QMetaType::Float || type == QMetaType::Double)
					out.write("spisvariant(" + QByteArray::number(f.def().toDouble()) + ")");
				else
					out.write("spisvariant(QStringLiteral(\"" + f.def().toString().toUtf8() + "\"))");
			}
			out.write(");\n");
			out.write("      return col;\n");
			out.write("    }\n");
			out.write("    " + f.cppType() + " _" + f.name() + ";\n");
			out.write("  public:\n");
			out.write("    " + f.cppType() + " " + f.name() + "() const { return _" + f.name() + "; }\n");
			if (!t->primaryKey().isEmpty() && (f.constraints() & SPIS::primarykey) == 0)
			{
				out.write("    bool set" + f.name().mid(0,1).toUpper() + f.name().mid(1) + "(" + f.cppArgType() + " " + f.name() + ")\n");
				out.write("    {\n");
				out.write("      if (!_parent)\n");
				out.write("        return false;\n");
				
				out.write("      bool success = _parent->db()->db()->updateTable(*_parent, col_" + f.name() + "(), ");
				if (f.type() == "date")
					out.write("_parent->db()->driver()->from" + QByteArray(qtype ? "Q" : "Chrono") + "Date(" + f.name() + ")");
				else if (f.type() == "time")
					out.write("_parent->db()->driver()->from" + QByteArray(qtype ? "Q" : "Chrono") + "Time(" + f.name() + ")");
				else if (f.type() == "datetime")
					out.write("_parent->db()->driver()->from" + QByteArray(qtype ? "Q" : "Chrono") + "DateTime(" + f.name() + ")");
				else if (f.type().startsWith("&"))
					out.write("spisvariant(" + f.name() + "." + f.type().mid(f.type().indexOf('.')+1) + "())");
				else
					out.write("spisvariant(" + f.name() + ")");
				out.write(", spisvariant(_" + t->primaryKey() + "));\n");
				
				out.write("      if (success)\n");
				out.write("        _" + f.name() + " = " + f.name() + ";\n");
				out.write("      return success;\n");
				out.write("    }\n");
			}
			out.write("\n");
		}
		
		// method to create a QVector<QVariant>
		out.write("  public:\n");
		out.write("    QVector<QVariant> toVector() const\n");
		out.write("    {\n");
		out.write("      Q_ASSERT(_parent);\n");
		out.write("      Q_ASSERT(_parent->db());\n");
		out.write("      QVector<QVariant> v(" + QByteArray::number(t->fields().size() - (t->primaryKey().isEmpty() ? 0 : 1)) + ");\n");
		i = 0;
		for (Column &f : t->fields())
		{
			if ((f.constraints() & SPIS::primarykey) != 0)
				continue;
			out.write("      v[" + QByteArray::number(i) + "] = ");
			if (f.type() == "date")
				out.write("_parent->db()->driver()->from" + QByteArray(qtype ? "Q" : "Chrono") + "Date(_" + f.name() + ");\n");
			else if (f.type() == "time")
				out.write("_parent->db()->driver()->from" + QByteArray(qtype ? "Q" : "Chrono") + "Time(_" + f.name() + ");\n");
			else if (f.type() == "datetime")
				out.write("_parent->db()->driver()->from" + QByteArray(qtype ? "Q" : "Chrono") + "DateTime(_" + f.name() + ");\n");
			else if (f.type().startsWith('&'))
				out.write("spisvariant(_" + f.name() + "." + f.type().mid(f.type().indexOf('.') + 1) + "());\n");
			else
				out.write("spisvariant(_" + f.name() + ");\n");
			i++;
		}
		out.write("      return v;\n");
		out.write("    }\n");
		
		out.write("  };\n\n");
		
		// class to create the query
		out.write("  class " + t->name() + "_q : public SPISTableQuery<" + t->name() + "_t, " + list_t + "<" + t->name() + "_t>>\n");
		out.write("  {\n");
		out.write("  public:\n");
		out.write("    " + t->name() + "_q(" + db->name() + " *db, SPISTable *tbl");
		if (!t->primaryKey().isEmpty())
			out.write(", PrimaryKey<" + pkCppTypes[t] + "> *pk");
		out.write(")\n");
		out.write("      : SPISTableQuery(tbl)\n");
		out.write("      , _db(db)\n");
		if (!t->primaryKey().isEmpty())
			out.write("      , _pk(pk)\n");
		out.write("    {\n");
		out.write("      Q_ASSERT(db);\n");
		out.write("    }\n\n");
		out.write("  private:\n");
		out.write("    " + db->name() + " *_db;\n");
		if (!t->primaryKey().isEmpty())
		{
			out.write("    PrimaryKey<" + pkCppTypes[t] + "> *_pk;\n");
		}
		
		// filters
		out.write("  public:\n");
		out.write("    template<typename... Arguments>\n");
		out.write("    " + t->name() + "_q& filter(Arguments... args)\n");
		out.write("    {\n");
		out.write("      QList<SPISFilterExprType> list;\n");
		out.write("      filter0(list, args...);\n");
		out.write("      return *this;\n");
		out.write("    }\n");
		out.write("  private:\n"),
		out.write("    template<typename T, typename... Arguments>\n");
		out.write("    void filter0(QList<SPISFilterExprType> &list, const T &arg, Arguments... args)\n");
		out.write("    {\n");
		out.write("      list.push_back(SPISFilterExprType(arg));\n");
		out.write("      filter0(list, args...);\n");
		out.write("    }\n");
		out.write("    void filter0(const QList<SPISFilterExprType> &list)\n");
		out.write("    {\n");
		out.write("      SPISFilter f = spis_filter(list);\n");
		out.write("      applyFilter(f);\n");
		out.write("    }\n\n");
		
		// limit, asc, desc
		out.write("  public:\n");
		out.write("    " + t->name() + "_q& limit(int l)\n");
		out.write("    {\n");
		out.write("      applyLimit(l);\n");
		out.write("      return *this;\n");
		out.write("    }\n");
		out.write("    " + t->name() + "_q& asc()\n");
		out.write("    {\n");
		out.write("      applyAsc();\n");
		out.write("      return *this;\n");
		out.write("    }\n");
		out.write("    " + t->name() + "_q& desc()\n");
		out.write("    {\n");
		out.write("      applyDesc();\n");
		out.write("      return *this;\n");
		out.write("    }\n\n");
		
		// query
		out.write("  public:\n");
		out.write("    virtual " + list_t + "<" + t->name() + "_t> query() override\n");
		out.write("    {\n");
		QList<Column> join;
		for (Column &f : t->fields())
			if (f.type().startsWith('&'))
				join.push_back(f);
		out.write("      static const QList<driver::Database::SPISJoinTable> join = {\n");
		for (int i = 0; i < join.size(); i++)
		{
			QByteArray table = join[i].type().mid(1, join[i].type().indexOf('.') - 1);
			QByteArray field = join[i].type().mid(join[i].type().indexOf('.') + 1);
			out.write("        { _db->_tbl_" + table + ", _db->_tbl_" + table + ".columns(), " +
					  t->name() + "_t::col_" + join[i].name() + "(), " + table + "_t::col_" + field + "(), "
					  "QStringLiteral(\"spis_fkey_" + join[i].name() + "_\") }");
			if (i != join.size() - 1)
				out.write(",");
			out.write("\n");
		}
		out.write("      };\n");
		out.write("      driver::SelectResult *result = _tbl->db()->db()->selectTable(*_tbl, _filter, join, _limit, _asc);\n");
		out.write("      if (!result)\n");
		out.write("      {\n");
		out.write("        fprintf(stderr, \"SPISQuery: Failed to query " + db->name() + "." + t->name() + "\\n\");\n");
		out.write("        return " + list_t + "<" + t->name() + "_t>();\n");
		out.write("      }\n");
		out.write("      " + list_t + "<" + t->name() + "_t> entries;\n");
		out.write("      while (result->next())\n");
		out.write("      {\n");
		out.write("        " + t->name() + "_t entry(_db, _tbl, result);\n");
		out.write("        entries.push_back(entry);\n");
		out.write("      }\n");
		out.write("      return entries;\n");
		out.write("    }\n\n");
		
		// insert
		out.write("  private:\n");
		out.write("    virtual bool insert0(const QVector<QVector<QVariant>> &rows)\n");
		out.write("    {\n");
		out.write("      static const QList<SPISColumn> cols = {\n");
		for (int i = 0; i < t->fields().size(); i++)
		{
			auto col = t->fields()[i];
			if ((col.constraints() & SPIS::primarykey) == 0)
			{
				out.write("        " + t->name() + "_t::col_" + col.name() + "()");
				if (!t->primaryKey().isEmpty() || i < t->fields().size() - 1)
					out.write(",");
				out.write("\n");
			}
		}
		if (!t->primaryKey().isEmpty())
			out.write("        " + t->name() + "_t::col_" + t->primaryKey() + "()\n");
		out.write("      };\n");
		if (t->primaryKey().isEmpty())
			out.write("      return _tbl->db()->db()->insertIntoTable(*_tbl, cols, rows);\n");
		else
		{
			out.write("      QVector<QVector<QVariant>> insertRows = rows;\n");
			out.write("      for (QVector<QVariant> &insertRow : insertRows)\n");
			out.write("        insertRow.push_back(spisvariant(_pk->next()));\n");
			out.write("      return _tbl->db()->db()->insertIntoTable(*_tbl, cols, insertRows);\n");
		}
		out.write("    }\n");
		out.write("  public:\n");
		out.write("    virtual bool insert(const " + t->name() + "_t &row) override\n");
		out.write("    {\n");
		out.write("      if (row._parent)\n");
		out.write("        return insert0(QVector<QVector<QVariant>>({row.toVector()}));\n");
		out.write("      " + t->name() + "_t r = row;\n");
		out.write("      r._parent = _tbl;\n");
		out.write("      return insert0(QVector<QVector<QVariant>>({r.toVector()}));\n");
		out.write("    }\n");
		out.write("    template<typename ForwardIterator>\n");
		out.write("    bool insert(const ForwardIterator &begin, const ForwardIterator &end)\n");
		out.write("    {\n");
		out.write("      int size = std::distance(begin, end);\n");
		out.write("      QVector<QVector<QVariant>> data(size);\n");
		out.write("      int i = 0;\n");
		out.write("      for (auto it = begin; it != end; it++)\n");
		out.write("      {\n");
		out.write("        " + t->name() + "_t row = *it;\n");
		out.write("        if (!row._parent)\n");
		out.write("          row._parent = _tbl;\n");
		out.write("        data[i] = row.toVector();\n");
		out.write("        i++;\n");
		out.write("      }\n");
		out.write("      return insert0(data);\n");
		out.write("    }\n");
		out.write("    template<typename Container>\n");
		out.write("    bool insert(const Container &rows)\n");
		out.write("    {\n");
		out.write("      return insert(rows.begin(), rows.end());\n");
		out.write("    }\n\n");
		
		// remove
		out.write("    bool remove(const " + t->name() + "_t &row) override\n");
		out.write("    {\n");
		if (t->primaryKey().isEmpty())
		{
			out.write("      fprintf(stderr, \"SPIS[Generated]: Sorry, but the table '" + t->name() + "' does not contain a primary key. You\\n\"\n");
			out.write("                      \"                cannot remove rows from a table without a primary key.\\n\");\n");
			out.write("      return false;\n");
		}
		else
		{
			out.write("      return _tbl->db()->db()->deleteFromTable(*_tbl, spisvariant(row." + t->primaryKey() + "()));\n");
		}
		out.write("    }\n");
		out.write("    template<typename ForwardIterator>\n");
		out.write("    bool remove(const ForwardIterator &begin, const ForwardIterator &end)\n");
		out.write("    {\n");
		if (t->primaryKey().isEmpty())
		{
			out.write("      fprintf(stderr, \"SPIS[Generated]: Sorry, but the table '" + t->name() + "' does not contain a primary key. You\\n\"\n");
			out.write("                      \"                cannot remove rows from a table without a primary key.\\n\");\n");
			out.write("      return false;\n");
		}
		else
		{
			out.write("      int size = std::distance(begin, end);\n");
			out.write("      QVector<QVariant> pks(size);\n");
			out.write("      int i = 0;\n");
			out.write("      for (auto it = begin; it != end; it++)\n");
			out.write("      {\n");
			out.write("        " + t->name() + "_t row = *it;\n");
			out.write("        pks[i] = spisvariant(row." + t->primaryKey() + "());\n");
			out.write("        i++;\n");
			out.write("      }\n");
			out.write("      return _tbl->db()->db()->deleteFromTable(*_tbl, pks);\n");
		}
		out.write("    }\n");
		out.write("    template<typename Container>\n");
		out.write("    bool remove(const Container &rows)\n");
		out.write("    {\n");
		out.write("      return remove(rows.begin(), rows.end());\n");
		out.write("    }\n");
		out.write("    bool remove() override\n");
		out.write("    {\n");
		out.write("      return _tbl->db()->db()->deleteFromTable(*_tbl, _filter);\n");
		out.write("    }\n");
		
		out.write("  };\n\n");
		
		// setup table
		out.write("private:\n");
		out.write("  SPISTable _tbl_" + t->name() + " = SPISTable(\"" + t->name() + "\", \"" + t->primaryKey() + "\", this);\n");
		out.write("  void setupTbl_" + t->name() + "()\n");
		out.write("  {\n");
		for (Column &f : t->fields())
			out.write("    _tbl_" + t->name() + ".addColumn(" + t->name() + "_t::col_" + f.name() + "());\n");
		out.write("  }\n");
		out.write("public:\n");
		out.write("  " + t->name() + "_q " + t->name() + "()\n");
		out.write("  {\n");
		out.write("    return " + t->name() + "_q(this, &_tbl_" + t->name());
		if (!t->primaryKey().isEmpty())
			out.write(", &_tbl_" + t->name() + "_pk");
		out.write(");\n");
		out.write("  }\n\n\n");
	}
	
	// the db setup
	out.write("public:\n");
	out.write("  " + db->name() + "(driver::Driver *driver)\n");
	out.write("    : SPISDB(\"" + db->name() + "\", driver)\n");
	out.write("  {\n");
	out.write("    setupDb();\n");
	out.write("  }\n");
	out.write("  " + db->name() + "(const QString &driver)\n");
	out.write("    : SPISDB(\"" + db->name() + "\", driver)\n");
	out.write("  {\n");
	out.write("    setupDb();\n");
	out.write("  }\n");
	out.write("private:\n");
	out.write("  void setupDb()\n");
	out.write("  {\n");
	for (Table *t : db->tables())
	{
		out.write("    setupTbl_" + t->name() + "();\n");
		out.write("    registerTable(&_tbl_" + t->name() + ");\n");
	}
	out.write("  }\n\n");
	
	// and for the initialization of the primary keys
	out.write("public:\n");
	out.write("  virtual bool connect() override\n");
	out.write("  {\n");
	out.write("    bool success = SPISDB::connect();\n");
	out.write("    if (!success)\n");
	out.write("      return false;\n");
	out.write("    SPIS_MAYBE_UNUSED driver::SelectResult *result = 0;\n");
	for (Table *t : db->tables())
	{
		if (t->primaryKey().isEmpty())
			continue;
		out.write("    result = db()->selectTable(_tbl_" + t->name() + ", QList<SPISColumn>({" + t->name() + "_t::col_" + t->primaryKey() + "()}), "
				  "SPISFilter(), QList<driver::Database::SPISJoinTable>(), 1, false);\n");
		out.write("    if (result && result->first())\n");
		out.write("      _tbl_" + t->name() + "_pk.used(result->value(\"" + t->primaryKey() + "\")");
		if (pkTypes[t] == "int")
			out.write(".toLongLong()");
		else
			out.write(".toULongLong()");
		out.write(");\n");
	}
	out.write("    return true;\n");
	out.write("  }\n\n");
	
	out.write("};\n\n"); // class db->name()
	
	out.write("}\n"); // namespace db
	out.write("}\n"); // namespace spis
	
	out.close();
	return true;
}
