
#include "previewtest.h"

#include <QLabel>
#include <QApplication>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>

#include <QDebug>
#include <KConfigGroup>
#include <KIconLoader>
#include <KSharedConfig>
#include <kio/previewjob.h>

PreviewTest::PreviewTest()
    : QWidget()
{
    QGridLayout *layout = new QGridLayout(this);
    m_url = new QLineEdit(this);

    QString path;
    KIconLoader().loadMimeTypeIcon(QStringLiteral("video-x-generic"), KIconLoader::Desktop, 256,
                                   KIconLoader::DefaultState, QStringList(),
                                   &path);

    m_url->setText(path);
    layout->addWidget(m_url, 0, 0);
    QPushButton *btn = new QPushButton(QStringLiteral("Generate"), this);
    connect(btn, &QAbstractButton::clicked, this, &PreviewTest::slotGenerate);
    layout->addWidget(btn, 0, 1);

    const KConfigGroup globalConfig(KSharedConfig::openConfig(), "PreviewSettings");
    const QStringList enabledPlugins = globalConfig.readEntry("Plugins", QStringList()
                            << QStringLiteral("directorythumbnail")
                            << QStringLiteral("imagethumbnail")
                            << QStringLiteral("jpegthumbnail"));

    m_plugins = new QLineEdit(this);
    layout->addWidget(m_plugins, 1, 0, 1, 2);
    m_plugins->setText(enabledPlugins.join("; "));

    m_preview = new QLabel(this);
    m_preview->setMinimumSize(400, 300);
    layout->addWidget(m_preview, 2, 0, 1, 2);
}

void PreviewTest::slotGenerate()
{
    KFileItemList items;
    items.append(KFileItem(QUrl::fromUserInput(m_url->text())));

    QStringList enabledPlugins;
    const QStringList splittedText = m_plugins->text().split(';');
    for (const QString &plugin : splittedText)
        enabledPlugins << plugin.trimmed();

    KIO::PreviewJob *job = KIO::filePreview(items, QSize(m_preview->width(), m_preview->height()), &enabledPlugins);
    connect(job, &KJob::result, this, &PreviewTest::slotResult);
    connect(job, &KIO::PreviewJob::gotPreview, this, &PreviewTest::slotPreview);
    connect(job, &KIO::PreviewJob::failed, this, &PreviewTest::slotFailed);
}

void PreviewTest::slotResult(KJob *)
{
    qDebug() << "PreviewTest::slotResult(...)";
}

void PreviewTest::slotPreview(const KFileItem &, const QPixmap &pix)
{
    qDebug() << "PreviewTest::slotPreview()";
    m_preview->setPixmap(pix);
}

void PreviewTest::slotFailed()
{
    qDebug() << "PreviewTest::slotFailed()";
    m_preview->setText(QStringLiteral("failed"));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    PreviewTest *w = new PreviewTest;
    w->show();
    return app.exec();
}

