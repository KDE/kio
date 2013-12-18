#ifndef PREVIEWTEST_H
#define PREVIEWTEST_H

#include <qwidget.h>
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
    void slotPreview( const KFileItem&, const QPixmap & );
    void slotFailed();

private:
    QLineEdit *m_url;
    QLabel *m_preview;
};

#endif

