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
        Future<X> ret;

        then([next, ret](T wert) mutable {
            Future<X> bret = next(wert);
            bret.then([ret](X wert) mutable {
                ret.succeed("andThen", wert);
            });
        });

        return ret;
    }
};

template<typename T>
Future<T> f(QString desc, T job)
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
QString ss(const T& value) {
    QString it;
    QDebug(&it) << value;
    return it;
}

auto move(const QList<QUrl>& items, const QUrl& to)
{
    auto job = KIO::move(items, to);

    Fum->recordCopyJob(job);

    return f(QStringLiteral("move %1 to %2").arg(ss(items)).arg(ss(to)), job);
}

auto copy(const QList<QUrl>& items, const QUrl& to)
{
    auto job = KIO::move(items, to);

    Fum->recordCopyJob(job);

    return f(QStringLiteral("copy %1 to %2").arg(ss(items)).arg(ss(to)), job);
}

auto doRename(const QUrl& from, const QUrl& to)
{
    auto job = KIO::rename(from, to);

    Fum->recordJob(KIO::FileUndoManager::Rename, {from}, to, job);

    return f(QStringLiteral("rename %1 to %2").arg(ss(from)).arg(ss(to)), job);
}

auto makeFolderExist(const QUrl& at)
{
    auto job = KIO::mkdir(at);

    Fum->recordJob(KIO::FileUndoManager::Mkdir, {}, at, job);

    return f(QStringLiteral("make directory at %1").arg(ss(at)), job);
}

auto yeet(const QList<QUrl>& items)
{
    auto job = KIO::trash(items);

    Fum->recordJob(KIO::FileUndoManager::Trash, items, QUrl("trash:/"), job);

    return f(QStringLiteral("trash %1").arg(ss(items)), job);
}

auto strongYeet(const QList<QUrl>& items)
{
    auto job = KIO::del(items);

    return f(QStringLiteral("delete %1").arg(ss(items)), job);
}

auto init()
{
    system(R"(
echo "Making some directories for the test..."

rm -rf ~/.cache/kio-test-barf
mkdir -p ~/.cache/kio-test-barf

echo "mald" > ~/.cache/kio-test-barf/1
echo "mald" > ~/.cache/kio-test-barf/2
echo "mald" > ~/.cache/kio-test-barf/3
echo "mald" > ~/.cache/kio-test-barf/4
echo "mald" > ~/.cache/kio-test-barf/5
echo "mald" > ~/.cache/kio-test-barf/6
echo "mald" > ~/.cache/kio-test-barf/7
echo "mald" > ~/.cache/kio-test-barf/8
echo "mald" > ~/.cache/kio-test-barf/9
echo "mald" > ~/.cache/kio-test-barf/10

mkdir -p ~/.cache/kio-test-barf/more-barf

echo "mald" > ~/.cache/kio-test-barf/more-barf/1
echo "mald" > ~/.cache/kio-test-barf/more-barf/2
echo "mald" > ~/.cache/kio-test-barf/more-barf/3
echo "mald" > ~/.cache/kio-test-barf/more-barf/4
echo "mald" > ~/.cache/kio-test-barf/more-barf/5
echo "mald" > ~/.cache/kio-test-barf/more-barf/6
echo "mald" > ~/.cache/kio-test-barf/more-barf/7
echo "mald" > ~/.cache/kio-test-barf/more-barf/8
echo "mald" > ~/.cache/kio-test-barf/more-barf/9
echo "mald" > ~/.cache/kio-test-barf/more-barf/10

sudo rm -rf /.kio-test-barf/
sudo mkdir -p /.kio-test-barf/

echo "Made them!"
    )");
}

auto h(const QString& it)
{
    auto home = QDir::homePath();
    auto path = it;

    return QUrl::fromLocalFile(QDir::cleanPath(home + QDir::separator() + path));
}

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
    return h(QStringLiteral("/.cache/kio-test-barf/%1").arg(QString::fromLocal8Bit(it, len)));
}

QUrl operator "" _rc(const char* it, size_t len)
{
    return u(QStringLiteral("/.kio-test-barf/%1").arg(QString::fromLocal8Bit(it, len)));
}

#define doUndo [](auto) { return undo(); }
#define do [](auto)

auto doit()
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
        return move({"more-barf"_hc}, ""_rc);
    })
    // Undo Drag-and-drop move to /
    .andThen(doUndo)
    // Drag-and-drop copy to /
    .andThen(do {
        return copy({"more-barf"_hc}, ""_rc);
    })
    // Undo drag-and-drop copy to /
    .andThen(doUndo)
    // Copy again
    .andThen(do {
        return copy({"more-barf"_hc}, ""_rc);
    })
    // Rename folder full of stuff on /
    .andThen(do {
        return doRename("more-barf"_rc, "barfier"_rc);
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

    doit();

    return k.exec();
}
