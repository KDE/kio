#ifndef PREVIEWTEST_H
#define PREVIEWTEST_H

#include <QWidget>
#include <kio/job.h>

class QLineEdit;
class QLabel;
class KFileItem;

class PreviewTest : public QWidget
{
    Q_OBJECT
public:
    PreviewTest();

private Q_SLOTS:
    void slotGenerate();
    void slotResult(KJob *);
    void slotPreview(const KFileItem &, const QPixmap &);
    void slotFailed();

private:
    QLineEdit *m_url;
    QLineEdit *m_plugins;
    QLabel *m_preview;
};

#endif

