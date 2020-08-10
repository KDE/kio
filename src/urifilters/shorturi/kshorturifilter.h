/*
    kshorturifilter.h

    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2000 Malte Starostik <starosti@zedat.fu-berlin.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KSHORTURIFILTER_H
#define KSHORTURIFILTER_H

#include <QList>
#include <QRegularExpression>
#include <QVariant>

#include <kurifilter.h>

/**
* This is short URL filter class.
*
* @short A filter that converts short URLs into fully qualified ones.
*
* @author Dawit Alemayehu <adawit@kde.org>
* @author Malte Starostik <starosti@zedat.fu-berlin.de>
*/
class KShortUriFilter : public KUriFilterPlugin
{
    Q_OBJECT
public:

    /**
     * Creates a Short URI filter object
     *
     * @param parent the parent of this class.
     * @param name the internal name for this object.
     */
    explicit KShortUriFilter(QObject *parent = nullptr, const QVariantList &args = QVariantList());

    /**
     * Destructor
     */
    virtual ~KShortUriFilter()
    {
    }

    /**
     * Converts short URIs into fully qualified valid URIs
     * whenever possible.
     *
     * Parses any given invalid URI to determine whether it
     * is a known short URI and converts it to its fully
     * qualified version.
     *
     * @param data the data to be filtered
     * @return true if the url has been filtered
     */
    bool filterUri(KUriFilterData &data) const override;

    /**
     * Returns the name of the config module for
     * this plugin.
     *
     * @return the name of the config module.
     */
    QString configName() const override;

    /**
     * Returns an instance of the module used to configure
     * this object.
         *
         * @return the config module
         */
    KCModule *configModule(QWidget *, const char *) const override;

public Q_SLOTS:
    void configure();

private:

    struct URLHint
    {
        URLHint()
        {
        }

        URLHint(const QString &r, const QString &p, KUriFilterData::UriTypes t = KUriFilterData::NetProtocol)
            : hintRe(r)
            , prepend(p)
            , type(t)
        {
        }

        QRegularExpression hintRe; // if this matches, then...
        QString prepend; // ...prepend this to the url
        KUriFilterData::UriTypes type;
    };

    QList<URLHint> m_urlHints;
    QString m_strDefaultUrlScheme;
};

#endif
