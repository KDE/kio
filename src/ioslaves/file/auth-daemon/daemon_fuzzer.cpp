#include <QtCore>
#include "authdaemoninterface.h"

static QRandomGenerator generator;

static OrgKdeKioFilemanagementInterface* iface = nullptr;

inline QString randomString()
{
    const auto len = generator.bounded(0, 25565);
    QString str;
    str.reserve(len);

    for (int i = 0; i < len; i++) {
        str += QChar(generator.bounded(0, INT_MAX));
    }

    return str;
}

quint32 randInt()
{
    return generator.generate();
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!iface) {
        iface =
            new OrgKdeKioFilemanagementInterface(
                    QStringLiteral("org.kde.kio.filemanagement"),
                    QStringLiteral("/"),
                    QDBusConnection::systemBus(),
                    nullptr);
    }

    QByteArray dat(reinterpret_cast<const char*>(data), size);

    if (!dat.isEmpty()) {
        generator.seed(dat[0]);
    }

    switch (generator.bounded(0, 10)) {
    case 0: iface->UpdateTime(randomString(), randInt(), randInt()).waitForFinished(); break;
    case 1: iface->ChangeMode(randomString(), randInt()).waitForFinished(); break;
    case 2: iface->ChangeOwner(randomString(), randInt(), randInt()).waitForFinished(); break;
    case 3: iface->CreateSymlink(randomString(), randomString()).waitForFinished(); break;
    case 4: iface->Delete(randomString()).waitForFinished(); break;
    case 5: iface->MakeDirectory(randomString(), randInt()).waitForFinished(); break;
    case 6: iface->Open(randomString(), randInt(), randInt()).waitForFinished(); break;
    case 7: iface->OpenDirectory(randomString(), randInt(), randInt()).waitForFinished(); break;
    case 8: iface->RemoveDir(randomString()).waitForFinished(); break;
    case 9: iface->Rename(randomString(), randomString()).waitForFinished(); break;
    }

    return 0;
}