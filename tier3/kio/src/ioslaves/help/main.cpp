#ifdef _WIN32
#define LIBXML_DLL_IMPORT __declspec(dllimport)
#else
extern "C" int xmlLoadExtDtdDefaultValue;
#endif

#include "kio_help.h"
#include "xslt.h"


#include <QDebug>

#include <QtCore/QString>

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <libxml/xmlversion.h>
#include <libxml/xmlmemory.h>
#include <libxml/debugXML.h>
#include <libxml/HTMLtree.h>
#include <libxml/xmlIO.h>
#include <libxml/parserInternals.h>

#include <libxslt/xsltconfig.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <libexslt/exslt.h>

extern "C"
{
    Q_DECL_EXPORT int kdemain( int argc, char **argv )
    {
        setupStandardDirs();

        //qDebug() << "Starting " << getpid();

        if (argc != 4)
        {
            fprintf(stderr, "Usage: kio_help protocol domain-socket1 domain-socket2\n");
            exit(-1);
        }

        LIBXML_TEST_VERSION
        xmlSubstituteEntitiesDefault(1);
        xmlLoadExtDtdDefaultValue = 1;
        exsltRegisterAll();

        HelpProtocol slave(false, argv[2], argv[3]);
        slave.dispatchLoop();

        //qDebug() << "Done";
        return 0;
    }
}



