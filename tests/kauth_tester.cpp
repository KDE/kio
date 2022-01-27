/*
    SPDX-FileCopyrightText: 2022 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#include <QApplication>
#include <QDebug>
#include <QDir>

#include <KIO/FileUndoManager>
#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KIO/SimpleJob>
#include <KIO/MkdirJob>

#include <cstdlib>
#include <optional>
#include <variant>

#define Fum KIO::FileUndoManager::self()

template<typename T>
class Future
{
    struct Shared {
        std::optional<T> ret = {};
        std::function<void(T)> next = nullptr;
    };
    QSharedPointer<Shared> d;

public:
    using Kind = T;

    Future()
    {
        d.reset(new Shared);
    }
    Future(const Future& other)
    {
        d = other.d;
    }
    ~Future()
    {
    }

    void succeed(const QString& desc, T value)
    {
        qDebug() << "Task succeeded!" << desc << value;

        if (d->ret.has_value())
            return;

        d->ret = value;
        if (d->next) {
            d->next(value);
        }
    }
    void fail(const QString& desc, T)
    {
        qWarning() << "Task failed: " << desc;
        qFatal("Aborting...");
    }
    void then(std::function<void(T)> next)
    {
        d->next = next;
        if (d->ret.has_value()) {
            next(d->ret.value());
        }
    }
    template<typename Function>
    Future<typename std::result_of_t<Function(T)>::Kind>
    andThen(Function next)
    {
        using X = typename std::result_of_t<Function(T)>::Kind;
        Future<X> outerFuture;

        then([next, outerFuture](T val) mutable {
            Future<X> innerFuture = next(val);
            innerFuture.then([outerFuture](X val) mutable {
                outerFuture.succeed("andThen", val);
            });
        });

        return outerFuture;
    }
};

template<typename T>
Future<T> jobToFuture(QString desc, T job)
{
    Future<T> ret;

    QObject::connect(job, &KIO::Job::finished, job, [desc, ret, job]() mutable {
        KIO::Job* it = job;
        if (it->error() == KIO::Job::NoError) {
            ret.succeed(desc, job);
        } else {
            ret.fail(desc, job);
        }
    });

    return ret;
}

template<typename T>
QString toStr(const T& value) {
    QString it;
    QDebug(&it) << value;
    return it;
}

auto move(const QList<QUrl>& items, const QUrl& to)
{
    auto job = KIO::move(items, to);

    Fum->recordCopyJob(job);

    return jobToFuture(QStringLiteral("move %1 to %2").arg(toStr(items)).arg(toStr(to)), job);
}

auto copy(const QList<QUrl>& items, const QUrl& to)
{
    auto job = KIO::move(items, to);

    Fum->recordCopyJob(job);

    return jobToFuture(QStringLiteral("copy %1 to %2").arg(toStr(items)).arg(toStr(to)), job);
}

auto doRename(const QUrl& from, const QUrl& to)
{
    auto job = KIO::rename(from, to);

    Fum->recordJob(KIO::FileUndoManager::Rename, {from}, to, job);

    return jobToFuture(QStringLiteral("rename %1 to %2").arg(toStr(from)).arg(toStr(to)), job);
}

auto makeFolderExist(const QUrl& at)
{
    auto job = KIO::mkdir(at);

    Fum->recordJob(KIO::FileUndoManager::Mkdir, {}, at, job);

    return jobToFuture(QStringLiteral("make directory at %1").arg(toStr(at)), job);
}

auto doTrash(const QList<QUrl>& items)
{
    auto job = KIO::trash(items);

    Fum->recordJob(KIO::FileUndoManager::Trash, items, QUrl("trash:/"), job);

    return jobToFuture(QStringLiteral("trash %1").arg(toStr(items)), job);
}

auto doDelete(const QList<QUrl>& items)
{
    auto job = KIO::del(items);

    return jobToFuture(QStringLiteral("delete %1").arg(toStr(items)), job);
}

auto init()
{
    system(R"(
echo "Making some directories for the test..."

rm -rf ~/.cache/kio-test-files
mkdir -p ~/.cache/kio-test-files

echo "hello world" > ~/.cache/kio-test-files/1
echo "hello world" > ~/.cache/kio-test-files/2
echo "hello world" > ~/.cache/kio-test-files/3
echo "hello world" > ~/.cache/kio-test-files/4
echo "hello world" > ~/.cache/kio-test-files/5
echo "hello world" > ~/.cache/kio-test-files/6
echo "hello world" > ~/.cache/kio-test-files/7
echo "hello world" > ~/.cache/kio-test-files/8
echo "hello world" > ~/.cache/kio-test-files/9
echo "hello world" > ~/.cache/kio-test-files/10

mkdir -p ~/.cache/kio-test-files/more-files

echo "hello world" > ~/.cache/kio-test-files/more-files/1
echo "hello world" > ~/.cache/kio-test-files/more-files/2
echo "hello world" > ~/.cache/kio-test-files/more-files/3
echo "hello world" > ~/.cache/kio-test-files/more-files/4
echo "hello world" > ~/.cache/kio-test-files/more-files/5
echo "hello world" > ~/.cache/kio-test-files/more-files/6
echo "hello world" > ~/.cache/kio-test-files/more-files/7
echo "hello world" > ~/.cache/kio-test-files/more-files/8
echo "hello world" > ~/.cache/kio-test-files/more-files/9
echo "hello world" > ~/.cache/kio-test-files/more-files/10

sudo rm -rf /.kio-test-files/
sudo mkdir -p /.kio-test-files/

echo "Made them!"
    )");
}

// prefixes the given string with home path
auto h(const QString& it)
{
    auto home = QDir::homePath();
    auto path = it;

    return QUrl::fromLocalFile(QDir::cleanPath(home + QDir::separator() + path));
}

// turns the given string into a file
auto u(const QString& it)
{
    return QUrl::fromLocalFile(it);
}

auto undo()
{
    Future<bool> dummy;

    QObject::connect(Fum, &KIO::FileUndoManager::undoJobFinished, Fum, [dummy]() mutable {
        dummy.succeed("undo", true);
    });

    Fum->undo();

    return dummy;
}

QUrl operator "" _hc(const char* it, size_t len)
{
    return h(QStringLiteral("/.cache/kio-test-files/%1").arg(QString::fromLocal8Bit(it, len)));
}

QUrl operator "" _rc(const char* it, size_t len)
{
    return u(QStringLiteral("/.kio-test-files/%1").arg(QString::fromLocal8Bit(it, len)));
}

#define doUndo [](auto) { return undo(); }
#define do [](auto)

auto doTheTestOperations()
{

    // Single file

    // Drag-and-drop move to /
    move({"1"_hc}, "1"_rc)
    // Undo dnd move to /
    .andThen(doUndo)
    // Drag-and-drop copy to /
    .andThen(do {
        return copy({"2"_hc}, "2"_rc);
    })
    // Undo dnd copy one file to /
    .andThen(doUndo)
    // Cut-and-paste to /
    .andThen(do {
        return move({"3"_hc}, "3"_rc);
    })
    // Undo cut-and-paste to /
    .andThen(doUndo)
    // Copy-and-paste to /
    .andThen(do {
        return copy({"4"_hc}, "4"_rc);
    })
    // Undo copy-and-paste to /
    .andThen(doUndo)
    // Rename file on /
    .andThen(do {
        return doRename({"4"_rc}, "4!"_rc);
    })
    // Duplicate file on /
    .andThen(do {
        return copy({"4!"_rc}, "4! copy"_rc);
    })
    // Undo duplication of a file on /
    .andThen(doUndo)


    // Four individual files


    // Drag-and-drop move to /
    .andThen(do {
        return move({"5"_hc, "6"_hc, "7"_hc, "8"_hc}, ""_rc);
    })
    // Undo dnd move to /
    .andThen(doUndo)
    // Copy-and-paste to /
    .andThen(do {
        return copy({"5"_hc, "6"_hc, "7"_hc, "8"_hc}, ""_rc);
    })
    // Undo copy-and-paste to /
    .andThen(doUndo)
    // Cut-and-paste to /
    .andThen(do {
        return move({"5"_hc, "6"_hc, "7"_hc, "8"_hc}, ""_rc);
    })
    // Undo cut-and-paste to /
    .andThen(doUndo)
    // Rename four files on /
    // TODO: figure out this
    // Duplicate four files on /
    // Undo duplicate four files on /
    // TODO: figure out this


    // Single Folder full of stuff


    // Drag-and-drop move to /
    .andThen(do {
        return move({"more-files"_hc}, ""_rc);
    })
    // Undo Drag-and-drop move to /
    .andThen(doUndo)
    // Drag-and-drop copy to /
    .andThen(do {
        return copy({"more-files"_hc}, ""_rc);
    })
    // Undo drag-and-drop copy to /
    .andThen(doUndo)
    // Copy again
    .andThen(do {
        return copy({"more-files"_hc}, ""_rc);
    })
    // Rename folder full of stuff on /
    .andThen(do {
        return doRename("more-files"_rc, "filesier"_rc);
    })
    // Duplicate folder full of stuff on /


    // Miscellaneous


    // Create folder on /
    .andThen(do {
        return makeFolderExist("idk"_rc);
    })
    // Undo creating folder on /
    .andThen(doUndo);
    // Create a file on /
    // Undo creating a file on /
    // Edit permissions for file on /
}

int main(int argc, char *argv[])
{
    init();

    QApplication::setApplicationName("KAuth Tester");
    QApplication k(argc, argv);

    doTheTestOperations();

    return k.exec();
}
