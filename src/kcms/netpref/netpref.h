#ifndef NETPREF_H
#define NETPREF_H

#include <KCModule>

class QGroupBox;
class QCheckBox;

class KPluralHandlingSpinBox;

class KIOPreferences : public KCModule
{
    Q_OBJECT

public:
    KIOPreferences(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
    ~KIOPreferences() override;

    void load() override;
    void save() override;
    void defaults() override;

protected Q_SLOTS:
    void configChanged()
    {
        setNeedsSave(true);
    }

private:
    QGroupBox *gb_Ftp;
    QGroupBox *gb_Timeout;
    QCheckBox *cb_globalMarkPartial;
    KPluralHandlingSpinBox *sb_globalMinimumKeepSize;
    QCheckBox *cb_ftpEnablePasv;
    QCheckBox *cb_ftpMarkPartial;

    KPluralHandlingSpinBox *sb_socketRead;
    KPluralHandlingSpinBox *sb_proxyConnect;
    KPluralHandlingSpinBox *sb_serverConnect;
    KPluralHandlingSpinBox *sb_serverResponse;
};

#endif // NETPREF_H
