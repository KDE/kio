/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2009 Christian Ehrlicher <ch.ehrlicher@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kio_trash_win.h"
#include "kio/job.h"
#include "kioglobal_p.h"
#include "kiotrashdebug.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDateTime>

#include <KConfigGroup>
#include <KLocalizedString>

#include <objbase.h>

// Pseudo plugin class to embed meta data
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.trash" FILE "trash.json")
};

extern "C" {
int Q_DECL_EXPORT kdemain(int argc, char **argv)
{
    bool bNeedsUninit = (CoInitializeEx(NULL, COINIT_MULTITHREADED) == S_OK);
    // necessary to use other KIO workers
    QCoreApplication app(argc, argv);

    // start the worker
    TrashProtocol worker(argv[1], argv[2], argv[3]);
    worker.dispatchLoop();

    if (bNeedsUninit) {
        CoUninitialize();
    }
    return 0;
}
}

static const qint64 KDE_SECONDS_SINCE_1601 = 11644473600LL;
static const qint64 KDE_USEC_IN_SEC = 1000000LL;
static const int WM_SHELLNOTIFY = (WM_USER + 42);
#ifndef SHCNRF_InterruptLevel
static const int SHCNRF_InterruptLevel = 0x0001;
static const int SHCNRF_ShellLevel = 0x0002;
static const int SHCNRF_RecursiveInterrupt = 0x1000;
#endif

static inline time_t filetimeToTime_t(const FILETIME *time)
{
    ULARGE_INTEGER i64;
    i64.LowPart = time->dwLowDateTime;
    i64.HighPart = time->dwHighDateTime;
    i64.QuadPart /= KDE_USEC_IN_SEC * 10;
    i64.QuadPart -= KDE_SECONDS_SINCE_1601;
    return i64.QuadPart;
}

LRESULT CALLBACK trash_internal_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    if (message == WM_SHELLNOTIFY) {
        TrashProtocol *that = (TrashProtocol *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        that->updateRecycleBin();
    }
    return DefWindowProc(hwnd, message, wp, lp);
}

TrashProtocol::TrashProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app)
    : WorkerBase(protocol, pool, app)
    , m_config(QString::fromLatin1("trashrc"), KConfig::SimpleConfig)
{
    // create a hidden window to receive notifications through window messages
    const QString className = QLatin1String("TrashProtocol_Widget") + QString::number(quintptr(trash_internal_proc));
    HINSTANCE hi = GetModuleHandle(nullptr);
    WNDCLASS wc;
    memset(&wc, 0, sizeof(WNDCLASS));
    wc.lpfnWndProc = trash_internal_proc;
    wc.hInstance = hi;
    wc.lpszClassName = (LPCWSTR)className.utf16();
    RegisterClass(&wc);
    m_notificationWindow = CreateWindow(wc.lpszClassName, // classname
                                        wc.lpszClassName, // window name
                                        0, // style
                                        0,
                                        0,
                                        0,
                                        0, // geometry
                                        0, // parent
                                        0, // menu handle
                                        hi, // application
                                        0); // windows creation data.
    SetWindowLongPtr(m_notificationWindow, GWLP_USERDATA, (LONG_PTR)this);

    // get trash IShellFolder object
    LPITEMIDLIST iilTrash;
    IShellFolder *isfDesktop;
    // we assume that this will always work - if not we've a bigger problem than a kio_trash crash...
    SHGetFolderLocation(NULL, CSIDL_BITBUCKET, 0, 0, &iilTrash);
    SHGetDesktopFolder(&isfDesktop);
    isfDesktop->BindToObject(iilTrash, NULL, IID_IShellFolder2, (void **)&m_isfTrashFolder);
    isfDesktop->Release();
    SHGetMalloc(&m_pMalloc);

    // register for recycle bin notifications, have to do it for *every* single recycle bin
#if 0
    // TODO: this does not work for devices attached after this loop here...
    DWORD dwSize = GetLogicalDriveStrings(0, NULL);
    LPWSTR pszDrives = (LPWSTR)malloc((dwSize + 2) * sizeof(WCHAR));
#endif

    SHChangeNotifyEntry stPIDL;
    stPIDL.pidl = iilTrash;
    stPIDL.fRecursive = TRUE;
    m_hNotifyRBin = SHChangeNotifyRegister(m_notificationWindow,
                                           SHCNRF_InterruptLevel | SHCNRF_ShellLevel | SHCNRF_RecursiveInterrupt,
                                           SHCNE_ALLEVENTS,
                                           WM_SHELLNOTIFY,
                                           1,
                                           &stPIDL);

    ILFree(iilTrash);

    updateRecycleBin();
}

TrashProtocol::~TrashProtocol()
{
    SHChangeNotifyDeregister(m_hNotifyRBin);
    const QString className = QLatin1String("TrashProtocol_Widget") + QString::number(quintptr(trash_internal_proc));
    UnregisterClass((LPCWSTR)className.utf16(), GetModuleHandle(nullptr));
    DestroyWindow(m_notificationWindow);

    if (m_pMalloc) {
        m_pMalloc->Release();
    }
    if (m_isfTrashFolder) {
        m_isfTrashFolder->Release();
    }
}

KIO::WorkerResult TrashProtocol::restore(const QUrl &trashURL, const QUrl &destURL)
{
    LPITEMIDLIST pidl = NULL;
    LPCONTEXTMENU pCtxMenu = NULL;

    const QString path = trashURL.path().mid(1).replace(QLatin1Char('/'), QLatin1Char('\\'));
    LPWSTR lpFile = (LPWSTR)path.utf16();
    HRESULT res = m_isfTrashFolder->ParseDisplayName(0, 0, lpFile, 0, &pidl, 0);
    if (auto result = translateError(res); !result.success()) {
        return result;
    }

    res = m_isfTrashFolder->GetUIObjectOf(0, 1, (LPCITEMIDLIST *)&pidl, IID_IContextMenu, NULL, (LPVOID *)&pCtxMenu);
    if (auto result = translateError(res); !result.success()) {
        return result;
    }

    // this looks hacky but it's the only solution I found so far...
    HMENU hmenuCtx = CreatePopupMenu();
    res = pCtxMenu->QueryContextMenu(hmenuCtx, 0, 1, 0x00007FFF, CMF_NORMAL);
    if (auto result = translateError(res); !result.success()) {
        return result;
    }

    UINT uiCommand = ~0U;
    char verb[MAX_PATH];
    const int iMenuMax = GetMenuItemCount(hmenuCtx);
    for (int i = 0; i < iMenuMax; i++) {
        UINT uiID = GetMenuItemID(hmenuCtx, i) - 1;
        if ((uiID == -1) || (uiID == 0)) {
            continue;
        }
        res = pCtxMenu->GetCommandString(uiID, GCS_VERBA, NULL, verb, sizeof(verb));
        if (FAILED(res)) {
            continue;
        }
        if (stricmp(verb, "undelete") == 0) {
            uiCommand = uiID;
            break;
        }
    }

    KIO::WorkerResult result = KIO::WorkerResult::pass();

    if (uiCommand != ~0U) {
        CMINVOKECOMMANDINFO cmi;

        memset(&cmi, 0, sizeof(CMINVOKECOMMANDINFO));
        cmi.cbSize = sizeof(CMINVOKECOMMANDINFO);
        cmi.lpVerb = MAKEINTRESOURCEA(uiCommand);
        cmi.fMask = CMIC_MASK_FLAG_NO_UI;
        res = pCtxMenu->InvokeCommand((CMINVOKECOMMANDINFO *)&cmi);

        result = translateError(res);
    }
    DestroyMenu(hmenuCtx);
    pCtxMenu->Release();
    ILFree(pidl);

    return result;
}

KIO::WorkerResult TrashProtocol::clearTrash()
{
    return translateError(SHEmptyRecycleBin(0, 0, 0));
}

KIO::WorkerResult TrashProtocol::rename(const QUrl &oldURL, const QUrl &newURL, KIO::JobFlags flags)
{
    qCDebug(KIO_TRASH) << "TrashProtocol::rename(): old=" << oldURL << " new=" << newURL << " overwrite=" << (flags & KIO::Overwrite);

    if (oldURL.scheme() == QLatin1String("trash") && newURL.scheme() == QLatin1String("trash")) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_RENAME, oldURL.toDisplayString());
    }

    return copyOrMove(oldURL, newURL, (flags & KIO::Overwrite), Move);
}

KIO::WorkerResult TrashProtocol::copy(const QUrl &src, const QUrl &dest, int /*permissions*/, KIO::JobFlags flags)
{
    qCDebug(KIO_TRASH) << "TrashProtocol::copy(): " << src << " " << dest;

    if (src.scheme() == QLatin1String("trash") && dest.scheme() == QLatin1String("trash")) {
        return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, i18n("This file is already in the trash bin."));
    }

    return copyOrMove(src, dest, (flags & KIO::Overwrite), Copy);
}

KIO::WorkerResult TrashProtocol::copyOrMove(const QUrl &src, const QUrl &dest, bool overwrite, CopyOrMove action)
{
    if (src.scheme() == QLatin1String("trash") && dest.isLocalFile()) {
        if (action == Move) {
            return restore(src, dest);
        } else {
            return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, i18n("not supported"));
        }
    } else if (src.isLocalFile() && dest.scheme() == QLatin1String("trash")) {
        UINT op = (action == Move) ? FO_DELETE : FO_COPY;
        if (auto result = doFileOp(src, FO_DELETE, FOF_ALLOWUNDO); !result.success()) {
            return result;
        }
        return KIO::WorkerResult::pass();
    } else {
        return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, i18n("Internal error in copyOrMove, should never happen"));
    }

    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::stat(const QUrl &url)
{
    KIO::UDSEntry entry;
    if (url.path() == QLatin1String("/")) {
        STRRET strret;
        IShellFolder *isfDesktop;
        LPITEMIDLIST iilTrash;

        SHGetFolderLocation(NULL, CSIDL_BITBUCKET, 0, 0, &iilTrash);
        SHGetDesktopFolder(&isfDesktop);
        isfDesktop->BindToObject(iilTrash, NULL, IID_IShellFolder2, (void **)&m_isfTrashFolder);
        isfDesktop->GetDisplayNameOf(iilTrash, SHGDN_NORMAL, &strret);
        isfDesktop->Release();
        ILFree(iilTrash);

        entry.fastInsert(KIO::UDSEntry::UDS_NAME, QString::fromUtf16(reinterpret_cast<const char16_t *>(strret.pOleStr)));
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0700);
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QString::fromLatin1("inode/directory"));
        m_pMalloc->Free(strret.pOleStr);
    } else {
        // TODO: when does this happen?
    }
    statEntry(entry);
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::del(const QUrl &url, bool /*isfile*/)
{
    if (auto result = doFileOp(url, FO_DELETE, 0); !result.success()) {
        return result;
    }
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::listDir(const QUrl &url)
{
    qCDebug(KIO_TRASH) << "TrashProtocol::listDir(): " << url;
    // There are no subfolders in Windows Trash
    return listRoot();
}

KIO::WorkerResult TrashProtocol::listRoot()
{
    IEnumIDList *l;
    HRESULT res = m_isfTrashFolder->EnumObjects(0, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN, &l);
    if (res != S_OK) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, QStringLiteral("fixme!"));
    }

    STRRET strret;
    SFGAOF attribs;
    KIO::UDSEntry entry;
    LPITEMIDLIST i;
    WIN32_FIND_DATAW findData;
    while (l->Next(1, &i, NULL) == S_OK) {
        m_isfTrashFolder->GetDisplayNameOf(i, SHGDN_NORMAL, &strret);
        entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, QString::fromUtf16(reinterpret_cast<const char16_t *>(strret.pOleStr)));
        m_pMalloc->Free(strret.pOleStr);
        m_isfTrashFolder->GetDisplayNameOf(i, SHGDN_FORPARSING | SHGDN_INFOLDER, &strret);
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, QString::fromUtf16(reinterpret_cast<const char16_t *>(strret.pOleStr)));
        m_pMalloc->Free(strret.pOleStr);
        m_isfTrashFolder->GetAttributesOf(1, (LPCITEMIDLIST *)&i, &attribs);
        SHGetDataFromIDList(m_isfTrashFolder, i, SHGDFIL_FINDDATA, &findData, sizeof(findData));
        entry.fastInsert(KIO::UDSEntry::UDS_SIZE, ((quint64)findData.nFileSizeLow) + (((quint64)findData.nFileSizeHigh) << 32));
        entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, filetimeToTime_t(&findData.ftLastWriteTime));
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS_TIME, filetimeToTime_t(&findData.ftLastAccessTime));
        entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, filetimeToTime_t(&findData.ftCreationTime));
        entry.fastInsert(KIO::UDSEntry::UDS_EXTRA, QString::fromUtf16(reinterpret_cast<const char16_t *>(strret.pOleStr)));
        entry.fastInsert(KIO::UDSEntry::UDS_EXTRA + 1, QDateTime().toString(Qt::ISODate));
        mode_t type = QT_STAT_REG;
        if ((attribs & SFGAO_FOLDER) == SFGAO_FOLDER) {
            type = QT_STAT_DIR;
        }
        if ((attribs & SFGAO_LINK) == SFGAO_LINK) {
            type = QT_STAT_LNK;
        }
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, type);
        mode_t access = 0700;
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) == FILE_ATTRIBUTE_READONLY) {
            type = 0300;
        }
        listEntry(entry);

        ILFree(i);
    }
    l->Release();
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::special(const QByteArray &data)
{
    QDataStream stream(data);
    int cmd;
    stream >> cmd;

    switch (cmd) {
    case 1:
        // empty trash folder
        return clearTrash();
    case 2:
        // convert old trash folder (non-windows only)
        return KIO::WorkerResult::pass();
    case 3: {
        QUrl url;
        stream >> url;
        return restore(url, QUrl());
    }
    default:
        qCWarning(KIO_TRASH) << "Unknown command in special(): " << cmd;
        return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, QString::number(cmd));
        break;
    }

    return KIO::WorkerResult::pass();
}

void TrashProtocol::updateRecycleBin()
{
    IEnumIDList *l;
    HRESULT res = m_isfTrashFolder->EnumObjects(0, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN, &l);
    if (res != S_OK) {
        return;
    }

    bool bEmpty = true;
    LPITEMIDLIST i;
    if (l->Next(1, &i, NULL) == S_OK) {
        bEmpty = false;
        ILFree(i);
    }
    KConfigGroup group = m_config.group(QStringLiteral("Status"));
    group.writeEntry("Empty", bEmpty);
    m_config.sync();
    l->Release();
}

KIO::WorkerResult TrashProtocol::put(const QUrl &url, int /*permissions*/, KIO::JobFlags)
{
    qCDebug(KIO_TRASH) << "put: " << url;
    // create deleted file. We need to get the mtime and original location from metadata...
    // Maybe we can find the info file for url.fileName(), in case ::rename() was called first, and failed...
    return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED, url.toDisplayString());
}

KIO::WorkerResult TrashProtocol::get(const QUrl &url)
{
    // TODO
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::doFileOp(const QUrl &url, UINT wFunc, FILEOP_FLAGS fFlags)
{
    const QString path = url.path().replace(QLatin1Char('/'), QLatin1Char('\\'));
    // must be double-null terminated.
    QByteArray delBuf((path.length() + 2) * 2, 0);
    memcpy(delBuf.data(), path.utf16(), path.length() * 2);

    SHFILEOPSTRUCTW op;
    memset(&op, 0, sizeof(SHFILEOPSTRUCTW));
    op.wFunc = wFunc;
    op.pFrom = (LPCWSTR)delBuf.constData();
    op.fFlags = fFlags | FOF_NOCONFIRMATION | FOF_NOERRORUI;
    return translateError(SHFileOperationW(&op));
}

KIO::WorkerResult TrashProtocol::translateError(HRESULT hRes)
{
    // TODO!
    if (FAILED(hRes)) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, QLatin1String("fixme!"));
    }
    return KIO::WorkerResult::pass();
}

#include "kio_trash_win.moc"

#include "moc_kio_trash_win.cpp"
