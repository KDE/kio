/*
   kproxydlg.cpp - Proxy configuration dialog

   Copyright (C) 2001, 2011 Dawit Alemayehu <adawit@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License (GPL) version 2 as published by the Free Software
   Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

// Own
#include "kproxydlg.h"

// Local
#include "ksaveioconfig.h"

// KDE
#include <kpluginfactory.h>
#include <KLineEdit>
#include <KLocalizedString>
#include <kmessagebox.h>
#include <kurifilter.h>
#include <QUrl>

// Qt
#include <QSpinBox>


#define QL1C(x)         QLatin1Char(x)
#define QL1S(x)         QLatin1String(x)

#define ENV_HTTP_PROXY    QStringLiteral("HTTP_PROXY,http_proxy,HTTPPROXY,httpproxy,PROXY,proxy")
#define ENV_HTTPS_PROXY   QStringLiteral("HTTPS_PROXY,https_proxy,HTTPSPROXY,httpsproxy,PROXY,proxy")
#define ENV_FTP_PROXY     QStringLiteral("FTP_PROXY,ftp_proxy,FTPPROXY,ftpproxy,PROXY,proxy")
#define ENV_SOCKS_PROXY   QStringLiteral("SOCKS_PROXY,socks_proxy,SOCKSPROXY,socksproxy,PROXY,proxy")
#define ENV_NO_PROXY      QStringLiteral("NO_PROXY,no_proxy")

K_PLUGIN_FACTORY_DECLARATION (KioConfigFactory)


class InputValidator : public QValidator
{
    
public:
    State validate(QString& input, int& pos) const Q_DECL_OVERRIDE {
        if (input.isEmpty())
            return Acceptable;

        const QChar ch = input.at((pos > 0 ? pos - 1 : pos));
        if (ch.isSpace())
            return Invalid;

        return Acceptable;
    }
};


static QString manualProxyToText(const QLineEdit* edit, const QSpinBox* spinBox, const QChar& separator)
{
    QString value;

    value = edit->text();
    value += separator;
    value +=  QString::number(spinBox->value());

    return value;
}

static void setManualProxyFromText(const QString& value, QLineEdit* edit, QSpinBox* spinBox)
{
    if (value.isEmpty())
        return;

    const QStringList values = value.split(' ');
    edit->setText(values.at(0));
    bool ok = false;
    const int num = values.at(1).toInt(&ok);
    if (ok) {
        spinBox->setValue(num);
    }
}

static void showSystemProxyUrl(QLineEdit* edit, QString* value)
{
    Q_ASSERT(edit);
    Q_ASSERT(value);

    *value = edit->text();
    edit->setEnabled(false);
    const QByteArray envVar(edit->text().toUtf8());
    edit->setText(QString::fromUtf8(qgetenv(envVar.constData())));
}

static QString proxyUrlFromInput(KProxyDialog::DisplayUrlFlags* flags,
                                 const QLineEdit* edit, const QSpinBox* spinBox,
                                 const QString& defaultScheme = QString(),
                                 KProxyDialog::DisplayUrlFlag flag = KProxyDialog::HideNone)
{
    Q_ASSERT(edit);
    Q_ASSERT(spinBox);

    QString proxyStr;

    if (edit->text().isEmpty())
        return proxyStr;

    if (flags && !edit->text().contains(QL1S("://"))) {
        *flags |= flag;
    }

    KUriFilterData data;
    data.setData(edit->text());
    data.setCheckForExecutables(false);
    if (!defaultScheme.isEmpty()) {
        data.setDefaultUrlScheme(defaultScheme);
    }

    if (KUriFilter::self()->filterUri(data, QStringList() << QStringLiteral("kshorturifilter"))) {
        QUrl url = data.uri();
        const int portNum = (spinBox->value() > 0 ? spinBox->value() : url.port());
        url.setPort(-1);

        proxyStr = url.url();
        proxyStr += QL1C(' ');
        if (portNum > -1) {
            proxyStr += QString::number(portNum);
        }
    } else {
        proxyStr = edit->text();
        if (spinBox->value() > 0) {
            proxyStr += QL1C(' ');
            proxyStr += QString::number(spinBox->value());
        }
    }

    return proxyStr;
}

static void setProxyInformation(const QString& value,
                                int proxyType,
                                QLineEdit* manEdit,
                                QLineEdit* sysEdit,
                                QSpinBox* spinBox ,
                                const QString& defaultScheme,
                                KProxyDialog::DisplayUrlFlag flag)
{
    const bool isSysProxy = (!value.contains(QL1C(' ')) &&
                             !value.contains(QL1C('.')) &&
                             !value.contains(QL1C(',')) &&
                             !value.contains(QL1C(':')));

    if (proxyType == KProtocolManager::EnvVarProxy || isSysProxy) {
#if defined(Q_OS_LINUX) || defined (Q_OS_UNIX)
        sysEdit->setText(value);
#endif
        return;
    }

    if (spinBox) {
        QString urlStr;
        int portNum = -1;
        int index = value.lastIndexOf(QL1C(' '));
        if (index == -1)
            index = value.lastIndexOf(QL1C(':'));

        if (index > 0) {
            bool ok = false;
            portNum = value.midRef(index+1).toInt(&ok);
            if (!ok) {
                portNum = -1;
            }
            urlStr = value.left(index).trimmed();
        } else {
            urlStr = value.trimmed();
        }

        KUriFilterData data;
        data.setData(urlStr);
        data.setCheckForExecutables(false);
        if (!defaultScheme.isEmpty()) {
            data.setDefaultUrlScheme(defaultScheme);
        }

        if (KUriFilter::self()->filterUri(data, QStringList() << QStringLiteral("kshorturifilter"))) {
            QUrl url (data.uri());
            if (portNum == -1 && url.port() > -1) {
                portNum = url.port();
            }

            url.setPort(-1);
            url.setUserName(QString());
            url.setPassword(QString());
            url.setPath(QString());

            manEdit->setText(((KSaveIOConfig::proxyDisplayUrlFlags() & flag) ? url.host(): url.url()));
        } else {
            QUrl url(urlStr);
            if (KSaveIOConfig::proxyDisplayUrlFlags() & flag) {
                manEdit->setText((url.scheme().isEmpty()) ? url.path() : url.host());
            } else {
                manEdit->setText(urlStr);
            }
        }

        if (spinBox && portNum > -1) {
            spinBox->setValue(portNum);
        }
        return;
    }

    manEdit->setText(value); // Manual proxy exception...
}

KProxyDialog::KProxyDialog(QWidget* parent, const QVariantList& args)
    : KCModule(/*KioConfigFactory::componentData(),*/ parent)
{
    Q_UNUSED(args);
    mUi.setupUi(this);

    mUi.systemProxyGroupBox->setVisible(false);
    mUi.manualProxyGroupBox->setVisible(false);
    mUi.autoDetectButton->setVisible(false);
    mUi.proxyConfigScriptGroupBox->setVisible(false);

    InputValidator* v = new InputValidator;
    mUi.proxyScriptUrlRequester->lineEdit()->setValidator(v);
    mUi.manualProxyHttpEdit->setValidator(v);
    mUi.manualProxyHttpsEdit->setValidator(v);
    mUi.manualProxyFtpEdit->setValidator(v);
    mUi.manualProxySocksEdit->setValidator(v);
    mUi.manualNoProxyEdit->setValidator(v);

#if defined(Q_OS_LINUX) || defined (Q_OS_UNIX)
    connect(mUi.systemProxyRadioButton, SIGNAL(toggled(bool)), mUi.systemProxyGroupBox, SLOT(setVisible(bool)));
#else
    mUi.autoDetectButton->setVisible(false);
    connect(mUi.systemProxyRadioButton, SIGNAL(clicked()), SLOT(slotChanged()));
#endif

    // signals and slots connections
    connect(mUi.noProxyRadioButton, SIGNAL(clicked()), SLOT(slotChanged()));
    connect(mUi.autoDiscoverProxyRadioButton, SIGNAL(clicked()), SLOT(slotChanged()));
    connect(mUi.autoScriptProxyRadioButton, SIGNAL(clicked()), SLOT(slotChanged()));
    connect(mUi.manualProxyRadioButton, SIGNAL(clicked()), SLOT(slotChanged()));
    connect(mUi.systemProxyRadioButton, SIGNAL(clicked()), SLOT(slotChanged()));
    connect(mUi.noProxyRadioButton, SIGNAL(clicked()), SLOT(slotChanged()));
    connect(mUi.useReverseProxyCheckBox, SIGNAL(clicked()), SLOT(slotChanged()));
    connect(mUi.useSameProxyCheckBox, SIGNAL(clicked()), SLOT(slotChanged()));

    connect(mUi.proxyScriptUrlRequester, SIGNAL(textChanged(QString)), SLOT(slotChanged()));

    connect(mUi.manualProxyHttpEdit, SIGNAL(textChanged(QString)), SLOT(slotChanged()));
    connect(mUi.manualProxyHttpsEdit, SIGNAL(textChanged(QString)), SLOT(slotChanged()));
    connect(mUi.manualProxyFtpEdit, SIGNAL(textChanged(QString)), SLOT(slotChanged()));
    connect(mUi.manualProxySocksEdit, SIGNAL(textChanged(QString)), SLOT(slotChanged()));
    connect(mUi.manualNoProxyEdit, SIGNAL(textChanged(QString)), SLOT(slotChanged()));

    connect(mUi.manualProxyHttpSpinBox, SIGNAL(valueChanged(int)), SLOT(slotChanged()));
    connect(mUi.manualProxyHttpsSpinBox, SIGNAL(valueChanged(int)), SLOT(slotChanged()));
    connect(mUi.manualProxyFtpSpinBox, SIGNAL(valueChanged(int)), SLOT(slotChanged()));
    connect(mUi.manualProxySocksSpinBox, SIGNAL(valueChanged(int)), SLOT(slotChanged()));

    connect(mUi.systemProxyHttpEdit, SIGNAL(textEdited(QString)), SLOT(slotChanged()));
    connect(mUi.systemProxyHttpsEdit, SIGNAL(textEdited(QString)), SLOT(slotChanged()));
    connect(mUi.systemProxyFtpEdit, SIGNAL(textEdited(QString)), SLOT(slotChanged()));
    connect(mUi.systemProxySocksEdit, SIGNAL(textEdited(QString)), SLOT(slotChanged()));
    connect(mUi.systemNoProxyEdit, SIGNAL(textEdited(QString)), SLOT(slotChanged()));
}

KProxyDialog::~KProxyDialog()
{
}

void KProxyDialog::load()
{
    mProxyMap[QStringLiteral("HttpProxy")] = KProtocolManager::proxyFor(QStringLiteral("http"));
    mProxyMap[QStringLiteral("HttpsProxy")] = KProtocolManager::proxyFor(QStringLiteral("https"));
    mProxyMap[QStringLiteral("FtpProxy")] = KProtocolManager::proxyFor(QStringLiteral("ftp"));
    mProxyMap[QStringLiteral("SocksProxy")] = KProtocolManager::proxyFor(QStringLiteral("socks"));
    mProxyMap[QStringLiteral("ProxyScript")] = KProtocolManager::proxyConfigScript();
    mProxyMap[QStringLiteral("NoProxy")] = KSaveIOConfig::noProxyFor();

    const int proxyType = KProtocolManager::proxyType();

    // Make sure showEnvValueCheckBox is unchecked before setting proxy env var names
    mUi.showEnvValueCheckBox->setChecked(false);

    setProxyInformation(mProxyMap.value(QStringLiteral("HttpProxy")), proxyType, mUi.manualProxyHttpEdit, mUi.systemProxyHttpEdit, mUi.manualProxyHttpSpinBox, QStringLiteral("http"), HideHttpUrlScheme);
    setProxyInformation(mProxyMap.value(QStringLiteral("HttpsProxy")), proxyType, mUi.manualProxyHttpsEdit, mUi.systemProxyHttpsEdit, mUi.manualProxyHttpsSpinBox, QStringLiteral("http"), HideHttpsUrlScheme);
    setProxyInformation(mProxyMap.value(QStringLiteral("FtpProxy")), proxyType, mUi.manualProxyFtpEdit, mUi.systemProxyFtpEdit, mUi.manualProxyFtpSpinBox, QStringLiteral("ftp"), HideFtpUrlScheme);
    setProxyInformation(mProxyMap.value(QStringLiteral("SocksProxy")), proxyType, mUi.manualProxySocksEdit, mUi.systemProxySocksEdit, mUi.manualProxySocksSpinBox, QStringLiteral("socks"), HideSocksUrlScheme);
    setProxyInformation(mProxyMap.value(QStringLiteral("NoProxy")), proxyType, mUi.manualNoProxyEdit, mUi.systemNoProxyEdit, nullptr, QString(), HideNone);

    // Check the "Use this proxy server for all protocols" if all the proxy URLs are the same...
    const QString httpProxy(mUi.manualProxyHttpEdit->text());
    if (!httpProxy.isEmpty()) {
        const int httpProxyPort = mUi.manualProxyHttpSpinBox->value();
        mUi.useSameProxyCheckBox->setChecked(httpProxy == mUi.manualProxyHttpsEdit->text() &&
                                             httpProxy == mUi.manualProxyFtpEdit->text() &&
                                             httpProxy == mUi.manualProxySocksEdit->text() &&
                                             httpProxyPort ==  mUi.manualProxyHttpsSpinBox->value() &&
                                             httpProxyPort == mUi.manualProxyFtpSpinBox->value() &&
                                             httpProxyPort == mUi.manualProxySocksSpinBox->value());
    }

    // Validate and Set the automatic proxy configuration script url.
    QUrl u (mProxyMap.value(QStringLiteral("ProxyScript")));
    if (u.isValid() && !u.isEmpty()) {
        u.setUserName (QString());
        u.setPassword (QString());
        mUi.proxyScriptUrlRequester->setUrl(u);
    }

    // Set use reverse proxy checkbox...
    mUi.useReverseProxyCheckBox->setChecked((!mProxyMap.value(QStringLiteral("NoProxy")).isEmpty()
                                              && KProtocolManager::useReverseProxy()));

    switch (proxyType) {
    case KProtocolManager::WPADProxy:
        mUi.autoDiscoverProxyRadioButton->setChecked(true);
        break;
    case KProtocolManager::PACProxy:
        mUi.autoScriptProxyRadioButton->setChecked(true);
        break;
    case KProtocolManager::ManualProxy:
        mUi.manualProxyRadioButton->setChecked(true);
        break;
    case KProtocolManager::EnvVarProxy:
        mUi.systemProxyRadioButton->setChecked(true);
        break;
    case KProtocolManager::NoProxy:
    default:
        mUi.noProxyRadioButton->setChecked(true);
        break;
    }
}

static bool isPACProxyType(KProtocolManager::ProxyType type)
{
    return (type == KProtocolManager::PACProxy || type == KProtocolManager::WPADProxy);
}

void KProxyDialog::save()
{
    const KProtocolManager::ProxyType lastProxyType = KProtocolManager::proxyType();
    KProtocolManager::ProxyType proxyType = KProtocolManager::NoProxy;
    DisplayUrlFlags displayUrlFlags = static_cast<DisplayUrlFlags>(KSaveIOConfig::proxyDisplayUrlFlags());

    if (mUi.manualProxyRadioButton->isChecked()) {
        DisplayUrlFlags flags = HideNone;
        proxyType = KProtocolManager::ManualProxy;
        mProxyMap[QStringLiteral("HttpProxy")] = proxyUrlFromInput(&flags, mUi.manualProxyHttpEdit, mUi.manualProxyHttpSpinBox, QStringLiteral("http"), HideHttpUrlScheme);
        mProxyMap[QStringLiteral("HttpsProxy")] = proxyUrlFromInput(&flags, mUi.manualProxyHttpsEdit, mUi.manualProxyHttpsSpinBox, QStringLiteral("http"), HideHttpsUrlScheme);
        mProxyMap[QStringLiteral("FtpProxy")] = proxyUrlFromInput(&flags, mUi.manualProxyFtpEdit, mUi.manualProxyFtpSpinBox, QStringLiteral("ftp"), HideFtpUrlScheme);
        mProxyMap[QStringLiteral("SocksProxy")] = proxyUrlFromInput(&flags, mUi.manualProxySocksEdit, mUi.manualProxySocksSpinBox, QStringLiteral("socks"), HideSocksUrlScheme);
        mProxyMap[QStringLiteral("NoProxy")] = mUi.manualNoProxyEdit->text();
        displayUrlFlags = flags;
    } else if (mUi.systemProxyRadioButton->isChecked()) {
        proxyType = KProtocolManager::EnvVarProxy;
        if (!mUi.showEnvValueCheckBox->isChecked()) {
            mProxyMap[QStringLiteral("HttpProxy")] = mUi.systemProxyHttpEdit->text();
            mProxyMap[QStringLiteral("HttpsProxy")] = mUi.systemProxyHttpsEdit->text();
            mProxyMap[QStringLiteral("FtpProxy")] = mUi.systemProxyFtpEdit->text();
            mProxyMap[QStringLiteral("SocksProxy")] = mUi.systemProxySocksEdit->text();
            mProxyMap[QStringLiteral("NoProxy")] = mUi.systemNoProxyEdit->text();
        }
        else {
            mProxyMap[QStringLiteral("HttpProxy")] = mProxyMap.take(mUi.systemProxyHttpEdit->objectName());
            mProxyMap[QStringLiteral("HttpsProxy")] = mProxyMap.take(mUi.systemProxyHttpsEdit->objectName());
            mProxyMap[QStringLiteral("FtpProxy")] = mProxyMap.take(mUi.systemProxyFtpEdit->objectName());
            mProxyMap[QStringLiteral("SocksProxy")] = mProxyMap.take(mUi.systemProxySocksEdit->objectName());
            mProxyMap[QStringLiteral("NoProxy")] = mProxyMap.take(mUi.systemNoProxyEdit->objectName());
        }
    } else if (mUi.autoScriptProxyRadioButton->isChecked()) {
        proxyType = KProtocolManager::PACProxy;
        mProxyMap[QStringLiteral("ProxyScript")] = mUi.proxyScriptUrlRequester->text();
    } else if (mUi.autoDiscoverProxyRadioButton->isChecked()) {
        proxyType = KProtocolManager::WPADProxy;
    }

    KSaveIOConfig::setProxyType(proxyType);
    KSaveIOConfig::setProxyDisplayUrlFlags(displayUrlFlags);
    KSaveIOConfig::setUseReverseProxy(mUi.useReverseProxyCheckBox->isChecked());

    // Save the common proxy setting...
    KSaveIOConfig::setProxyFor(QStringLiteral("http"), mProxyMap.value(QStringLiteral("HttpProxy")));
    KSaveIOConfig::setProxyFor(QStringLiteral("https"), mProxyMap.value(QStringLiteral("HttpsProxy")));
    KSaveIOConfig::setProxyFor(QStringLiteral("ftp"), mProxyMap.value(QStringLiteral("FtpProxy")));
    KSaveIOConfig::setProxyFor(QStringLiteral("socks"), mProxyMap.value(QStringLiteral("SocksProxy")));

    KSaveIOConfig::setProxyConfigScript (mProxyMap.value(QStringLiteral("ProxyScript")));
    KSaveIOConfig::setNoProxyFor (mProxyMap.value(QStringLiteral("NoProxy")));

    KSaveIOConfig::updateRunningIOSlaves (this);
    if (isPACProxyType(lastProxyType) || isPACProxyType(proxyType)) {
        KSaveIOConfig::updateProxyScout (this);
    }

    emit changed (false);
}

void KProxyDialog::defaults()
{
    mUi.noProxyRadioButton->setChecked(true);
    mUi.proxyScriptUrlRequester->clear();

    mUi.manualProxyHttpEdit->clear();
    mUi.manualProxyHttpsEdit->clear();
    mUi.manualProxyFtpEdit->clear();
    mUi.manualProxySocksEdit->clear();
    mUi.manualNoProxyEdit->clear();

    mUi.manualProxyHttpSpinBox->setValue(0);
    mUi.manualProxyHttpsSpinBox->setValue(0);
    mUi.manualProxyFtpSpinBox->setValue(0);
    mUi.manualProxySocksSpinBox->setValue(0);

    mUi.systemProxyHttpEdit->clear();
    mUi.systemProxyHttpsEdit->clear();
    mUi.systemProxyFtpEdit->clear();
    mUi.systemProxySocksEdit->clear();

    emit changed (true);
}

bool KProxyDialog::autoDetectSystemProxy(QLineEdit* edit, const QString& envVarStr, bool showValue)
{
    const QStringList envVars = envVarStr.split(QL1S(","), QString::SkipEmptyParts);
    Q_FOREACH (const QString & envVar, envVars) {
        const QByteArray envVarUtf8(envVar.toUtf8());
        const QByteArray envVarValue = qgetenv(envVarUtf8.constData());
        if (!envVarValue.isEmpty()) {
            if (showValue) {
                mProxyMap[edit->objectName()] = envVar;
                edit->setText(envVarValue);
            } else {
                edit->setText(envVar);
            }
            edit->setEnabled(!showValue);
            return true;
        }
    }
    return false;
}

void KProxyDialog::on_autoDetectButton_clicked()
{
    const bool showValue = mUi.showEnvValueCheckBox->isChecked();
    bool wasChanged = false;

    wasChanged |= autoDetectSystemProxy(mUi.systemProxyHttpEdit, ENV_HTTP_PROXY, showValue);
    wasChanged |= autoDetectSystemProxy(mUi.systemProxyHttpsEdit, ENV_HTTPS_PROXY, showValue);
    wasChanged |= autoDetectSystemProxy(mUi.systemProxyFtpEdit, ENV_FTP_PROXY, showValue);
    wasChanged |= autoDetectSystemProxy(mUi.systemProxySocksEdit, ENV_SOCKS_PROXY, showValue);
    wasChanged |= autoDetectSystemProxy(mUi.systemNoProxyEdit, ENV_NO_PROXY, showValue);

    if (wasChanged)
        emit changed (true);
}

void KProxyDialog::on_manualProxyHttpEdit_textChanged(const QString& text)
{
    mUi.useSameProxyCheckBox->setEnabled(!text.isEmpty());
}

void KProxyDialog::on_manualNoProxyEdit_textChanged (const QString& text)
{
    mUi.useReverseProxyCheckBox->setEnabled(!text.isEmpty());
}

void KProxyDialog::on_manualProxyHttpEdit_textEdited(const QString& text)
{
    if (!mUi.useSameProxyCheckBox->isChecked()) {
        return;
    }

    mUi.manualProxyHttpsEdit->setText(text);
    mUi.manualProxyFtpEdit->setText(text);
    mUi.manualProxySocksEdit->setText(text);
}

void KProxyDialog::on_manualProxyHttpSpinBox_valueChanged (int value)
{
    if (!mUi.useSameProxyCheckBox->isChecked()) {
        return;
    }

    mUi.manualProxyHttpsSpinBox->setValue(value);
    mUi.manualProxyFtpSpinBox->setValue(value);
    mUi.manualProxySocksSpinBox->setValue(value);
}

void KProxyDialog::on_showEnvValueCheckBox_toggled (bool on)
{
    if (on) {
        showSystemProxyUrl(mUi.systemProxyHttpEdit, &mProxyMap[mUi.systemProxyHttpEdit->objectName()]);
        showSystemProxyUrl(mUi.systemProxyHttpsEdit, &mProxyMap[mUi.systemProxyHttpsEdit->objectName()]);
        showSystemProxyUrl(mUi.systemProxyFtpEdit, &mProxyMap[mUi.systemProxyFtpEdit->objectName()]);
        showSystemProxyUrl(mUi.systemProxySocksEdit, &mProxyMap[mUi.systemProxySocksEdit->objectName()]);
        showSystemProxyUrl(mUi.systemNoProxyEdit, &mProxyMap[mUi.systemNoProxyEdit->objectName()]);
        return;
    }

    mUi.systemProxyHttpEdit->setText(mProxyMap.take(mUi.systemProxyHttpEdit->objectName()));
    mUi.systemProxyHttpEdit->setEnabled(true);
    mUi.systemProxyHttpsEdit->setText(mProxyMap.take(mUi.systemProxyHttpsEdit->objectName()));
    mUi.systemProxyHttpsEdit->setEnabled(true);
    mUi.systemProxyFtpEdit->setText(mProxyMap.take(mUi.systemProxyFtpEdit->objectName()));
    mUi.systemProxyFtpEdit->setEnabled(true);
    mUi.systemProxySocksEdit->setText(mProxyMap.take(mUi.systemProxySocksEdit->objectName()));
    mUi.systemProxySocksEdit->setEnabled(true);
    mUi.systemNoProxyEdit->setText(mProxyMap.take(mUi.systemNoProxyEdit->objectName()));
    mUi.systemNoProxyEdit->setEnabled(true);
}

void KProxyDialog::on_useSameProxyCheckBox_clicked(bool on)
{
    if (on) {
        mProxyMap[QStringLiteral("ManProxyHttps")] = manualProxyToText (mUi.manualProxyHttpsEdit, mUi.manualProxyHttpsSpinBox, QL1C (' '));
        mProxyMap[QStringLiteral("ManProxyFtp")] = manualProxyToText (mUi.manualProxyFtpEdit, mUi.manualProxyFtpSpinBox, QL1C (' '));
        mProxyMap[QStringLiteral("ManProxySocks")] = manualProxyToText (mUi.manualProxySocksEdit, mUi.manualProxySocksSpinBox, QL1C (' '));

        const QString& httpProxy = mUi.manualProxyHttpEdit->text();
        if (!httpProxy.isEmpty()) {
            mUi.manualProxyHttpsEdit->setText(httpProxy);
            mUi.manualProxyFtpEdit->setText(httpProxy);
            mUi.manualProxySocksEdit->setText(httpProxy);
        }
        const int httpProxyPort = mUi.manualProxyHttpSpinBox->value();
        if (httpProxyPort > 0) {
            mUi.manualProxyHttpsSpinBox->setValue(httpProxyPort);
            mUi.manualProxyFtpSpinBox->setValue(httpProxyPort);
            mUi.manualProxySocksSpinBox->setValue(httpProxyPort);
        }
        return;
    }

    setManualProxyFromText(mProxyMap.take (QStringLiteral("ManProxyHttps")), mUi.manualProxyHttpsEdit, mUi.manualProxyHttpsSpinBox);
    setManualProxyFromText(mProxyMap.take (QStringLiteral("ManProxyFtp")), mUi.manualProxyFtpEdit, mUi.manualProxyFtpSpinBox);
    setManualProxyFromText(mProxyMap.take (QStringLiteral("ManProxySocks")), mUi.manualProxySocksEdit, mUi.manualProxySocksSpinBox);
}

void KProxyDialog::slotChanged()
{
    emit changed(true);
}

QString KProxyDialog::quickHelp() const
{
    return i18n ("<h1>Proxy</h1>"
                 "<p>A proxy server is an intermediate program that sits between "
                 "your machine and the Internet and provides services such as "
                 "web page caching and/or filtering.</p>"
                 "<p>Caching proxy servers give you faster access to sites you have "
                 "already visited by locally storing or caching the content of those "
                 "pages; filtering proxy servers, on the other hand, provide the "
                 "ability to block out requests for ads, spam, or anything else you "
                 "want to block.</p>"
                 "<p><u>Note:</u> Some proxy servers provide both services.</p>");
}


