/*
    This file is part of the KIO framework tests
    SPDX-FileCopyrightText: 2022 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kfilefilter.h"

#include <QMimeDatabase>
#include <QTest>
#include <qtestcase.h>
#include <qvariant.h>

class KFileFilterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testFromFilterString_data()
    {
        QTest::addColumn<QString>("filterString");
        QTest::addColumn<QVariantList>("expectedFilters");

        QVariantList cppFilters = {QVariant::fromValue(KFileFilter("C++ Source Files", {"*.cpp", "*.cc", "*.C"}, {})),
                                   QVariant::fromValue(KFileFilter("Header files", {"*.h", "*.H"}, {}))};

        QMimeDatabase mdb;

        QTest::addRow("empty") << "" << QVariantList{};
        QTest::addRow("cpp") << "*.cpp|Sources (*.cpp)" << QVariantList{QVariant::fromValue(KFileFilter("Sources (*.cpp)", {"*.cpp"}, {}))};
        QTest::addRow("cpp_headers") << "*.cpp *.cc *.C|C++ Source Files\n*.h *.H|Header files" << cppFilters;
        QTest::addRow("no_label") << "*.cpp" << QVariantList{QVariant::fromValue(KFileFilter("*.cpp", {"*.cpp"}, {}))};
        QTest::addRow("escaped_slash") << "*.cue|CUE\\/BIN Files (*.cue)"
                                       << QVariantList{QVariant::fromValue(KFileFilter("CUE/BIN Files (*.cue)", {"*.cue"}, {}))};
        QTest::addRow("single_mimetype") << "text/plain"
                                         << QVariantList{QVariant::fromValue(KFileFilter(mdb.mimeTypeForName("text/plain").comment(), {}, {"text/plain"}))};

        QVariantList multipleMimeFilters = {QVariant::fromValue(KFileFilter(mdb.mimeTypeForName("image/png").comment(), {}, {"image/png"})),
                                            QVariant::fromValue(KFileFilter(mdb.mimeTypeForName("image/jpeg").comment(), {}, {"image/jpeg"}))};

        QTest::addRow("multiple_mimetypes") << "image/png image/jpeg" << multipleMimeFilters;
        QTest::addRow("mimeglob") << "audio/*" << QVariantList{QVariant::fromValue(KFileFilter(QString(), {}, {"audio/*"}))};
    }

    void testFromFilterString()
    {
        QFETCH(QString, filterString);
        QFETCH(QVariantList, expectedFilters);

        const QVector<KFileFilter> filters = KFileFilter::fromFilterString(filterString);

        QCOMPARE(filters.size(), expectedFilters.size());

        for (int i = 0; i < filters.size(); ++i) {
            KFileFilter expectedFilter = expectedFilters[i].value<KFileFilter>();
            KFileFilter filter = filters[i];

            QCOMPARE(expectedFilter, filter);
        }
    }

    void testToFilterString_data()
    {
        QTest::addColumn<QVariant>("input");
        QTest::addColumn<QString>("expectedFilterString");

        QTest::addRow("single_mime") << QVariant::fromValue(KFileFilter("", {}, {"text/plain"})) << "text/plain";
        QTest::addRow("double_mime") << QVariant::fromValue(KFileFilter("", {}, {"text/plain", "image/png"})) << "text/plain image/png";
        QTest::addRow("cpp") << QVariant::fromValue(KFileFilter("C++ source files", {"*.cpp"}, {})) << "*.cpp|C++ source files";
        QTest::addRow("cpp_with_header") << QVariant::fromValue(KFileFilter("C++ files", {"*.cpp", "*.h"}, {})) << "*.cpp *.h|C++ files";
        QTest::addRow("no_label") << QVariant::fromValue(KFileFilter("", {"*.png"}, {})) << "*.png";
        QTest::addRow("duplicate_label") << QVariant::fromValue(KFileFilter("*.cpp", {"*.cpp"}, {})) << "*.cpp";
        QTest::addRow("slash_to_escape") << QVariant::fromValue(KFileFilter("VCS/ICS calendar", {"*.ical"}, {})) << "*.ical|VCS\\/ICS calendar";
    }

    void testToFilterString()
    {
        QFETCH(QVariant, input);
        QFETCH(QString, expectedFilterString);

        QCOMPARE(input.value<KFileFilter>().toFilterString(), expectedFilterString);
    }
};
Q_DECLARE_METATYPE(KFileFilter);

QTEST_MAIN(KFileFilterTest)

#include "filefiltertest.moc"
