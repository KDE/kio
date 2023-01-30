/*
    SPDX-License-Identifier: LGPL-2.0-or-later
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2019-2022 Harald Sitter <sitter@kde.org>
*/

#include "workerbase.h"
#include "workerbase_p.h"

#include <commands_p.h>

namespace KIO
{

WorkerBase::WorkerBase(const QByteArray &protocol, const QByteArray &poolSocket, const QByteArray &appSocket)
    : d(new WorkerBasePrivate(protocol, poolSocket, appSocket, this))
{
}

WorkerBase::~WorkerBase() = default;

void WorkerBase::dispatchLoop()
{
    d->bridge.dispatchLoop();
}

void WorkerBase::connectWorker(const QString &address)
{
    d->bridge.connectSlave(address);
}

void WorkerBase::disconnectWorker()
{
    d->bridge.disconnectSlave();
}

void WorkerBase::setMetaData(const QString &key, const QString &value)
{
    d->bridge.setMetaData(key, value);
}

QString WorkerBase::metaData(const QString &key) const
{
    return d->bridge.metaData(key);
}

MetaData WorkerBase::allMetaData() const
{
    return d->bridge.allMetaData();
}

bool WorkerBase::hasMetaData(const QString &key) const
{
    return d->bridge.hasMetaData(key);
}

QMap<QString, QVariant> WorkerBase::mapConfig() const
{
    return d->bridge.mapConfig();
}

bool WorkerBase::configValue(const QString &key, bool defaultValue) const
{
    return d->bridge.configValue(key, defaultValue);
}

int WorkerBase::configValue(const QString &key, int defaultValue) const
{
    return d->bridge.configValue(key, defaultValue);
}

QString WorkerBase::configValue(const QString &key, const QString &defaultValue) const
{
    return d->bridge.configValue(key, defaultValue);
}

KConfigGroup *WorkerBase::config()
{
    return d->bridge.config();
}

void WorkerBase::sendMetaData()
{
    d->bridge.sendMetaData();
}

void WorkerBase::sendAndKeepMetaData()
{
    d->bridge.sendAndKeepMetaData();
}

KRemoteEncoding *WorkerBase::remoteEncoding()
{
    return d->bridge.remoteEncoding();
}

void WorkerBase::data(const QByteArray &data)
{
    d->bridge.data(data);
}

void WorkerBase::dataReq()
{
    d->bridge.dataReq();
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 103)
void WorkerBase::needSubUrlData()
{
    d->bridge.needSubUrlData();
}
#endif

void WorkerBase::workerStatus(const QString &host, bool connected)
{
    d->bridge.slaveStatus(host, connected);
}

void WorkerBase::canResume()
{
    d->bridge.canResume();
}

void WorkerBase::totalSize(KIO::filesize_t _bytes)
{
    d->bridge.totalSize(_bytes);
}

void WorkerBase::processedSize(KIO::filesize_t _bytes)
{
    d->bridge.processedSize(_bytes);
}

void WorkerBase::written(KIO::filesize_t _bytes)
{
    d->bridge.written(_bytes);
}

void WorkerBase::position(KIO::filesize_t _pos)
{
    d->bridge.position(_pos);
}

void WorkerBase::truncated(KIO::filesize_t _length)
{
    d->bridge.truncated(_length);
}

void WorkerBase::speed(unsigned long _bytes_per_second)
{
    d->bridge.speed(_bytes_per_second);
}

void WorkerBase::redirection(const QUrl &_url)
{
    d->bridge.redirection(_url);
}

void WorkerBase::errorPage()
{
    d->bridge.errorPage();
}

void WorkerBase::mimeType(const QString &_type)
{
    d->bridge.mimeType(_type);
}

void WorkerBase::exit()
{
    d->bridge.exit();
}

void WorkerBase::warning(const QString &_msg)
{
    d->bridge.warning(_msg);
}

void WorkerBase::infoMessage(const QString &_msg)
{
    d->bridge.infoMessage(_msg);
}

void WorkerBase::statEntry(const UDSEntry &entry)
{
    d->bridge.statEntry(entry);
}

void WorkerBase::listEntry(const UDSEntry &entry)
{
    d->bridge.listEntry(entry);
}

void WorkerBase::listEntries(const UDSEntryList &list)
{
    d->bridge.listEntries(list);
}

void WorkerBase::appConnectionMade()
{
} // No response!

void WorkerBase::setHost(QString const &, quint16, QString const &, QString const &)
{
} // No response!

WorkerResult WorkerBase::openConnection()
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_CONNECT));
}

void WorkerBase::closeConnection()
{
} // No response!

WorkerResult WorkerBase::stat(QUrl const &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_STAT));
}

WorkerResult WorkerBase::put(QUrl const &, int, JobFlags)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_PUT));
}

WorkerResult WorkerBase::special(const QByteArray &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SPECIAL));
}

WorkerResult WorkerBase::listDir(QUrl const &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_LISTDIR));
}

WorkerResult WorkerBase::get(QUrl const &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_GET));
}

WorkerResult WorkerBase::open(QUrl const &, QIODevice::OpenMode)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_OPEN));
}

WorkerResult WorkerBase::read(KIO::filesize_t)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_READ));
}

WorkerResult WorkerBase::write(const QByteArray &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_WRITE));
}

WorkerResult WorkerBase::seek(KIO::filesize_t)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SEEK));
}

WorkerResult WorkerBase::truncate(KIO::filesize_t)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_TRUNCATE));
}

WorkerResult WorkerBase::close()
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_CLOSE));
}

WorkerResult WorkerBase::mimetype(QUrl const &url)
{
    return get(url);
}

WorkerResult WorkerBase::rename(QUrl const &, QUrl const &, JobFlags)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_RENAME));
}

WorkerResult WorkerBase::symlink(QString const &, QUrl const &, JobFlags)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SYMLINK));
}

WorkerResult WorkerBase::copy(QUrl const &, QUrl const &, int, JobFlags)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_COPY));
}

WorkerResult WorkerBase::del(QUrl const &, bool)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_DEL));
}

WorkerResult WorkerBase::mkdir(QUrl const &, int)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_MKDIR));
}

WorkerResult WorkerBase::chmod(QUrl const &, int)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_CHMOD));
}

WorkerResult WorkerBase::setModificationTime(QUrl const &, const QDateTime &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SETMODIFICATIONTIME));
}

WorkerResult WorkerBase::chown(QUrl const &, const QString &, const QString &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_CHOWN));
}

WorkerResult WorkerBase::multiGet(const QByteArray &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_MULTI_GET));
}

WorkerResult WorkerBase::fileSystemFreeSpace(const QUrl &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_FILESYSTEMFREESPACE));
}

void WorkerBase::worker_status()
{
    workerStatus(QString(), false);
}

void WorkerBase::reparseConfiguration()
{
    // base implementation called by bridge
}

int WorkerBase::openPasswordDialog(AuthInfo &info, const QString &errorMsg)
{
    return d->bridge.openPasswordDialogV2(info, errorMsg);
}

int WorkerBase::messageBox(MessageBoxType type, const QString &text, const QString &title, const QString &primaryActionText, const QString &secondaryActionText)
{
    return messageBox(text, type, title, primaryActionText, secondaryActionText, QString());
}

int WorkerBase::messageBox(const QString &text,
                           MessageBoxType type,
                           const QString &title,
                           const QString &primaryActionText,
                           const QString &secondaryActionText,
                           const QString &dontAskAgainName)
{
    return d->bridge.messageBox(text, static_cast<SlaveBase::MessageBoxType>(type), title, primaryActionText, secondaryActionText, dontAskAgainName);
}

bool WorkerBase::canResume(KIO::filesize_t offset)
{
    return d->bridge.canResume(offset);
}

int WorkerBase::waitForAnswer(int expected1, int expected2, QByteArray &data, int *pCmd)
{
    return d->bridge.waitForAnswer(expected1, expected2, data, pCmd);
}

int WorkerBase::readData(QByteArray &buffer)
{
    return d->bridge.readData(buffer);
}

void WorkerBase::setTimeoutSpecialCommand(int timeout, const QByteArray &data)
{
    d->bridge.setTimeoutSpecialCommand(timeout, data);
}

bool WorkerBase::checkCachedAuthentication(AuthInfo &info)
{
    return d->bridge.checkCachedAuthentication(info);
}

bool WorkerBase::cacheAuthentication(const AuthInfo &info)
{
    return d->bridge.cacheAuthentication(info);
}

int WorkerBase::connectTimeout()
{
    return d->bridge.connectTimeout();
}

int WorkerBase::proxyConnectTimeout()
{
    return d->bridge.proxyConnectTimeout();
}

int WorkerBase::responseTimeout()
{
    return d->bridge.responseTimeout();
}

int WorkerBase::readTimeout()
{
    return d->bridge.readTimeout();
}

bool WorkerBase::wasKilled() const
{
    return d->bridge.wasKilled();
}

void WorkerBase::lookupHost(const QString &host)
{
    return d->bridge.lookupHost(host);
}

int WorkerBase::waitForHostInfo(QHostInfo &info)
{
    return d->bridge.waitForHostInfo(info);
}

PrivilegeOperationStatus WorkerBase::requestPrivilegeOperation(const QString &operationDetails)
{
    return d->bridge.requestPrivilegeOperation(operationDetails);
}

void WorkerBase::addTemporaryAuthorization(const QString &action)
{
    d->bridge.addTemporaryAuthorization(action);
}

class WorkerResultPrivate
{
public:
    bool success;
    int error;
    QString errorString;
};

WorkerResult::~WorkerResult() = default;

WorkerResult::WorkerResult(const WorkerResult &rhs)
    : d(std::make_unique<WorkerResultPrivate>(*rhs.d))
{
}

WorkerResult &WorkerResult::operator=(const WorkerResult &rhs)
{
    if (this == &rhs) {
        return *this;
    }
    d = std::make_unique<WorkerResultPrivate>(*rhs.d);
    return *this;
}

WorkerResult::WorkerResult(WorkerResult &&) noexcept = default;
WorkerResult &WorkerResult::operator=(WorkerResult &&) noexcept = default;

bool WorkerResult::success() const
{
    return d->success;
}

int WorkerResult::error() const
{
    return d->error;
}

QString WorkerResult::errorString() const
{
    return d->errorString;
}

Q_REQUIRED_RESULT WorkerResult WorkerResult::fail(int _error, const QString &_errorString)
{
    return WorkerResult(std::make_unique<WorkerResultPrivate>(WorkerResultPrivate{false, _error, _errorString}));
}

Q_REQUIRED_RESULT WorkerResult WorkerResult::pass()
{
    return WorkerResult(std::make_unique<WorkerResultPrivate>(WorkerResultPrivate{true, 0, QString()}));
}

WorkerResult::WorkerResult(std::unique_ptr<WorkerResultPrivate> &&dptr)
    : d(std::move(dptr))
{
}

void WorkerBase::setIncomingMetaData(const KIO::MetaData &metaData)
{
    d->bridge.setIncomingMetaData(metaData);
}
} // namespace KIO
