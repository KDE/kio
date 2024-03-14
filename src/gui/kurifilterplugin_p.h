/*
    SPDX-FileCopyrightText: 2000-2001, 2003, 2010 Dawit Alemayehu <adawit at kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "kiogui_export.h"
#include "kurifilter.h"

#include <KPluginMetaData>

/**
 * @class KUriFilterPlugin kurifilter.h <KUriFilter>
 *
 * Base class for URI filter plugins.
 *
 * This class applies a single filter to a URI. All plugins designed to provide
 * URI filtering service should inherit from this abstract class and provide a
 * concrete implementation.
 *
 * All inheriting classes need to implement the pure virtual function
 * @ref filterUri.
 *
 * @short Abstract class for URI filter plugins.
 */
class KIOGUI_EXPORT KUriFilterPlugin : public QObject
{
    Q_OBJECT

public:
    /**
     * Constructs a filter plugin with a given name
     *
     * @param parent the parent object, or @c nullptr for no parent
     * @param name the name of the plugin, mandatory
     */
    explicit KUriFilterPlugin(QObject *parent, const KPluginMetaData &data);

    ~KUriFilterPlugin() override;

public:
    /**
     * Filters a URI.
     *
     * @param data the URI data to be filtered.
     * @return A boolean indicating whether the URI has been changed.
     */
    virtual bool filterUri(KUriFilterData &data) const = 0;

protected:
    /**
     * Sets the URL in @p data to @p uri.
     */
    void setFilteredUri(KUriFilterData &data, const QUrl &uri) const;

    /**
     * Sets the error message in @p data to @p errormsg.
     */
    void setErrorMsg(KUriFilterData &data, const QString &errmsg) const;

    /**
     * Sets the URI type in @p data to @p type.
     */
    void setUriType(KUriFilterData &data, KUriFilterData::UriTypes type) const;

    /**
     * Sets the arguments and options string in @p data to @p args if any were
     * found during filtering.
     */
    void setArguments(KUriFilterData &data, const QString &args) const;

    /**
     * Sets the name of the search provider, the search term and keyword/term
     * separator in @p data.
     */
    void setSearchProvider(KUriFilterData &data, KUriFilterSearchProvider *provider, const QString &term, const QChar &separator) const;

    /**
     * Sets the information about the search @p providers in @p data.
     */
    void setSearchProviders(KUriFilterData &data, const QList<KUriFilterSearchProvider *> &providers) const;

    /**
     * Returns the icon name for the given @p url and URI @p type.
     */
    QString iconNameFor(const QUrl &url, KUriFilterData::UriTypes type) const;

    /**
     * Performs a DNS lookup for @p hostname and returns the result.
     *
     * This function uses the KIO DNS cache to speed up the
     * lookup. It also avoids doing a reverse lookup if the given
     * host name is already an ip address.
     *
     * \note All uri filter plugins that need to perform a hostname
     * lookup should use this function.
     *
     * @param hostname   the hostname to lookup.
     * @param timeout    the amount of time in msecs to wait for the lookup.
     * @return the result of the host name lookup.
     */
    QHostInfo resolveName(const QString &hostname, unsigned long timeout) const;

private:
    class KUriFilterPluginPrivate *const d;
};
