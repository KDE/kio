#ifndef NETPREF_H
#define NETPREF_H

#include <kcmodule.h>

class QGroupBox;
class QCheckBox;

class KPluralHandlingSpinBox;

class KIOPreferences : public KCModule
{
    Q_OBJECT

public:
    KIOPreferences(QWidget *parent, const QVariantList &args);
    ~KIOPreferences();

    void load() Q_DECL_OVERRIDE;
    void save() Q_DECL_OVERRIDE;
    void defaults() Q_DECL_OVERRIDE;

    QString quickHelp() const Q_DECL_OVERRIDE;

protected Q_SLOTS:
    void configChanged() { emit changed(true); }

private:
    QGroupBox* gb_Ftp;
    QGroupBox* gb_Timeout;
    QCheckBox* cb_ftpEnablePasv;
    QCheckBox* cb_ftpMarkPartial;

    KPluralHandlingSpinBox* sb_socketRead;
    KPluralHandlingSpinBox* sb_proxyConnect;
    KPluralHandlingSpinBox* sb_serverConnect;
    KPluralHandlingSpinBox* sb_serverResponse;
};

#endif // NETPREF_H
