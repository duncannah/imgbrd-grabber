#include "exiftool.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include "logger.h"


Exiftool::Exiftool(QObject *parent)
	: QObject(parent)
{
	connect(&m_process, &QProcess::readyReadStandardError, this, &Exiftool::onError);
}


QString Exiftool::version(int msecs)
{
	QProcess process;
	process.start("exiftool", { "-ver" });

	if (!process.waitForStarted(msecs)) {
		return "";
	}
	if (!process.waitForFinished(msecs)) {
		process.kill();
		return "";
	}
	if (process.exitCode() != 0) {
		return "";
	}

	return QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
}


bool Exiftool::start(int msecs)
{
	if (m_process.state() != QProcess::NotRunning) {
		return true;
	}

	const QString program = "exiftool";
	const QStringList arguments { "-stay_open", "True", "-@", "-" };

	m_process.start(program, arguments);
	bool ok = m_process.waitForStarted(msecs);

	if (!ok) {
		log(QStringLiteral("Error starting exiftool: %1 (%2)").arg(m_process.errorString()).arg(m_process.error()));
	}

	return ok;
}

bool Exiftool::setMetadata(const QString &file, const QMap<QString, QString> &metadata, bool clear, bool keepColorProfile, SidecarFile sidecarFile, bool sidecarFileNoExtension, int msecs)
{
	QStringList commands;
	commands.append({ "-charset", "filename=utf8" });
	commands.append({ "-sep", ";" });
	for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
		commands.append("-" + it.key() + "=" + it.value());
	}
	commands.append("-m"); // Ignore minor errors

	// Try to write metadata to the file first, unless we only want to write to the sidecar
	if (sidecarFile != SidecarFile::Only) {
		QStringList originalFileCommands(commands);
		if (clear) {
			originalFileCommands.insert(2, "-all=");
			if (keepColorProfile) {
				originalFileCommands.insert(3, QStringList{ "--icc_profile:all", "-tagsfromfile", "@", "-colorspacetags" }.join("\n"));
			}
		}
		originalFileCommands.append("-overwrite_original");

		bool ok = execute(file, originalFileCommands.join("\n"), msecs);

		// Stop here if we only want to write to the original file
		if (sidecarFile == SidecarFile::No) {
			return ok;
		}

		// Stop here on success if we only want to write sidecar files on error
		if (ok && sidecarFile == SidecarFile::OnError) {
			return ok;
		}
	}

	// If we're here, that means we either only want to write sidecar files, always want both, or an error occurred and we only write sidecars on error
	const QFileInfo info(file);
	const QString sidecarFilename = (sidecarFileNoExtension ? info.path() + QDir::separator() + info.completeBaseName() : file) + ".xmp";
	return execute(sidecarFilename, commands.join("\n"), msecs);
}

bool Exiftool::execute(const QString &file, const QString &command, int msecs)
{
	if (m_process.state() != QProcess::Running) {
		log(QStringLiteral("Cannot execute command since Exiftool is not running"));
		return false;
	}

	const QString toWrite = (command.isEmpty() ? "" : command + "\n") + file + "\n-execute\n";
	m_process.write(toWrite.toUtf8());

	m_process.setReadChannel(QProcess::StandardOutput);

	while (m_process.waitForReadyRead(msecs)) {
		QString output = QString::fromLocal8Bit(m_process.readAllStandardOutput()).trimmed();
		log(QString("[Exiftool] %1").arg(output), Logger::Debug);
		if (output.endsWith("{ready}")) {
			return !output.contains("due to errors");
		}
	}

	return false;
}

bool Exiftool::stop(int msecs)
{
	if (m_process.state() == QProcess::NotRunning) {
		return true;
	}

	m_process.write("-stay_open\nFalse\n");
	m_process.closeWriteChannel();
	return m_process.waitForFinished(msecs);
}


void Exiftool::onError()
{
	const QString output = QString::fromLocal8Bit(m_process.readAllStandardError()).trimmed();
	const Logger::LogLevel logLevel = output.startsWith("Warning:") ? Logger::Warning : Logger::Error;
	log(QString("[Exiftool] %1").arg(output), logLevel);
}
