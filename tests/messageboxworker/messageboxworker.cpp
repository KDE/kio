/*
    SPDX-FileCopyrightText: 2022 Friedrich W. H. Kossebau <kossebau@kde.org>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "messageboxworker.h"

// KF
#include <KIO/UDSEntry>
// Qt
#include <QCoreApplication>
#include <QDebug>
#include <QMap>

// Pseudo plugin class to embed meta data
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.messagebox" FILE "messageboxworker.json")
};

extern "C" {
int Q_DECL_EXPORT kdemain(int argc, char **argv);
}

int kdemain(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kiomessagebox"));

    MessageBoxWorker worker(argv[2], argv[3]);
    worker.dispatchLoop();

    return 0;
}

namespace
{

const QString kdeOrgCertChain = QStringLiteral(
    "-----BEGIN CERTIFICATE-----\n"
    "MIIHJDCCBgygAwIBAgIQYxPG9R/EMYOGtmoK3J5ByDANBgkqhkiG9w0BAQsFADCB\n"
    "jzELMAkGA1UEBhMCR0IxGzAZBgNVBAgTEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4G\n"
    "A1UEBxMHU2FsZm9yZDEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMTcwNQYDVQQD\n"
    "Ey5TZWN0aWdvIFJTQSBEb21haW4gVmFsaWRhdGlvbiBTZWN1cmUgU2VydmVyIENB\n"
    "MB4XDTIxMTAwNTAwMDAwMFoXDTIyMTAxNzIzNTk1OVowFDESMBAGA1UEAwwJKi5r\n"
    "ZGUub3JnMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEA00TBF2PRJ+YJ\n"
    "LzO/Gab+mZ21NIHwDXCDctq1v5YfMeHVR/rsOYu9w8DKzD6YShjdYyKaFfaBPgrI\n"
    "L7MnEDt60wWMsSBEhJMLvgBg7kODd7fzYeeXLoSe7SdrhoZADD0dvML7Y9ow0OSf\n"
    "fWC4ihwG08pU1NepKOvcNMPPTpDeYr9bDYOJEuYdPdgLx9uw0FCT8bWx/LLHUFOC\n"
    "eap+/iczZcYAM++953yQ7iQOYDys1LyZT/j2zEjg/BcmTg3gTTOC1BazHYB9q1KU\n"
    "7qaHaLDBnfx2a5tsKGKYxhx56gcrmLQkCcH6wyUuLLlBGjr7VnlUpmNIV8AKUaiE\n"
    "K7GLJZjvbP/7mbAx1EPECxREjJ54YIpi6hPjLjrFfBxAP+epmrQZz8faQ9wFVeO8\n"
    "Tt1fpG1kStWZJpGLsCX3/YPBsh6U6xC2V8UK5YnnWA9hDayTevCMul/uh7YOJGSy\n"
    "UsxKA72aiuJ87WrEfpndThM9Q7xvytkYF7ekIkMNKScsst0/qdWeFnKVHGX6xgxM\n"
    "mXJP1gcOO2ulYIxfpxLZIGrCyMUYmmbkj10kb2ZS8FJhWTbSSZyRQ+01LXmYyC2V\n"
    "4MBB8pY1XorRzFuPgyqI8iMkO2yYDSGBw0QAFuFFHB7h6+OK5gNjC4mD8bO7bEig\n"
    "3fcRwDzUyXPHYy919djxhyvHKC2Kj98CAwEAAaOCAvQwggLwMB8GA1UdIwQYMBaA\n"
    "FI2MXsRUrYrhd+mb+ZsF4bgBjWHhMB0GA1UdDgQWBBShM3QAgRLzTMAcX7L3ViqM\n"
    "1cW1kzAOBgNVHQ8BAf8EBAMCBaAwDAYDVR0TAQH/BAIwADAdBgNVHSUEFjAUBggr\n"
    "BgEFBQcDAQYIKwYBBQUHAwIwSQYDVR0gBEIwQDA0BgsrBgEEAbIxAQICBzAlMCMG\n"
    "CCsGAQUFBwIBFhdodHRwczovL3NlY3RpZ28uY29tL0NQUzAIBgZngQwBAgEwgYQG\n"
    "CCsGAQUFBwEBBHgwdjBPBggrBgEFBQcwAoZDaHR0cDovL2NydC5zZWN0aWdvLmNv\n"
    "bS9TZWN0aWdvUlNBRG9tYWluVmFsaWRhdGlvblNlY3VyZVNlcnZlckNBLmNydDAj\n"
    "BggrBgEFBQcwAYYXaHR0cDovL29jc3Auc2VjdGlnby5jb20wHQYDVR0RBBYwFIIJ\n"
    "Ki5rZGUub3JnggdrZGUub3JnMIIBfgYKKwYBBAHWeQIEAgSCAW4EggFqAWgAdgBG\n"
    "pVXrdfqRIDC1oolp9PN9ESxBdL79SbiFq/L8cP5tRwAAAXxPukAhAAAEAwBHMEUC\n"
    "IQCE+7woQy/KdPuDdG231NKPubsYZaM7v5P1gl2Ari2/IAIgegQ2sDSiqdxEVV51\n"
    "Dj3OzGGiRdGDRhaxQ3cabA742mkAdgBByMqx3yJGShDGoToJQodeTjGLGwPr60vH\n"
    "aPCQYpYG9gAAAXxPuj/eAAAEAwBHMEUCIQCUa9AoUVxL8t1F3R8vkP6BnkGuSCsa\n"
    "q4ZKc8381KxUCAIgIguRzJLrH6NQcHFdSQD1e1gOGSxsoRaMYceE+GTl9ZMAdgAp\n"
    "eb7wnjk5IfBWc59jpXflvld9nGAK+PlNXSZcJV3HhAAAAXxPuj+2AAAEAwBHMEUC\n"
    "IAdw8Q7jQRzi9sMoCfaFcNg0xIh9B8Ii4iV6hG+Ec5B+AiEAqqco3T3l3Se9WDWC\n"
    "kSpOMdeiGdvYbq5cyG48jXWPNhYwDQYJKoZIhvcNAQELBQADggEBAMQiM6KnNEMH\n"
    "5eUx/0kTJ2gwlx2KKt81KF5SM6hTiArrcfymG0FitmYjI6euWnPtNKWI//EGgGFU\n"
    "3Kp3T4oI09LGBFB0Tvr9QQbP05FBkqu0rnvalc2iiq+bSFkgzJ6YeWGIovjv7+1F\n"
    "Kthfil10s6mN4j6UxY6wAKTZq+p5LNUUv55j/t+i8J145j0qJ5IaZZPtVQrCa85u\n"
    "t+v40WPxKepNqLv165T3wRfPnVtXlyxgUsBm81ZVw+mckJH3f8JpAnIvmkfZ528N\n"
    "0lv8AyjIepK0y8KQE4LGy00mW8qGqWrUt09uE9imOAVbAMD2sH24x3tz1gJT+FY6\n"
    "vXNzj3j5/eo=\n"
    "-----END CERTIFICATE-----\n"
    "\u0001"
    "-----BEGIN CERTIFICATE-----\n"
    "MIIGEzCCA/ugAwIBAgIQfVtRJrR2uhHbdBYLvFMNpzANBgkqhkiG9w0BAQwFADCB\n"
    "iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl\n"
    "cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV\n"
    "BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTgx\n"
    "MTAyMDAwMDAwWhcNMzAxMjMxMjM1OTU5WjCBjzELMAkGA1UEBhMCR0IxGzAZBgNV\n"
    "BAgTEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4GA1UEBxMHU2FsZm9yZDEYMBYGA1UE\n"
    "ChMPU2VjdGlnbyBMaW1pdGVkMTcwNQYDVQQDEy5TZWN0aWdvIFJTQSBEb21haW4g\n"
    "VmFsaWRhdGlvbiBTZWN1cmUgU2VydmVyIENBMIIBIjANBgkqhkiG9w0BAQEFAAOC\n"
    "AQ8AMIIBCgKCAQEA1nMz1tc8INAA0hdFuNY+B6I/x0HuMjDJsGz99J/LEpgPLT+N\n"
    "TQEMgg8Xf2Iu6bhIefsWg06t1zIlk7cHv7lQP6lMw0Aq6Tn/2YHKHxYyQdqAJrkj\n"
    "eocgHuP/IJo8lURvh3UGkEC0MpMWCRAIIz7S3YcPb11RFGoKacVPAXJpz9OTTG0E\n"
    "oKMbgn6xmrntxZ7FN3ifmgg0+1YuWMQJDgZkW7w33PGfKGioVrCSo1yfu4iYCBsk\n"
    "Haswha6vsC6eep3BwEIc4gLw6uBK0u+QDrTBQBbwb4VCSmT3pDCg/r8uoydajotY\n"
    "uK3DGReEY+1vVv2Dy2A0xHS+5p3b4eTlygxfFQIDAQABo4IBbjCCAWowHwYDVR0j\n"
    "BBgwFoAUU3m/WqorSs9UgOHYm8Cd8rIDZsswHQYDVR0OBBYEFI2MXsRUrYrhd+mb\n"
    "+ZsF4bgBjWHhMA4GA1UdDwEB/wQEAwIBhjASBgNVHRMBAf8ECDAGAQH/AgEAMB0G\n"
    "A1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjAbBgNVHSAEFDASMAYGBFUdIAAw\n"
    "CAYGZ4EMAQIBMFAGA1UdHwRJMEcwRaBDoEGGP2h0dHA6Ly9jcmwudXNlcnRydXN0\n"
    "LmNvbS9VU0VSVHJ1c3RSU0FDZXJ0aWZpY2F0aW9uQXV0aG9yaXR5LmNybDB2Bggr\n"
    "BgEFBQcBAQRqMGgwPwYIKwYBBQUHMAKGM2h0dHA6Ly9jcnQudXNlcnRydXN0LmNv\n"
    "bS9VU0VSVHJ1c3RSU0FBZGRUcnVzdENBLmNydDAlBggrBgEFBQcwAYYZaHR0cDov\n"
    "L29jc3AudXNlcnRydXN0LmNvbTANBgkqhkiG9w0BAQwFAAOCAgEAMr9hvQ5Iw0/H\n"
    "ukdN+Jx4GQHcEx2Ab/zDcLRSmjEzmldS+zGea6TvVKqJjUAXaPgREHzSyrHxVYbH\n"
    "7rM2kYb2OVG/Rr8PoLq0935JxCo2F57kaDl6r5ROVm+yezu/Coa9zcV3HAO4OLGi\n"
    "H19+24rcRki2aArPsrW04jTkZ6k4Zgle0rj8nSg6F0AnwnJOKf0hPHzPE/uWLMUx\n"
    "RP0T7dWbqWlod3zu4f+k+TY4CFM5ooQ0nBnzvg6s1SQ36yOoeNDT5++SR2RiOSLv\n"
    "xvcRviKFxmZEJCaOEDKNyJOuB56DPi/Z+fVGjmO+wea03KbNIaiGCpXZLoUmGv38\n"
    "sbZXQm2V0TP2ORQGgkE49Y9Y3IBbpNV9lXj9p5v//cWoaasm56ekBYdbqbe4oyAL\n"
    "l6lFhd2zi+WJN44pDfwGF/Y4QA5C5BIG+3vzxhFoYt/jmPQT2BVPi7Fp2RBgvGQq\n"
    "6jG35LWjOhSbJuMLe/0CjraZwTiXWTb2qHSihrZe68Zk6s+go/lunrotEbaGmAhY\n"
    "LcmsJWTyXnW0OMGuf1pGg+pRyrbxmRE1a6Vqe8YAsOf4vmSyrcjC8azjUeqkk+B5\n"
    "yOGBQMkKW+ESPMFgKuOXwIlCypTPRpgSabuY0MLTDXJLR27lk8QyKGOHQ+SwMj4K\n"
    "00u/I5sUKUErmgQfky3xxzlIPK1aEn8=\n"
    "-----END CERTIFICATE-----\n"
    "\u0001"
    "-----BEGIN CERTIFICATE-----\n"
    "MIIF3jCCA8agAwIBAgIQAf1tMPyjylGoG7xkDjUDLTANBgkqhkiG9w0BAQwFADCB\n"
    "iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl\n"
    "cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV\n"
    "BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAw\n"
    "MjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNV\n"
    "BAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU\n"
    "aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBSU0EgQ2Vy\n"
    "dGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIK\n"
    "AoICAQCAEmUXNg7D2wiz0KxXDXbtzSfTTK1Qg2HiqiBNCS1kCdzOiZ/MPans9s/B\n"
    "3PHTsdZ7NygRK0faOca8Ohm0X6a9fZ2jY0K2dvKpOyuR+OJv0OwWIJAJPuLodMkY\n"
    "tJHUYmTbf6MG8YgYapAiPLz+E/CHFHv25B+O1ORRxhFnRghRy4YUVD+8M/5+bJz/\n"
    "Fp0YvVGONaanZshyZ9shZrHUm3gDwFA66Mzw3LyeTP6vBZY1H1dat//O+T23LLb2\n"
    "VN3I5xI6Ta5MirdcmrS3ID3KfyI0rn47aGYBROcBTkZTmzNg95S+UzeQc0PzMsNT\n"
    "79uq/nROacdrjGCT3sTHDN/hMq7MkztReJVni+49Vv4M0GkPGw/zJSZrM233bkf6\n"
    "c0Plfg6lZrEpfDKEY1WJxA3Bk1QwGROs0303p+tdOmw1XNtB1xLaqUkL39iAigmT\n"
    "Yo61Zs8liM2EuLE/pDkP2QKe6xJMlXzzawWpXhaDzLhn4ugTncxbgtNMs+1b/97l\n"
    "c6wjOy0AvzVVdAlJ2ElYGn+SNuZRkg7zJn0cTRe8yexDJtC/QV9AqURE9JnnV4ee\n"
    "UB9XVKg+/XRjL7FQZQnmWEIuQxpMtPAlR1n6BB6T1CZGSlCBst6+eLf8ZxXhyVeE\n"
    "Hg9j1uliutZfVS7qXMYoCAQlObgOK6nyTJccBz8NUvXt7y+CDwIDAQABo0IwQDAd\n"
    "BgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rIDZsswDgYDVR0PAQH/BAQDAgEGMA8G\n"
    "A1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAFzUfA3P9wF9QZllDHPF\n"
    "Up/L+M+ZBn8b2kMVn54CVVeWFPFSPCeHlCjtHzoBN6J2/FNQwISbxmtOuowhT6KO\n"
    "VWKR82kV2LyI48SqC/3vqOlLVSoGIG1VeCkZ7l8wXEskEVX/JJpuXior7gtNn3/3\n"
    "ATiUFJVDBwn7YKnuHKsSjKCaXqeYalltiz8I+8jRRa8YFWSQEg9zKC7F4iRO/Fjs\n"
    "8PRF/iKz6y+O0tlFYQXBl2+odnKPi4w2r78NBc5xjeambx9spnFixdjQg3IM8WcR\n"
    "iQycE0xyNN+81XHfqnHd4blsjDwSXWXavVcStkNr/+XeTWYRUc+ZruwXtuhxkYze\n"
    "Sf7dNXGiFSeUHM9h4ya7b6NnJSFd5t0dCy5oGzuCr+yDZ4XUmFF0sbmZgIn/f3gZ\n"
    "XHlKYC6SQK5MNyosycdiyA5d9zZbyuAlJQG03RoHnHcAP9Dc1ew91Pq7P8yF1m9/\n"
    "qS3fuQL39ZeatTXaw2ewh0qpKJ4jjv9cJ2vhsE/zB+4ALtRZh8tSQZXq9EfX7mRB\n"
    "VXyNWQKV3WKdwrnuWih0hKWbt5DHDAff9Yk2dDLWKMGwsAvgnEzDHNb842m1R0aB\n"
    "L6KCq9NjRHDEjf8tM7qtj3u1cIiuPhnPQCjY/MiQu12ZIvVS5ljFH4gxQ+6IHdfG\n"
    "jjxDah2nGN59PRbxYvnKkKj9\n"
    "-----END CERTIFICATE-----\n");

static const QMap<int, QString> typeNames = {
    {KIO::WorkerBase::QuestionTwoActions, QStringLiteral("QuestionTwoActions")},
    {KIO::WorkerBase::WarningTwoActions, QStringLiteral("WarningTwoActions")},
    {KIO::WorkerBase::WarningContinueCancel, QStringLiteral("WarningContinueCancel")},
    {KIO::WorkerBase::WarningTwoActionsCancel, QStringLiteral("WarningTwoActionsCancel")},
    {KIO::WorkerBase::Information, QStringLiteral("Information")},
    {KIO::WorkerBase::SSLMessageBox, QStringLiteral("SSLMessageBox")},
    {KIO::WorkerBase::WarningContinueCancelDetailed, QStringLiteral("WarningContinueCancelDetailed")},
};

static constexpr int messageBoxList = -1;
static constexpr int noMessageBoxType = -2;

static QString buttonCodeToDisplayString(int buttonCode)
{
    switch (buttonCode) {
    case KIO::WorkerBase::Ok:
        return QStringLiteral("Ok");
    case KIO::WorkerBase::Cancel:
        return QStringLiteral("Cancel");
    case KIO::WorkerBase::PrimaryAction:
        return QStringLiteral("PrimaryAction");
    case KIO::WorkerBase::SecondaryAction:
        return QStringLiteral("SecondaryAction");
    case KIO::WorkerBase::Continue:
        return QStringLiteral("Continue");
    default:
        Q_UNREACHABLE();
        return QString();
    };
}

int messageBoxType(const QUrl &url)
{
    QString path = url.adjusted(QUrl::StripTrailingSlash).path();
    if (path.startsWith(QLatin1Char('/'))) {
        path.remove(0, 1);
    }
    if (path.isEmpty()) {
        return messageBoxList;
    }

    bool ok;
    const int type = path.toInt(&ok);
    if (ok && typeNames.contains(type)) {
        return type;
    }
    return noMessageBoxType;
}

KIO::UDSEntry typeDirEntry(int messageBoxType)
{
    KIO::UDSEntry entry;
    entry.reserve(4);
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QString::number(messageBoxType));
    entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, typeNames.value(messageBoxType));
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    return entry;
}

KIO::UDSEntry rootDirEntry()
{
    KIO::UDSEntry entry;

    entry.reserve(3);
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);

    return entry;
}

}

MessageBoxWorker::MessageBoxWorker(const QByteArray &pool_socket, const QByteArray &app_socket)
    : KIO::WorkerBase("messagebox", pool_socket, app_socket)
{
}

MessageBoxWorker::~MessageBoxWorker() = default;

KIO::WorkerResult MessageBoxWorker::get(const QUrl &url)
{
    return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.path());
}

KIO::WorkerResult MessageBoxWorker::stat(const QUrl &url)
{
    const int type = ::messageBoxType(url);

    if (type == ::noMessageBoxType) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, QStringLiteral("No such path."));
    }

    // is root directory?
    if (type == ::messageBoxList) {
        statEntry(rootDirEntry());
        return KIO::WorkerResult::pass();
    }

    // type dir
    statEntry(typeDirEntry(type));
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult MessageBoxWorker::listDir(const QUrl &url)
{
    const int type = ::messageBoxType(url);

    if (type == ::noMessageBoxType) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, QStringLiteral("No such directory."));
    }
    if (type == messageBoxList) {
        const QList<int> items = typeNames.keys();

        // report number of expected entries
        totalSize(1 + items.size());
        // own dir
        listEntry(rootDirEntry());
        // type dirs
        for (const auto &item : items) {
            listEntry(typeDirEntry(item));
        }

        return KIO::WorkerResult::pass();
    }

    // trigger the respective messagebox, then redirect to root dir
    if (type == SSLMessageBox) {
        // kde.org data in October 2022 as example
        setMetaData(QStringLiteral("ssl_in_use"), QStringLiteral("TRUE"));
        setMetaData(QStringLiteral("ssl_peer_chain"), kdeOrgCertChain);
        setMetaData(QStringLiteral("ssl_peer_ip"), QStringLiteral("136.243.103.182"));
        setMetaData(QStringLiteral("ssl_protocol_version"), QStringLiteral("TLSv1.3"));
        setMetaData(QStringLiteral("ssl_cipher"), QStringLiteral("TLS_AES_256_GCM_SHA384"));
        setMetaData(QStringLiteral("ssl_cipher_used_bits"), QStringLiteral("256"));
        setMetaData(QStringLiteral("ssl_cipher_bits"), QStringLiteral("256"));
        sendMetaData();

        messageBox(static_cast<MessageBoxType>(type), QStringLiteral("kde.org"));
    } else {
        if (type == WarningContinueCancelDetailed) {
            setMetaData(QStringLiteral("privilege_conf_details"), QStringLiteral("Some details"));
            sendMetaData();
        }
        const int reply = messageBox(QStringLiteral("Message in a box."),
                                     static_cast<MessageBoxType>(type),
                                     typeNames.value(type),
                                     QStringLiteral("Primary"),
                                     QStringLiteral("Secondary"));
        qDebug() << "MESSAGEBOX REPLY" << buttonCodeToDisplayString(reply);
    }

    redirection(QUrl(QStringLiteral("messagebox:")));
    return KIO::WorkerResult::pass();
}

#include "messageboxworker.moc"
