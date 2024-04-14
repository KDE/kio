#ifndef SCOPEDPROCESSRUNNER_P_H
#define SCOPEDPROCESSRUNNER_P_H
#include "kprocessrunner_p.h"

class QDBusPendingCallWatcher;

class ScopedProcessRunner : public ForkingProcessRunner
{
    Q_OBJECT
public:
    explicit ScopedProcessRunner();
    void startProcess() override;
    bool waitForStarted(int timeout) override;

private:
    QDBusPendingCallWatcher *m_transientUnitStartupwatcher;
};

#endif // SCOPEDPROCESSRUNNER_H
