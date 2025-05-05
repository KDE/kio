/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2001, 2003, 2010 Dawit Alemayehu <adawit at kde.org>

    Original author
    SPDX-FileCopyrightText: 2000 Yves Arrouye <yves@realnames.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURIFILTER_H
#define KURIFILTER_H

#include "kiogui_export.h"

#include <QHash>
#include <QObject>
#include <QPair>
#include <QStringList>
#include <QUrl>

#include <memory>

#ifdef Q_OS_WIN
#undef ERROR
#endif

class KUriFilterPrivate;
class KUriFilterDataPrivate;
class QHostInfo;

/*!
 * \class KUriFilterSearchProvider
 * \inheaderfile KUriFilter
 * \inmodule KIOGui
 *
 * \brief Class that holds information about a search provider.
 *
 */
class KIOGUI_EXPORT KUriFilterSearchProvider
{
public:
    /*!
     * Default constructor.
     */
    KUriFilterSearchProvider();

    /*!
     * Copy constructor.
     */
    KUriFilterSearchProvider(const KUriFilterSearchProvider &);

    virtual ~KUriFilterSearchProvider();

    /*!
     * Returns the desktop filename of the search provider without any extension.
     *
     * For example, if the desktop filename of the search provider was
     * "foobar.desktop", this function will return "foobar".
     */
    QString desktopEntryName() const;

    /*!
     * Returns the descriptive name of the search provider, e.g.\ "Google News".
     *
     * This name comes from the "Name=" property entry in the desktop file that
     * contains the search provider's information.
     */
    QString name() const;

    /*!
     * Returns the icon name associated with the search provider when available.
     */
    virtual QString iconName() const;

    /*!
     * Returns all the web shortcut keys associated with this search provider.
     *
     * \sa defaultKey
     */
    QStringList keys() const;

    /*!
     * Returns the default web shortcut key for this search provider.
     *
     * Right now this is the same as doing keys().first(), it might however
     * change based on what the backend plugins do.
     *
     * \sa keys
     */
    QString defaultKey() const;

    KUriFilterSearchProvider &operator=(const KUriFilterSearchProvider &);

protected:
    /*!
     *
     */
    void setDesktopEntryName(const QString &);

    /*!
     *
     */
    void setIconName(const QString &);

    /*!
     *
     */
    void setKeys(const QStringList &);

    /*!
     *
     */
    void setName(const QString &);

private:
    friend class KUriFilterPlugin;
    class KUriFilterSearchProviderPrivate;
    std::unique_ptr<KUriFilterSearchProviderPrivate> const d;
};

/*!
 * \class KUriFilterData
 * \inheaderfile KUriFilter
 * \inmodule KIOGui
 *
 * \brief A class for exchanging filtering information.
 *
 * This class is a basic messaging class used to exchange filtering information
 * between the filter plugins and the application requesting the filtering
 * service.
 *
 * Use this object if you require a more detailed information about the URI you
 * want to filter. Any application can create an instance of this class and send
 * it to KUriFilter to have the plugins fill out all possible information about
 * the URI.
 *
 * On successful filtering you can use uriType() to determine what type
 * of resource the request was filtered into. See KUriFilter::UriTypes for
 * details. If an error is encountered, then KUriFilter::Error is returned.
 * You can use errorMsg to obtain the error information.
 *
 * The functions in this class are not reentrant.
 *
 * Example:
 *
 * Here is a basic example of how this class is used with KUriFilter:
 * \code
 *   KUriFilterData filterData (QLatin1String("kde.org"));
 *   bool filtered = KUriFilter::self()->filterUri(filterData);
 * \endcode
 *
 * If you are only interested in getting the list of preferred search providers,
 * then you can do the following:
 *
 * \code
 * KUriFilterData data;
 * data.setData("<text-to-search-for>");
 * data.setSearchFilteringOptions(KUriFilterData::RetrievePreferredSearchProvidersOnly);
 * bool filtered = KUriFilter::self()->filterSearchUri(data, KUriFilter::NormalTextFilter);
 * \endcode
 */

class KIOGUI_EXPORT KUriFilterData
{
public:
    /*!
     * Describes the type of the URI that was filtered.
     *
     * \value NetProtocol Any network protocol: http, ftp, nttp, pop3, etc...
     * \value LocalFile A local file whose executable flag is not set
     * \value LocalDir A local directory
     * \value Executable A local file whose executable flag is set
     * \value Help A man or info page
     * \value Shell A shell executable (ex: echo "Test..." >> ~/testfile)
     * \value Blocked A URI that should be blocked/filtered (ex: ad filtering)
     * \value Error An incorrect URI (ex: "~johndoe" when user johndoe does not exist in that system)
     * \value Unknown A URI that is not identified. Default value when a KUriFilterData is first created.
     */
    enum UriTypes {
        NetProtocol = 0,
        LocalFile,
        LocalDir,
        Executable,
        Help,
        Shell,
        Blocked,
        Error,
        Unknown,
    };

    /*!
     * This enum describes the search filtering options to be used.
     *
     * \value SearchFilterOptionNone No search filter options are set and normal filtering is performed on the input data.
     * \value RetrieveSearchProvidersOnly If set, the list of all available search providers are returned without any input filtering. This flag only applies
     * when used in conjunction with the KUriFilter::NormalTextFilter flag.
     * \value RetrievePreferredSearchProvidersOnly If set, the list of preferred search providers are returned without any input filtering. This flag only
     * applies when used in conjunction with the KUriFilter::NormalTextFilter flag.
     * \value RetrieveAvailableSearchProvidersOnly Same as doing RetrievePreferredSearchProvidersOnly |  RetrieveSearchProvidersOnly, where all available search
     * providers are returned if no preferred ones are available. No input filtering will be performed.
     *
     * \sa setSearchFilteringOptions
     * \sa KUriFilter::filterSearchUri
     */
    enum SearchFilterOption {
        SearchFilterOptionNone = 0x0,
        RetrieveSearchProvidersOnly = 0x01,
        RetrievePreferredSearchProvidersOnly = 0x02,
        RetrieveAvailableSearchProvidersOnly = (RetrievePreferredSearchProvidersOnly | RetrieveSearchProvidersOnly),
    };
    Q_DECLARE_FLAGS(SearchFilterOptions, SearchFilterOption)

    /*!
     * Default constructor.
     *
     * Creates a UriFilterData object.
     */
    KUriFilterData();

    /*!
     * Creates a KUriFilterData object from the given URL.
     *
     * \a url is the URL to be filtered.
     */
    explicit KUriFilterData(const QUrl &url);

    /*!
     * Creates a KUriFilterData object from the given string.
     *
     * \a url is the string to be filtered.
     */
    explicit KUriFilterData(const QString &url);

    /*!
     * Copy constructor.
     *
     * Creates a KUriFilterData object from another KURIFilterData object.
     *
     * \a other the uri filter data to be copied.
     */
    KUriFilterData(const KUriFilterData &other);

    ~KUriFilterData();

    /*!
     * Returns the filtered or the original URL.
     *
     * If one of the plugins successfully filtered the original input, this
     * function returns it. Otherwise, it will return the input itself.
     *
     * Returns the filtered or original url.
     */
    QUrl uri() const;

    /*!
     * Returns an error message.
     *
     * This functions returns the error message set by the plugin whenever the
     * uri type is set to KUriFilterData::ERROR. Otherwise, it returns a nullptr
     * string.
     *
     * Returns the error message or a nullptr when there is none.
     */
    QString errorMsg() const;

    /*!
     * Returns the URI type.
     *
     * This method always returns KUriFilterData::UNKNOWN if the given URL was
     * not filtered.
     *
     * Returns the type of the URI
     */
    UriTypes uriType() const;

    /*!
     * Returns the absolute path if one has already been set.
     *
     * Returns the absolute path, or QString()
     *
     * \sa hasAbsolutePath()
     */
    QString absolutePath() const;

    /*!
     * Checks whether the supplied data had an absolute path.
     *
     * Returns true if the supplied data has an absolute path
     *
     * \sa absolutePath()
     */
    bool hasAbsolutePath() const;

    /*!
     * Returns the command line options and arguments for a local resource
     * when present.
     *
     * Returns options and arguments when present, otherwise QString()
     */
    QString argsAndOptions() const;

    /*!
     * Checks whether the current data is a local resource with command line
     * options and arguments.
     *
     * Returns true if the current data has command line options and arguments
     */
    bool hasArgsAndOptions() const;

    /*!
     * Returns true if the filters should attempt to check whether the
     * supplied uri is an executable. False otherwise.
     */
    bool checkForExecutables() const;

    /*!
     * The string as typed by the user, before any URL processing is done.
     */
    QString typedString() const;

    /*!
     * Returns the search term portion of the typed string.
     *
     * If the typedString was not filtered by a search filter plugin, this
     * function returns an empty string.
     *
     * \sa typedString
     */
    QString searchTerm() const;

    /*!
     * Returns the character that is used to separate the search term from the
     * keyword.
     *
     * If typedString was not filtered by a search filter plugin, this
     * function returns a null character.
     *
     * \sa typedString
     */
    QChar searchTermSeparator() const;

    /*!
     * Returns the name of the search service provider, e.g.\ Google.
     *
     * If typedString was not filtered by a search filter plugin, this
     * function returns an empty string.
     *
     * \sa typedString
     */
    QString searchProvider() const;

    /*!
     * Returns a list of the names of preferred or available search providers.
     *
     * This function returns the list of providers marked as preferred whenever
     * the input data, i.e. typedString, is successfully filtered.
     *
     * If no default search provider has been selected prior to a filter request,
     * this function will return an empty list. To avoid this problem you must
     * either set an alternate default search provider using setAlternateDefaultSearchProvider
     * or set one of the SearchFilterOption flags if you are only interested
     * in getting the list of providers and not filtering the input.
     *
     * Additionally, you can also provide alternate search providers in case
     * there are no preferred ones already selected.
     *
     * You can use queryForPreferredServiceProvider to obtain the query
     * associated with the list of search providers returned by this function.
     *
     * \sa setAlternateSearchProviders()
     * \sa setAlternateDefaultSearchProvider()
     * \sa setSearchFilteringOptions()
     * \sa queryForPreferredSearchProvider()
     */
    QStringList preferredSearchProviders() const;

    /*!
     * Returns information about \a provider.
     *
     * You can use this function to obtain the more information about the search
     * providers returned by preferredSearchProviders.
     *
     * \sa preferredSearchProviders()
     */
    KUriFilterSearchProvider queryForSearchProvider(const QString &provider) const;

    /*!
     * Returns the web shortcut url for the given preferred search provider.
     *
     * You can use this function to obtain the query for the preferred search
     * providers returned by preferredSearchProviders.
     *
     * The query returned by this function is in web shortcut format, i.e.
     * "gg:foo bar", and must be re-filtered through KUriFilter to obtain a
     * valid url.
     *
     * \sa preferredSearchProviders
     */
    QString queryForPreferredSearchProvider(const QString &provider) const;

    /*!
     * Returns all the query urls for the given search provider.
     *
     * Use this function to obtain all the different queries that can be used
     * for the given provider. For example, if a search engine provider named
     * "foobar" has web shortcuts named "foobar", "foo" and "bar", then this
     * function, unlike queryForPreferredSearchProvider, will return a
     * a query for each and every web shortcut.
     *
     * \sa queryForPreferredSearchProvider
     */
    QStringList allQueriesForSearchProvider(const QString &provider) const;

    /*!
     * Returns the icon associated with the given preferred search provider.
     *
     * You can use this function to obtain the icon names associated with the
     * preferred search providers returned by preferredSearchProviders.
     *
     * \sa preferredSearchProviders
     */
    QString iconNameForPreferredSearchProvider(const QString &provider) const;

    /*!
     * Returns the list of alternate search providers.
     *
     * This function returns an empty list if setAlternateSearchProviders
     * was not called to set the alternate search providers to be when no
     * preferred providers have been chosen by the user through the search
     * configuration module.
     *
     * \sa setAlternateSearchProviders
     * \sa preferredSearchProviders
     */
    QStringList alternateSearchProviders() const;

    /*!
     * Returns the search provider to use when a default provider is not available.
     *
     * This function returns an empty string if setAlternateDefaultSearchProvider
     * was not called to set the default search provider to be used when none has been
     * chosen by the user through the search configuration module.
     *
     * \sa setAlternateDefaultSearchProvider
     */
    QString alternateDefaultSearchProvider() const;

    /*!
     * Returns the default protocol to use when filtering potentially valid url inputs.
     *
     * By default this function will return an empty string.
     *
     * \sa setDefaultUrlScheme
     */
    QString defaultUrlScheme() const;

    /*!
     * Returns the specified search filter options.
     *
     * By default this function returns SearchFilterOptionNone.
     *
     * \sa setSearchFilteringOptions
     */
    SearchFilterOptions searchFilteringOptions() const;

    /*!
     * The name of the icon that matches the current filtered URL.
     *
     * This function returns a null string by default and when no icon is found
     * for the filtered URL.
     */
    QString iconName();

    /*!
     * Check whether the provided uri is executable or not.
     *
     * Setting this to false ensures that typing the name of an executable does
     * not start that application. This is useful in the location bar of a
     * browser. The default value is true.
     */
    void setCheckForExecutables(bool check);

    /*!
     * Same as above except the argument is a URL.
     *
     * Use this function to set the string to be filtered when you construct an
     * empty filter object.
     *
     * \a url the URL to be filtered.
     */
    void setData(const QUrl &url);

    /*!
     * Sets the URL to be filtered.
     *
     * Use this function to set the string to be
     * filtered when you construct an empty filter
     * object.
     *
     * \a url the string to be filtered.
     */
    void setData(const QString &url);

    /*!
     * Sets the absolute path to be used whenever the supplied data is a
     * relative local URL.
     *
     * \note This function should only be used for local resources, i.e. the
     * "file:/" protocol. It is useful for specifying the absolute path in
     * cases where the actual URL might be relative. If deriving the path from
     * a QUrl, make sure you set the argument for this function to the result
     * of calling path () instead of url ().
     *
     * \a abs_path  the absolute path to the local resource.
     *
     * Returns true if absolute path is successfully set. Otherwise, false.
     */
    bool setAbsolutePath(const QString &abs_path);

    /*!
     * Sets a list of search providers to use in case no preferred search
     * providers are available.
     *
     * The list of preferred search providers set using this function will only
     * be used if the default and favorite search providers have not yet been
     * selected by the user. Otherwise, the providers specified through this
     * function will be ignored.
     *
     * \sa alternateSearchProviders
     * \sa preferredSearchProviders
     */
    void setAlternateSearchProviders(const QStringList &providers);

    /*!
     * Sets the search provider to use in case no default provider is available.
     *
     * The default search provider set using this function will only be used if
     * the default and favorite search providers have not yet been selected by
     * the user. Otherwise, the default provider specified by through function
     * will be ignored.
     *
     * \sa alternateDefaultSearchProvider
     * \sa preferredSearchProviders
     */
    void setAlternateDefaultSearchProvider(const QString &provider);

    /*!
     * Sets the default scheme used when filtering potentially valid url inputs.
     *
     * Use this function to change the default protocol used when filtering
     * potentially valid url inputs. The default protocol is http.
     *
     * If the scheme is specified without a separator, then  "://" will be used
     * as the separator by default. For example, if the default url scheme was
     * simply set to "ftp", then a potentially valid url input such as "kde.org"
     * will be filtered to "ftp://kde.org".
     *
     * \sa defaultUrlScheme
     */
    void setDefaultUrlScheme(const QString &);

    /*!
     * Sets the options used by search filter plugins to filter requests.
     *
     * The default search filter option is SearchFilterOptionNone. See
     * SearchFilterOption for the description of the other flags.
     *
     * It is important to note that the options set through this function can
     * prevent any filtering from being performed by search filter plugins.
     * As such, uriTypes can return KUriFilterData::Unknown and uri
     * can return an invalid url even though the filtering request returned
     * a successful response.
     *
     * \sa searchFilteringOptions
     */
    void setSearchFilteringOptions(SearchFilterOptions options);

    KUriFilterData &operator=(const QUrl &url);

    KUriFilterData &operator=(const QString &url);

private:
    friend class KUriFilterPlugin;
    std::unique_ptr<KUriFilterDataPrivate> d;
};

/*!
 * \class KUriFilter
 * \inmodule KIOGui
 *
 * \brief Filters the given input into a valid url whenever possible.
 *
 * KUriFilter applies a number of filters to a URI and returns a filtered version if any
 * filter matches.
 * A simple example is "kde.org" to "http://www.kde.org", which is commonplace in web browsers.
 *
 * The filters are implemented as plugins in KUriFilterPlugin subclasses.
 *
 * KUriFilter is a singleton object: obtain the instance by calling
 * KUriFilter::self() and use the public member functions to
 * perform the filtering.
 *
 * Example:
 *
 * To simply filter a given string:
 *
 * \code
 * QString url("kde.org");
 * bool filtered = KUriFilter::self()->filteredUri( url );
 * \endcode
 *
 * You can alternatively use a QUrl:
 *
 * \code
 * QUrl url("kde.org");
 * bool filtered = KUriFilter::self()->filterUri( url );
 * \endcode
 *
 * If you have a constant string or a constant URL, simply invoke the
 * corresponding function to obtain the filtered string or URL instead
 * of a boolean flag:
 *
 * \code
 * QString filteredText = KUriFilter::self()->filteredUri( "kde.org" );
 * \endcode
 *
 * All of the above examples should result in "kde.org" being filtered into
 * "http://kde.org".
 *
 * You can also restrict the filters to be used by supplying the name of the
 * filters you want to use. By default all available filters are used.
 *
 * To use specific filters, add the names of the filters you want to use to a
 * QStringList and invoke the appropriate filtering function.
 *
 * The examples below show the use of specific filters. KDE ships with the
 * following filter plugins by default:
 *
 * kshorturifilter:
 * This is used for filtering potentially valid url inputs such as "kde.org"
 * Additionally it filters shell variables and shortcuts such as $HOME and
 * ~ as well as man and info page shortcuts, # and ## respectively.
 *
 * kuriikwsfilter:
 * This is used for filtering normal input text into a web search url using the
 * configured fallback search engine selected by the user.
 *
 * kurisearchfilter:
 * This is used for filtering KDE webshortcuts. For example "gg:KDE" will be
 * converted to a url for searching the work "KDE" using the Google search
 * engine.
 *
 * localdomainfilter:
 * This is used for doing a DNS lookup to determine whether the input is a valid
 * local address.
 *
 * fixuphosturifilter:
 * This is used to append "www." to the host name of a pre filtered http url
 * if the original url cannot be resolved.
 *
 * \code
 * QString text ("kde.org");
 * bool filtered = KUriFilter::self()->filterUri(text, QLatin1String("kshorturifilter"));
 * \endcode
 *
 * The above code should result in "kde.org" being filtered into "http://kde.org".
 *
 * \code
 * QStringList list;
 * list << QLatin1String("kshorturifilter") << QLatin1String("localdomainfilter");
 * bool filtered = KUriFilter::self()->filterUri( text, list );
 * \endcode
 *
 * Additionally if you only want to do search related filtering, you can use the
 * search specific function, filterSearchUri, that is available in KDE
 * 4.5 and higher. For example, to search for a given input on the web you
 * can do the following:
 *
 * KUriFilterData filterData ("foo");
 * bool filtered = KUriFilter::self()->filterSearchUri(filterData, KUriFilterData::NormalTextFilter);
 *
 * KUriFilter converts all filtering requests to use KUriFilterData
 * internally. The use of this bi-directional class allows you to send specific
 * instructions to the filter plugins as well as receive detailed information
 * about the filtered request from them. See the documentation of KUriFilterData
 * class for more examples and details.
 *
 * All functions in this class are thread safe and reentrant.
 */
class KIOGUI_EXPORT KUriFilter
{
public:
    /*!
     * This enum describes the types of search plugin filters available.
     *
     * \value NormalTextFilter The plugin used to filter normal text, e.g. "some term to search".
     * \value WebShortcutFilter The plugin used to filter web shortcuts, e.g. gg:KDE.
     */
    enum SearchFilterType {
        NormalTextFilter = 0x01,
        WebShortcutFilter = 0x02,
    };
    Q_DECLARE_FLAGS(SearchFilterTypes, SearchFilterType)

    ~KUriFilter();

    /*!
     * Returns an instance of KUriFilter.
     */
    static KUriFilter *self();

    /*!
     * Filters \a data using the specified \a filters.
     *
     * If no named filters are specified, the default, then all the
     * URI filter plugins found will be used.
     *
     * \a data object that contains the URI to be filtered.
     *
     * \a filters specify the list of filters to be used.
     *
     * Returns a boolean indicating whether the URI has been changed
     */
    bool filterUri(KUriFilterData &data, const QStringList &filters = QStringList());

    /*!
     * Filters the URI given by the URL.
     *
     * The given URL is filtered based on the specified list of filters.
     * If the list is empty all available filters would be used.
     *
     * \a uri the URI to filter.
     *
     * \a filters specify the list of filters to be used.
     *
     * Returns a boolean indicating whether the URI has been changed
     */
    bool filterUri(QUrl &uri, const QStringList &filters = QStringList());

    /*!
     * Filters a string representing a URI.
     *
     * The given URL is filtered based on the specified list of filters.
     * If the list is empty all available filters would be used.
     *
     * \a uri The URI to filter.
     *
     * \a filters specify the list of filters to be used.
     *
     * Returns a boolean indicating whether the URI has been changed
     */
    bool filterUri(QString &uri, const QStringList &filters = QStringList());

    /*!
     * Returns the filtered URI.
     *
     * The given URL is filtered based on the specified list of filters.
     * If the list is empty all available filters would be used.
     *
     * \a uri The URI to filter.
     *
     * \a filters specify the list of filters to be used.
     *
     * Returns the filtered URI or null if it cannot be filtered
     */
    QUrl filteredUri(const QUrl &uri, const QStringList &filters = QStringList());

    /*!
     * Return a filtered string representation of a URI.
     *
     * The given URL is filtered based on the specified list of filters.
     * If the list is empty all available filters would be used.
     *
     * \a uri the URI to filter.
     *
     * \a filters specify the list of filters to be used.
     *
     * Returns the filtered URI or null if it cannot be filtered
     */
    QString filteredUri(const QString &uri, const QStringList &filters = QStringList());

    /*!
     * Filter \a data using the criteria specified by \a types.
     *
     * The search filter type can be individual value of SearchFilterTypes
     * or a combination of those types using the bitwise OR operator.
     *
     * You can also use the flags from KUriFilterData::SearchFilterOption
     * to alter the filtering mechanisms of the search filter providers.
     *
     * \a data object that contains the URI to be filtered.
     *
     * \a types the search filters used to filter the request.
     *
     * Returns \c true if the specified \a data was successfully filtered.
     *
     * \sa KUriFilterData::setSearchFilteringOptions
     */
    bool filterSearchUri(KUriFilterData &data, SearchFilterTypes types);

    /*!
     * Return a list of the names of all loaded plugins.
     */
    QStringList pluginNames() const;

protected:
    /*!
     * Constructor.
     *
     * Creates a KUriFilter object and calls loads all available URI filter plugins.
     */
    KUriFilter();

private:
    std::unique_ptr<KUriFilterPrivate> const d;
    friend class KUriFilterSingleton;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KUriFilterData::SearchFilterOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(KUriFilter::SearchFilterTypes)

#endif
