#include "driver/driver.h"

#include <unistd.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

using namespace qsl;
using namespace qsl::driver;

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName("QSLDump");
	QCoreApplication::setApplicationVersion(QSL_VERSION_STR);
	
	QCommandLineParser parser;
	parser.setApplicationDescription("Dump QSL for an existing database");
	parser.addHelpOption();
	parser.addVersionOption();
	QCommandLineOption driverOption(QStringList() << "d" << "driver", "The driver used to connect to the database", "driver", "psql");
	parser.addOption(driverOption);
	QCommandLineOption hostOption(QStringList() << "h" << "host", "The host of the database server if required", "host");
	parser.addOption(hostOption);
	QCommandLineOption portOption(QStringList() << "p" << "port", "The port of the database server if required", "port");
	parser.addOption(portOption);
	QCommandLineOption userOption(QStringList() << "u" << "user", "The user used to connect to the database if required", "username");
	parser.addOption(userOption);
	QCommandLineOption passwordOption("password", "The password used to connect to the database if required", "password");
	parser.addOption(passwordOption);
	QCommandLineOption passwordAskOption("pw", "Ask for a password on the command line");
	parser.addOption(passwordAskOption);
	QCommandLineOption outputOption(QStringList() << "o" << "out", "The file to write the qsl file (if - write to stdout)", "file", "-");
	parser.addOption(outputOption);
	parser.addPositionalArgument("name", "The name (or filename) of the database", "<db-name>");
	parser.process(app);
	QStringList args = parser.positionalArguments();
	if (args.size() != 1)
	{
		parser.showHelp(1);
		return 1;
	}
	
	FILE *out = stdout;
	if (parser.value(outputOption) != "-")
	{
		out = fopen(qPrintable(parser.value(outputOption)), "w");
		if (!out)
		{
			fprintf(stderr, "Failed to open %s: %s\n", qPrintable(parser.value(outputOption)), strerror(errno));
			return 1;
		}
	}
	
	Driver *driver = Driver::driver(parser.value(driverOption));
	if (!driver)
		return 1;
	Database *db = driver->newDatabase();
	Q_ASSERT(db);
	db->setName(args[0]);
	if (parser.isSet(hostOption))
		db->setHost(parser.value(hostOption));
	if (parser.isSet(portOption))
		db->setPort(parser.value(portOption).toInt());
	if (parser.isSet(userOption))
		db->setUser(parser.value(userOption));
	if (parser.isSet(passwordOption))
		db->setPassword(parser.value(passwordOption));
	if (parser.isSet(passwordAskOption))
		db->setPassword(getpass("Password:"));
	if (!db->connect())
	{
		fprintf(stderr, "Failed to connect to database\n");
		return 1;
	}
	
	QString name = args[0];
	if (name.contains('/'))
		name = name.mid(name.lastIndexOf('/')+1);
	if (name.contains('.'))
		name = name.mid(0, name.lastIndexOf('.'));
	
	fprintf(out, "database \"%s\"\n", qPrintable(name));
	// TODO charset and usevar
	for (auto tbl : db->tables())
	{
		fprintf(out, "\n");
		fprintf(out, "table \"%s\"\n", tbl.name().data());
		for (auto col : tbl.columns())
		{
			fprintf(out, "- %s \"%s\"", col.type(), col.name().data());
			fprintf(out, "\n");
		}
	}
	
	fclose(out);
	return 0;
}
