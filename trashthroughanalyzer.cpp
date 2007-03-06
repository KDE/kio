/* This file is part of the KDE project
   Copyright (C) 2004 David Faure <faure@kde.org>
                 2007 Jos van den Oever <jos@vandenoever.info>

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
#define STRIGI_IMPORT_API
#include <strigi/streamthroughanalyzer.h>
#include <strigi/analyzerplugin.h>
#include <strigi/fieldtypes.h>
#include <strigi/indexable.h>
#include "trashimpl.h"
#include <KUrl>
using namespace jstreams;
using namespace std;

class TrashThroughAnalyzerFactory;
class TrashThroughAnalyzer : public StreamThroughAnalyzer {
    const TrashThroughAnalyzerFactory* factory;
    TrashImpl impl;
    Indexable* idx;
    void setIndexable(Indexable*i) {
        idx = i;
    }
    InputStream *connectInputStream(InputStream *in);
    bool isReadyWithStream() { return true; }
public:
    TrashThroughAnalyzer(const TrashThroughAnalyzerFactory* f) :factory(f) {}
};
//define all the available analyzers in this plugin
class TrashThroughAnalyzerFactory : public StreamThroughAnalyzerFactory {
private:
    const char* getName() const {
        return "TrashThroughAnalyzer";
    }
    StreamThroughAnalyzer* newInstance() const {
        return new TrashThroughAnalyzer(this);
    }
    void registerFields(FieldRegister&);
    static const cnstr originalpathFieldName;
    static const cnstr dateofdeletionFieldName;
public:
    const RegisteredField* originalpathField;
    const RegisteredField* dateofdeletionField;
};

InputStream*
TrashThroughAnalyzer::connectInputStream(InputStream* in) {
    const string& path = idx->getPath();
    if (strncmp(path.c_str(), "system:/trash", 13)
            || strncmp(path.c_str(), "trash:/", 7)) {
        return in;
    }
    KUrl url(path.c_str());

    if ( url.protocol()=="system"
      && url.path().startsWith("/trash") )
    {
        QString path = url.path();
        path.remove(0, 6);
        url.setProtocol("trash");
        url.setPath(path);
    }
    
    //kDebug() << k_funcinfo << info.url() << endl;
    if ( url.protocol() != "trash" )
        return false;

    int trashId;
    QString fileId;
    QString relativePath;
    if ( !TrashImpl::parseURL( url, trashId, fileId, relativePath ) ) {
        return in;
    }

    TrashImpl::TrashedFileInfo trashInfo;
    if ( !impl.infoForFile( trashId, fileId, trashInfo ) ) {
        return in;
    }

    idx->setField(factory->originalpathField,
        (const char*)trashInfo.origPath.toUtf8());
    idx->setField(factory->dateofdeletionField,
        trashInfo.deletionDate.toTime_t());
    return in;
}

const cnstr TrashThroughAnalyzerFactory::originalpathFieldName("originalpath");
const cnstr TrashThroughAnalyzerFactory::dateofdeletionFieldName(
    "dateofdeletion");

void
TrashThroughAnalyzerFactory::registerFields(FieldRegister& reg) {
    originalpathField = reg.registerField(originalpathFieldName,
        FieldRegister::stringType, 1, 0);
    dateofdeletionField = reg.registerField(dateofdeletionFieldName,
        FieldRegister::integerType, 1, 0);
}

class Factory : public AnalyzerFactoryFactory {
public:
    list<StreamThroughAnalyzerFactory*>
    getStreamThroughAnalyzerFactories() const {
        list<StreamThroughAnalyzerFactory*> af;
        af.push_back(new TrashThroughAnalyzerFactory());
        return af;
    }
};

STRIGI_ANALYZER_FACTORY(Factory)

