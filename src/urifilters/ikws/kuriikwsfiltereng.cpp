
/*  This file is part of the KDE project

    Copyright (C) 2002, 2003 Dawit Alemayehu <adawit@kde.org>
    Copyright (C) 2000 Yves Arrouye <yves@realnames.com>
    Copyright (C) 1999 Simon Hausmann <hausmann@kde.org>

    Advanced web shortcuts:
    Copyright (C) 2001 Andreas Hochsteger <e9625392@student.tuwien.ac.at>


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "kuriikwsfiltereng.h"
#include "searchprovider.h"

#include <kconfiggroup.h>
#include <kconfig.h>
#include <kprotocolinfo.h>

#include <QTextCodec>
#include <QLoggingCategory>

namespace {
QLoggingCategory category("org.kde.kurifilter-ikws", QtWarningMsg);
}

#define PDVAR(n,v) qCDebug(category) << n << " = '" << v << "'"

/**
 * IMPORTANT: If you change anything here, make sure you run the kurifiltertest
 * regression test (this should be included as part of "make test").
 */

KURISearchFilterEngine::KURISearchFilterEngine()
{
  loadConfig();
}

KURISearchFilterEngine::~KURISearchFilterEngine()
{
}

SearchProvider* KURISearchFilterEngine::webShortcutQuery(const QString& typedString, QString &searchTerm) const
{
  SearchProvider *provider = nullptr;

  if (m_bWebShortcutsEnabled)
  {
    const int pos = typedString.indexOf(QLatin1Char(m_cKeywordDelimiter));

    QString key;
    if ( pos > -1 )
      key = typedString.left(pos).toLower(); // #169801
    else if ( !typedString.isEmpty()  && m_cKeywordDelimiter == ' ')
      key = typedString;

    qCDebug(category) << "m_cKeywordDelimiter=" << QLatin1Char(m_cKeywordDelimiter) << "pos=" << pos << "key=" << key;

    if (!key.isEmpty() && !KProtocolInfo::isKnownProtocol(key))
    {
      provider = m_registry.findByKey(key);
      if (provider)
      {
        if (!m_bUseOnlyPreferredWebShortcuts || m_preferredWebShortcuts.contains(provider->desktopEntryName())) {
            searchTerm = typedString.mid(pos+1);
            qCDebug(category) << "found provider" << provider->desktopEntryName() << "searchTerm=" << searchTerm;
        } else {
          provider = nullptr;
        }
      }
    }
  }

  return provider;
}


SearchProvider* KURISearchFilterEngine::autoWebSearchQuery(const QString& typedString, const QString &defaultShortcut) const
{
  SearchProvider *provider = nullptr;
  const QString defaultSearchProvider = (m_defaultWebShortcut.isEmpty() ? defaultShortcut : m_defaultWebShortcut);

  if (m_bWebShortcutsEnabled && !defaultSearchProvider.isEmpty())
  {
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

KURISearchFilterEngine* KURISearchFilterEngine::self()
{
  return sSelfPtr;
}

QStringList KURISearchFilterEngine::modifySubstitutionMap(SubstMap& map,
                                                          const QString& query) const
{
  // Returns the number of query words
  QString userquery = query;

  // Do some pre-encoding, before we can start the work:
  {
    int start = 0;
    int pos = 0;
    QRegExp qsexpr(QStringLiteral("\\\"[^\\\"]*\\\""));

    // Temporary substitute spaces in quoted strings (" " -> "%20")
    // Needed to split user query into StringList correctly.
    while ((pos = qsexpr.indexIn(userquery, start)) >= 0)
    {
      QString s = userquery.mid (pos, qsexpr.matchedLength());
      s.replace(QLatin1Char(' '), QLatin1String("%20"));
      start = pos + s.length(); // Move after last quote
      userquery = userquery.replace (pos, qsexpr.matchedLength(), s);
    }
  }

  // Split user query between spaces:
  QStringList l = userquery.simplified().split(QLatin1Char(' '), QString::SkipEmptyParts);

  // Back-substitute quoted strings (%20 -> " "):
  userquery.replace (QLatin1String("%20"), QLatin1String(" "));
  l.replaceInStrings(QStringLiteral("%20"), QStringLiteral(" "));

  qCDebug(category) << "Generating substitution map:\n";
  // Generate substitution map from user query:
  for (int i=0; i<=l.count(); i++)
  {
    int pos = 0;
    QString v;
    QString nr = QString::number(i);

    // Add whole user query (\{0}) to substitution map:
    if (i==0)
      v = userquery;
    // Add partial user query items to substitution map:
    else
      v = l[i-1];

    // Insert partial queries (referenced by \1 ... \n) to map:
    map.insert(QString::number(i), v);
    PDVAR (QLatin1String("  map['") + nr + QLatin1String("']"), map[nr]);

    // Insert named references (referenced by \name) to map:
    if ((i>0) && (pos = v.indexOf(QLatin1Char('='))) > 0)
    {
      QString s = v.mid(pos + 1);
      QString k = v.left(pos);

      // Back-substitute references contained in references (e.g. '\refname' substitutes to 'thisquery=\0')
      s.replace(QLatin1String("%5C"), QLatin1String("\\"));
      map.insert(k, s);
      PDVAR (QLatin1String("  map['") + k + QLatin1String("']"), map[k]);
    }
  }

  return l;
}

static QString encodeString(const QString& s, QTextCodec *codec)
{
    // don't encode the space character, we replace it with + after the encoding
    QByteArray encoded = codec->fromUnicode(s).toPercentEncoding(QByteArrayLiteral(" "));
    encoded.replace(' ', '+');
    return QString::fromUtf8(encoded);
}

QString KURISearchFilterEngine::substituteQuery(const QString& url, SubstMap &map, const QString& userquery, QTextCodec *codec) const
{
  QString newurl = url;
  QStringList ql = modifySubstitutionMap (map, userquery);
  int count = ql.count();

  // Check, if old style '\1' is found and replace it with \{@} (compatibility mode):
  {
    int pos = -1;
    if ((pos = newurl.indexOf(QStringLiteral("\\1"))) >= 0)
    {
      qCWarning(category) << "WARNING: Using compatibility mode for newurl='" << newurl
                   << "'. Please replace old style '\\1' with new style '\\{0}' "
                      "in the query definition.\n";
      newurl = newurl.replace(pos, 2, QStringLiteral("\\{@}"));
    }
  }

  qCDebug(category) << "Substitute references:\n";
  // Substitute references (\{ref1,ref2,...}) with values from user query:
  {
    int pos = 0;
    QRegExp reflist(QStringLiteral("\\\\\\{[^\\}]+\\}"));

    // Substitute reflists (\{ref1,ref2,...}):
    while ((pos = reflist.indexIn(newurl)) >= 0)
    {
      bool found = false;

      //bool rest = false;
      QString v;
      QString rlstring = newurl.mid(pos + 2, reflist.matchedLength() - 3);
      PDVAR ("  reference list", rlstring);

      // \{@} gets a special treatment later
      if (rlstring == QLatin1String("@"))
      {
        v = QStringLiteral("\\@");
        found = true;
      }

      // TODO: strip whitespaces around commas
      QStringList rl = rlstring.split(QLatin1Char(','), QString::SkipEmptyParts);
      int i = 0;

      while ((i<rl.count()) && !found)
      {
        QString rlitem = rl[i];
        QRegExp range(QStringLiteral("[0-9]*\\-[0-9]*"));

        // Substitute a range of keywords
        if (range.indexIn(rlitem) >= 0)
        {
          int pos = rlitem.indexOf(QLatin1Char('-'));
          int first = rlitem.leftRef(pos).toInt();
          int last  = rlitem.rightRef(rlitem.length()-pos-1).toInt();

          if (first == 0)
            first = 1;

          if (last  == 0)
            last = count;

          for (int i=first; i<=last; i++)
          {
            v += map[QString::number(i)] + QLatin1Char(' ');
            // Remove used value from ql (needed for \{@}):
            ql[i-1] = QLatin1String("");
          }

          v = v.trimmed();
          if (!v.isEmpty())
            found = true;

          PDVAR (QLatin1String("    range"), QString::number(first) + QLatin1Char('-') + QString::number(last) + QLatin1String(" => '") + v + QLatin1Char('\''));
          v = encodeString(v, codec);
        }
        else if (rlitem.startsWith(QLatin1Char('\"')) && rlitem.endsWith(QLatin1Char('\"')))
        {
          // Use default string from query definition:
          found = true;
          QString s = rlitem.mid(1, rlitem.length() - 2);
          v = encodeString(s, codec);
          PDVAR ("    default", s);
        }
        else if (map.contains(rlitem))
        {
          // Use value from substitution map:
          found = true;
          PDVAR (QLatin1String("    map['") + rlitem + QLatin1String("']"), map[rlitem]);
          v = encodeString(map[rlitem], codec);

          // Remove used value from ql (needed for \{@}):
          QString c = rlitem.left(1);
          if (c==QLatin1String("0"))
          {
            // It's a numeric reference to '0'
            for (QStringList::Iterator it = ql.begin(); it!=ql.end(); ++it)
              (*it) = QLatin1String("");
          }
          else if ((c>=QLatin1String("0")) && (c<=QLatin1String("9"))) // krazy:excludeall=doublequote_chars
          {
            // It's a numeric reference > '0'
            int n = rlitem.toInt();
            ql[n-1] = QLatin1String("");
          }
          else
          {
            // It's a alphanumeric reference
            QStringList::Iterator it = ql.begin();
            while ((it != ql.end()) && !it->startsWith(rlitem + QLatin1Char('=')))
              ++it;
            if (it != ql.end())
              it->clear();
          }

          // Encode '+', otherwise it would be interpreted as space in the resulting url:
          v.replace(QLatin1Char('+'), QLatin1String("%2B"));
        }
        else if (rlitem == QLatin1String("@"))
        {
          v = QStringLiteral("\\@");
          PDVAR ("    v", v);
        }

        i++;
      }

      newurl.replace(pos, reflist.matchedLength(), v);
    }

    // Special handling for \{@};
    {
      PDVAR ("  newurl", newurl);
      // Generate list of unmatched strings:
      QString v = ql.join(QStringLiteral(" ")).simplified();

      PDVAR ("    rest", v);
      v = encodeString(v, codec);

      // Substitute \{@} with list of unmatched query strings
      newurl.replace(QLatin1String("\\@"), v);
    }
  }

  return newurl;
}

QUrl KURISearchFilterEngine::formatResult( const QString& url,
                                              const QString& cset1,
                                              const QString& cset2,
                                              const QString& query,
                                              bool isMalformed ) const
{
  SubstMap map;
  return formatResult (url, cset1, cset2, query, isMalformed, map);
}

QUrl KURISearchFilterEngine::formatResult( const QString& url,
                                              const QString& cset1,
                                              const QString& cset2,
                                              const QString& userquery,
                                              bool /* isMalformed */,
                                              SubstMap& map ) const
{
  // Return nothing if userquery is empty and it contains
  // substitution strings...
  if (userquery.isEmpty() && url.indexOf(QStringLiteral("\\{")) > 0)
    return QUrl();

  // Debug info of map:
  if (!map.isEmpty())
  {
    qCDebug(category) << "Got non-empty substitution map:\n";
    for(SubstMap::Iterator it = map.begin(); it != map.end(); ++it)
      PDVAR (QLatin1Literal("    map['") + it.key() + QLatin1Literal("']"), it.value());
  }

  // Create a codec for the desired encoding so that we can transcode the user's "url".
  QString cseta = cset1;
  if (cseta.isEmpty())
    cseta = QStringLiteral("UTF-8");

  QTextCodec *csetacodec = QTextCodec::codecForName(cseta.toLatin1());
  if (!csetacodec)
  {
    cseta = QStringLiteral("UTF-8");
    csetacodec = QTextCodec::codecForName(cseta.toLatin1());
  }

  PDVAR ("user query", userquery);
  PDVAR ("query definition", url);

  // Add charset indicator for the query to substitution map:
  map.insert(QStringLiteral("ikw_charset"), cseta);

  // Add charset indicator for the fallback query to substitution map:
  QString csetb = cset2;
  if (csetb.isEmpty())
    csetb = QStringLiteral("UTF-8");
  map.insert(QStringLiteral("wsc_charset"), csetb);

  QString newurl = substituteQuery (url, map, userquery, csetacodec);

  PDVAR ("substituted query", newurl);

  return QUrl(newurl, QUrl::StrictMode);
}

void KURISearchFilterEngine::loadConfig()
{
  qCDebug(category) << "Keywords Engine: Loading config...";

  // Load the config.
  KConfig config(QString::fromUtf8(name()) + QLatin1String("rc"), KConfig::NoGlobals);
  KConfigGroup group = config.group( "General" );

  m_cKeywordDelimiter = QString(group.readEntry("KeywordDelimiter", ":")).at(0).toLatin1();
  m_bWebShortcutsEnabled = group.readEntry("EnableWebShortcuts", true);
  m_defaultWebShortcut = group.readEntry("DefaultWebShortcut");
  m_bUseOnlyPreferredWebShortcuts = group.readEntry("UsePreferredWebShortcutsOnly", false);

  QStringList defaultPreferredShortcuts;
  if (!group.hasKey("PreferredWebShortcuts"))
      defaultPreferredShortcuts = DEFAULT_PREFERRED_SEARCH_PROVIDERS;
  m_preferredWebShortcuts = group.readEntry("PreferredWebShortcuts", defaultPreferredShortcuts);

  // Use either a white space or a : as the keyword delimiter...
  if (strchr (" :", m_cKeywordDelimiter) == nullptr)
    m_cKeywordDelimiter = ':';

  qCDebug(category) << "Web Shortcuts Enabled: " << m_bWebShortcutsEnabled;
  qCDebug(category) << "Default Shortcut: " << m_defaultWebShortcut;
  qCDebug(category) << "Keyword Delimiter: " << m_cKeywordDelimiter;
}

SearchProviderRegistry * KURISearchFilterEngine::registry()
{
     return &m_registry;
}
