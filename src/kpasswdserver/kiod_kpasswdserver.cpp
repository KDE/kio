#include "kpasswdserver.h"

#include <kpluginfactory.h>
#include <kpluginloader.h>

K_PLUGIN_FACTORY_WITH_JSON(KPasswdServerFactory, "kpasswdserver.json", registerPlugin<KPasswdServer>();)

#include "kiod_kpasswdserver.moc"

