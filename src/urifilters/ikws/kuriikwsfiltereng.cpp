/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2002, 2003 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2000 Yves Arrouye <yves@realnames.com>
    SPDX-FileCopyrightText: 1999 Simon Hausmann <hausmann@kde.org>

    Advanced web shortcuts:
    SPDX-FileCopyrightText: 2001 Andreas Hochsteger <e9625392@student.tuwien.ac.at>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kuriikwsfiltereng.h"
#include "searchprovider.h"

#include <KConfigGroup>
#include <KConfig>
#include <kprotocolinfo.h>

#include <QRegularExpression>
#include <QTextCodec>
#include <QLoggingCategory>

namespace {
QLoggingCategory category("kf.kio.urifilters.ikws", QtWarningMsg);
}

#define PDVAR(n, v) qCDebug(category) << n << " = '" << v << "'"

/**
 * IMPORTANT: If you change anything here, make sure kiowidgets-kurifiltertest-{colon,space}-separator
 * unit tests still pass (they're usually run as part of "make test").
 */

KURISearchFilterEngine::KURISearchFilterEngine()
{
    loadConfig();
}

KURISearchFilterEngine::~KURISearchFilterEngine()
{
}

SearchProvider *KURISearchFilterEngine::webShortcutQuery(const QString &typedString, QString &searchTerm) const
{
    SearchProvider *provider = nullptr;

    if (m_bWebShortcutsEnabled) {
        QString key;
         if (typedString.contains(QLatin1Char('!'))) {
            const static QRegularExpression bangRegex(QStringLiteral("!([^ ]+)"));
            const auto match = bangRegex.match(typedString);
            if (match.hasMatch() && match.lastCapturedIndex() == 1) {
                key = match.captured(1);
                searchTerm = QString(typedString).remove(bangRegex);
            }
        }
        if (key.isEmpty()) {
            const int pos = typedString.indexOf(QLatin1Char(m_cKeywordDelimiter));
            if (pos > -1) {
                key = typedString.left(pos).toLower(); // #169801
                searchTerm = typedString.mid(pos+1);
            } else if (!typedString.isEmpty() && m_cKeywordDelimiter == ' ') {
                key = typedString;
                searchTerm = typedString.mid(pos+1);
            }
        }

        qCDebug(category) << "m_cKeywordDelimiter=" << QLatin1Char(m_cKeywordDelimiter) << "key=" << key << "typedString=" << typedString;

        // If the key contains a : an assertion in the isKnownProtocol method would fail. This can be
        // the case if the delimiter is switched to space, see kiowidgets_space_separator_test
        if (!key.isEmpty() && (key.contains(QLatin1Char(':')) || !KProtocolInfo::isKnownProtocol(key))) {
            provider = m_registry.findByKey(key);
            if (provider) {
                if (!m_bUseOnlyPreferredWebShortcuts || m_preferredWebShortcuts.contains(provider->desktopEntryName())) {
                    qCDebug(category) << "found provider" << provider->desktopEntryName() << "searchTerm=" << searchTerm;
                } else {
                    provider = nullptr;
                }
            }
        }
    }

    return provider;
}

SearchProvider *KURISearchFilterEngine::autoWebSearchQuery(const QString &typedString, const QString &defaultShortcut) const
{
    SearchProvider *provider = nullptr;
    const QString defaultSearchProvider = (m_defaultWebShortcut.isEmpty() ? defaultShortcut : m_defaultWebShortcut);

    if (m_bWebShortcutsEnabled && !defaultSearchProvider.isEmpty()) {
        // Make sure we ignore supported protocols, e.g. "smb:", "http:"
        const int pos = typedString.indexOf(QLatin1Char(':'));

        if (pos == -1 || !KProtocolInfo::isKnownProtocol(typedString.left(pos))) {
            provider = m_registry.findByDesktopName(defaultSearchProvider);
        }
    }

    return provider;
}

QByteArray KURISearchFilterEngine::name() const
{
    return "kuriikwsfilter";
}

char KURISearchFilterEngine::keywordDelimiter() const
{
    return m_cKeywordDelimiter;
}

QString KURISearchFilterEngine::defaultSearchEngine() const
{
    return m_defaultWebShortcut;
}

QStringList KURISearchFilterEngine::favoriteEngineList() const
{
    return m_preferredWebShortcuts;
}

Q_GLOBAL_STATIC(KURISearchFilterEngine, sSelfPtr)

KURISearchFilterEngine *KURISearchFilterEngine::self()
{
    return sSelfPtr;
}

QStringList KURISearchFilterEngine::modifySubstitutionMap(SubstMap &map, const QString &query) const
{
    // Returns the number of query words
    QString userquery = query;

    // Do some pre-encoding, before we can start the work:
    {
        const QRegularExpression qsexpr(QStringLiteral("\\\"[^\\\"]*\\\""));
        // Temporarily substitute spaces in quoted strings (" " -> "%20")
        // Needed to split user query into StringList correctly.
        int start = 0;
        QRegularExpressionMatch match;
        while ((match = qsexpr.match(userquery, start)).hasMatch()) {
            QString str = match.captured(0);
            str.replace(QLatin1Char(' '), QLatin1String("%20"));
            userquery.replace(match.capturedStart(0), match.capturedLength(0), str);
            start = match.capturedStart(0) + str.size(); // Move after last quote
        }
    }

    // Split user query between spaces:
    QStringList l = userquery.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);

    // Back-substitute quoted strings (%20 -> " "):
    userquery.replace(QLatin1String("%20"), QLatin1String(" "));
    l.replaceInStrings(QStringLiteral("%20"), QStringLiteral(" "));

    qCDebug(category) << "Generating substitution map:\n";
    // Generate substitution map from user query:
    for (int i = 0; i <= l.count(); i++) {
        int pos = 0;
        QString v;
        QString nr = QString::number(i);

        // Add whole user query (\{0}) to substitution map:
        if (i == 0) {
            v = userquery;
        }
        // Add partial user query items to substitution map:
        else {
            v = l[i-1];
        }

        // Insert partial queries (referenced by \1 ... \n) to map:
        map.insert(QString::number(i), v);
        PDVAR(QLatin1String("  map['") + nr + QLatin1String("']"), map[nr]);

        // Insert named references (referenced by \name) to map:
        if ((i > 0) && (pos = v.indexOf(QLatin1Char('='))) > 0) {
            QString s = v.mid(pos + 1);
            QString k = v.left(pos);

            // Back-substitute references contained in references (e.g. '\refname' substitutes to 'thisquery=\0')
            s.replace(QLatin1String("%5C"), QLatin1String("\\"));
            map.insert(k, s);
            PDVAR(QLatin1String("  map['") + k + QLatin1String("']"), map[k]);
        }
    }

    return l;
}

static QString encodeString(const QString &s, QTextCodec *codec)
{
    // we encode all characters, including the space character BUG: 304276
    QByteArray encoded = codec->fromUnicode(s).toPercentEncoding();
    return QString::fromUtf8(encoded);
}

QString KURISearchFilterEngine::substituteQuery(const QString &url, SubstMap &map, const QString &userquery, QTextCodec *codec) const
{
    QString newurl = url;
    QStringList ql = modifySubstitutionMap(map, userquery);
    const int count = ql.count();

    // Check, if old style '\1' is found and replace it with \{@} (compatibility mode):
    {
        int pos = -1;
        if ((pos = newurl.indexOf(QLatin1String("\\1"))) >= 0) {
            qCWarning(category) << "WARNING: Using compatibility mode for newurl='" << newurl
                                << "'. Please replace old style '\\1' with new style '\\{0}' "
                "in the query definition.\n";
            newurl.replace(pos, 2, QStringLiteral("\\{@}"));
        }
    }

    qCDebug(category) << "Substitute references:\n";
    // Substitute references (\{ref1,ref2,...}) with values from user query:
    {
        const QRegularExpression reflistRe(QStringLiteral("\\\\\\{([^\\}]+)\\}"));
        // Substitute reflists (\{ref1,ref2,...}):
        int start = 0;
        QRegularExpressionMatch match;
        while ((match = reflistRe.match(newurl, start)).hasMatch()) {
            bool found = false;

            //bool rest = false;
            QString v;
            const QString rlstring = match.captured(1);
            PDVAR("  reference list", rlstring);

            // \{@} gets a special treatment later
            if (rlstring == QLatin1String("@")) {
                v = QStringLiteral("\\@");
                found = true;
            }

            // TODO: strip whitespaces around commas
            const QStringList refList = rlstring.split(QLatin1Char(','), Qt::SkipEmptyParts);

            for (const QString &rlitem : refList) {
                if (found) {
                    break;
                }

                const QRegularExpression rangeRe(QStringLiteral("([0-9]*)\\-([0-9]*)"));
                const QRegularExpressionMatch rangeMatch = rangeRe.match(rlitem);
                // Substitute a range of keywords
                if (rangeMatch.hasMatch()) {
                    int first = rangeMatch.captured(1).toInt();
                    int last = rangeMatch.captured(2).toInt();

                    if (first == 0) {
                        first = 1;
                    }

                    if (last == 0) {
                        last = count;
                    }

                    for (int i = first; i <= last; i++) {
                        v += map[QString::number(i)] + QLatin1Char(' ');
                        // Remove used value from ql (needed for \{@}):
                        ql[i-1].clear();
                    }

                    v = v.trimmed();
                    if (!v.isEmpty()) {
                        found = true;
                    }

                    PDVAR(QLatin1String("    range"), QString::number(first) + QLatin1Char('-') + QString::number(last) + QLatin1String(" => '") + v + QLatin1Char('\''));
                    v = encodeString(v, codec);
                } else if (rlitem.startsWith(QLatin1Char('\"')) && rlitem.endsWith(QLatin1Char('\"'))) {
                    // Use default string from query definition:
                    found = true;
                    QString s = rlitem.mid(1, rlitem.length() - 2);
                    v = encodeString(s, codec);
                    PDVAR("    default", s);
                } else if (map.contains(rlitem)) {
                    // Use value from substitution map:
                    found = true;
                    PDVAR(QLatin1String("    map['") + rlitem + QLatin1String("']"), map[rlitem]);
                    v = encodeString(map[rlitem], codec);

                    // Remove used value from ql (needed for \{@}):
                    const QChar c = rlitem.at(0); // rlitem can't be empty at this point
                    if (c == QLatin1Char('0')) {
                        // It's a numeric reference to '0'
                        for (QStringList::Iterator it = ql.begin(); it != ql.end(); ++it) {
                            (*it).clear();
                        }
                    } else if ((c >= QLatin1String("0")) && (c <= QLatin1String("9"))) { // krazy:excludeall=doublequote_chars
                        // It's a numeric reference > '0'
                        int n = rlitem.toInt();
                        ql[n-1].clear();
                    } else {
                        // It's a alphanumeric reference
                        QStringList::Iterator it = ql.begin();
                        while ((it != ql.end()) && !it->startsWith(rlitem + QLatin1Char('='))) {
                            ++it;
                        }
                        if (it != ql.end()) {
                            it->clear();
                        }
                    }

                    // Encode '+', otherwise it would be interpreted as space in the resulting url:
                    v.replace(QLatin1Char('+'), QLatin1String("%2B"));
                } else if (rlitem == QLatin1String("@")) {
                    v = QStringLiteral("\\@");
                    PDVAR("    v", v);
                }
            }

            newurl.replace(match.capturedStart(0), match.capturedLength(0), v);
            start = match.capturedStart(0) + v.size();
        }

        // Special handling for \{@};
        {
            PDVAR("  newurl", newurl);
            // Generate list of unmatched strings:
            QString v = ql.join(QLatin1Char(' ')).simplified();

            PDVAR("    rest", v);
            v = encodeString(v, codec);

            // Substitute \{@} with list of unmatched query strings
            newurl.replace(QLatin1String("\\@"), v);
        }
    }

    return newurl;
}

QUrl KURISearchFilterEngine::formatResult(const QString &url, const QString &cset1, const QString &cset2, const QString &query, bool isMalformed) const
{
    SubstMap map;
    return formatResult(url, cset1, cset2, query, isMalformed, map);
}

QUrl KURISearchFilterEngine::formatResult(const QString &url, const QString &cset1, const QString &cset2, const QString &userquery, bool /* isMalformed */, SubstMap &map) const
{
    // Return nothing if userquery is empty and it contains
    // substitution strings...
    if (userquery.isEmpty() && url.indexOf(QLatin1String("\\{")) > 0) {
        return QUrl();
    }

    // Debug info of map:
    if (!map.isEmpty()) {
        qCDebug(category) << "Got non-empty substitution map:\n";
        for (SubstMap::Iterator it = map.begin(); it != map.end(); ++it) {
            PDVAR(QLatin1String("    map['") + it.key() + QLatin1String("']"), it.value());
        }
    }

    // Create a codec for the desired encoding so that we can transcode the user's "url".
    QString cseta = cset1;
    if (cseta.isEmpty()) {
        cseta = QStringLiteral("UTF-8");
    }

    QTextCodec *csetacodec = QTextCodec::codecForName(cseta.toLatin1());
    if (!csetacodec) {
        cseta = QStringLiteral("UTF-8");
        csetacodec = QTextCodec::codecForName(cseta.toLatin1());
    }

    PDVAR("user query", userquery);
    PDVAR("query definition", url);

    // Add charset indicator for the query to substitution map:
    map.insert(QStringLiteral("ikw_charset"), cseta);

    // Add charset indicator for the fallback query to substitution map:
    QString csetb = cset2;
    if (csetb.isEmpty()) {
        csetb = QStringLiteral("UTF-8");
    }
    map.insert(QStringLiteral("wsc_charset"), csetb);

    QString newurl = substituteQuery(url, map, userquery, csetacodec);

    PDVAR("substituted query", newurl);

    return QUrl(newurl, QUrl::StrictMode);
}

void KURISearchFilterEngine::loadConfig()
{
    qCDebug(category) << "Keywords Engine: Loading config...";

    // Load the config.
    KConfig config(QString::fromUtf8(name()) + QLatin1String("rc"), KConfig::NoGlobals);
    KConfigGroup group = config.group("General");

    m_cKeywordDelimiter = QString(group.readEntry("KeywordDelimiter", ":")).at(0).toLatin1();
    m_bWebShortcutsEnabled = group.readEntry("EnableWebShortcuts", true);
    m_defaultWebShortcut = group.readEntry("DefaultWebShortcut");
    m_bUseOnlyPreferredWebShortcuts = group.readEntry("UsePreferredWebShortcutsOnly", false);

    QStringList defaultPreferredShortcuts;
    if (!group.hasKey("PreferredWebShortcuts")) {
        defaultPreferredShortcuts = DEFAULT_PREFERRED_SEARCH_PROVIDERS;
    }
    m_preferredWebShortcuts = group.readEntry("PreferredWebShortcuts", defaultPreferredShortcuts);

    // Use either a white space or a : as the keyword delimiter...
    if (strchr(" :", m_cKeywordDelimiter) == nullptr) {
        m_cKeywordDelimiter = ':';
    }

    qCDebug(category) << "Web Shortcuts Enabled: " << m_bWebShortcutsEnabled;
    qCDebug(category) << "Default Shortcut: " << m_defaultWebShortcut;
    qCDebug(category) << "Keyword Delimiter: " << m_cKeywordDelimiter;
    m_registry.reload();
}

SearchProviderRegistry *KURISearchFilterEngine::registry()
{
    return &m_registry;
}
