/* This file is part of the KDE libraries
    Copyright (C) 2000 David Smith <dsmith@algonet.se>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kshellcompletion.h"

#include <stdlib.h>
#include <QtCore/QCharRef>
#include <QtCore/QMutableStringListIterator>
#include <QtCore/QRegExp>
#include <kcompletion.h>

class KShellCompletionPrivate
{
public:
    KShellCompletionPrivate()
        : m_word_break_char(' ')
        , m_quote_char1( '\"' )
        , m_quote_char2( '\'' )
        , m_escape_char( '\\' )
    {
    }

    void splitText(const QString &text, QString &text_start, QString &text_compl) const;
    bool quoteText(QString *text, bool force, bool skip_last) const;
    QString unquote(const QString &text) const;

    QString m_text_start; // part of the text that was not completed
    QString m_text_compl; // part of the text that was completed (unchanged)

    QChar m_word_break_char;
    QChar m_quote_char1;
    QChar m_quote_char2;
    QChar m_escape_char;
};

KShellCompletion::KShellCompletion()
    : KUrlCompletion(),
      d( new KShellCompletionPrivate )
{
}

KShellCompletion::~KShellCompletion()
{
    delete d;
}


/*
 * makeCompletion()
 *
 * Entry point for file name completion
 */
QString KShellCompletion::makeCompletion(const QString &text)
{
	// Split text at the last unquoted space
	//
	d->splitText(text, d->m_text_start, d->m_text_compl);

	// Remove quotes from the text to be completed
	//
	QString tmp = d->unquote(d->m_text_compl);
	d->m_text_compl = tmp;

	// Do exe-completion if there was no unquoted space
	//
	bool is_exe_completion = true;

	for ( int i = 0; i < d->m_text_start.length(); i++ ) {
		if ( d->m_text_start[i] != d->m_word_break_char ) {
			is_exe_completion = false;
			break;
		}
	}

	Mode mode = (is_exe_completion ? ExeCompletion : FileCompletion );

	setMode(mode);

	// Make completion on the last part of text
	//
	return KUrlCompletion::makeCompletion( d->m_text_compl );
}

/*
 * postProcessMatch, postProcessMatches
 *
 * Called by KCompletion before emitting match() and matches()
 *
 * Add add the part of the text that was not completed
 * Add quotes when needed
 */
void KShellCompletion::postProcessMatch( QString *match ) const
{
	KUrlCompletion::postProcessMatch( match );

	if ( match->isNull() )
		return;

	if ( match->endsWith( QLatin1Char( '/' ) ) )
		d->quoteText( match, false, true ); // don't quote the trailing '/'
	else
		d->quoteText( match, false, false ); // quote the whole text

	match->prepend( d->m_text_start );
}

void KShellCompletion::postProcessMatches( QStringList *matches ) const
{
	KUrlCompletion::postProcessMatches( matches );

	for ( QStringList::Iterator it = matches->begin();
		  it != matches->end(); ++it )
	{
		if ( !(*it).isNull() ) {
			if ( (*it).endsWith( QLatin1Char( '/' ) ) )
				d->quoteText( &(*it), false, true ); // don't quote trailing '/'
			else
				d->quoteText( &(*it), false, false ); // quote the whole text

			(*it).prepend( d->m_text_start );
		}
	}
}

void KShellCompletion::postProcessMatches( KCompletionMatches *matches ) const
{
	KUrlCompletion::postProcessMatches( matches );

	for ( KCompletionMatches::Iterator it = matches->begin();
		  it != matches->end(); ++it )
	{
		if ( !(*it).value().isNull() ) {
			if ( (*it).value().endsWith( QLatin1Char( '/' ) ) )
				d->quoteText( &(*it).value(), false, true ); // don't quote trailing '/'
			else
				d->quoteText( &(*it).value(), false, false ); // quote the whole text

			(*it).value().prepend( d->m_text_start );
		}
	}
}

/*
 * splitText
 *
 * Split text at the last unquoted space
 *
 * text_start = [out] text at the left, including the space
 * text_compl = [out] text at the right
 */
void KShellCompletionPrivate::splitText(const QString &text, QString &text_start,
		QString &text_compl) const
{
	bool in_quote = false;
	bool escaped = false;
	QChar p_last_quote_char;
	int last_unquoted_space = -1;
	int end_space_len = 0;

	for (int pos = 0; pos < text.length(); pos++) {

		end_space_len = 0;

		if ( escaped ) {
			escaped = false;
		}
		else if ( in_quote && text[pos] == p_last_quote_char ) {
			in_quote = false;
		}
		else if ( !in_quote && text[pos] == m_quote_char1 ) {
			p_last_quote_char = m_quote_char1;
			in_quote = true;
		}
		else if ( !in_quote && text[pos] == m_quote_char2 ) {
			p_last_quote_char = m_quote_char2;
			in_quote = true;
		}
		else if ( text[pos] == m_escape_char ) {
			escaped = true;
		}
		else if ( !in_quote && text[pos] == m_word_break_char ) {

			end_space_len = 1;

			while ( pos+1 < text.length() && text[pos+1] == m_word_break_char ) {
				end_space_len++;
				pos++;
			}

			if ( pos+1 == text.length() )
				break;

			last_unquoted_space = pos;
		}
	}

	text_start = text.left( last_unquoted_space + 1 );

	// the last part without trailing blanks
	text_compl = text.mid( last_unquoted_space + 1 );
}

/*
 * quoteText()
 *
 * Add quotations to 'text' if needed or if 'force' = true
 * Returns true if quotes were added
 *
 * skip_last => ignore the last charachter (we add a space or '/' to all filenames)
 */
bool KShellCompletionPrivate::quoteText(QString *text, bool force, bool skip_last) const
{
	int pos = 0;

	if ( !force ) {
		pos = text->indexOf( m_word_break_char );
		if ( skip_last && (pos == (int)(text->length())-1) ) pos = -1;
	}

	if ( !force && pos == -1 ) {
		pos = text->indexOf( m_quote_char1 );
		if ( skip_last && (pos == (int)(text->length())-1) ) pos = -1;
	}

	if ( !force && pos == -1 ) {
		pos = text->indexOf( m_quote_char2 );
		if ( skip_last && (pos == (int)(text->length())-1) ) pos = -1;
	}

	if ( !force && pos == -1 ) {
		pos = text->indexOf( m_escape_char );
		if ( skip_last && (pos == (int)(text->length())-1) ) pos = -1;
	}

	if ( force || (pos >= 0) ) {

		// Escape \ in the string
		text->replace( m_escape_char,
		               QString( m_escape_char ) + m_escape_char );

		// Escape " in the string
		text->replace( m_quote_char1,
		               QString( m_escape_char ) + m_quote_char1 );

		// " at the beginning
		text->insert( 0, m_quote_char1 );

		// " at the end
		if ( skip_last )
			text->insert( text->length()-1, m_quote_char1 );
		else
			text->insert( text->length(), m_quote_char1 );

		return true;
	}

	return false;
}

/*
 * unquote
 *
 * Remove quotes and return the result in a new string
 *
 */
QString KShellCompletionPrivate::unquote(const QString &text) const
{
	bool in_quote = false;
	bool escaped = false;
	QChar p_last_quote_char;
	QString result;

	for (int pos = 0; pos < text.length(); pos++) {

		if ( escaped ) {
			escaped = false;
			result.insert( result.length(), text[pos] );
		}
		else if ( in_quote && text[pos] == p_last_quote_char ) {
			in_quote = false;
		}
		else if ( !in_quote && text[pos] == m_quote_char1 ) {
			p_last_quote_char = m_quote_char1;
			in_quote = true;
		}
		else if ( !in_quote && text[pos] == m_quote_char2 ) {
			p_last_quote_char = m_quote_char2;
			in_quote = true;
		}
		else if ( text[pos] == m_escape_char ) {
			escaped = true;
			result.insert( result.length(), text[pos] );
		}
		else {
			result.insert( result.length(), text[pos] );
		}

	}

	return result;
}


