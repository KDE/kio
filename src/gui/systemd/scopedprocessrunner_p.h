#ifndef SCOPEDPROCESSRUNNER_P_H
#define SCOPEDPROCESSRUNNER_P_H
#include "kprocessrunner_p.h"

class ScopedProcessRunner : public ForkingProcessRunner
{
    Q_OBJECT
public:
    explicit ScopedProcessRunner();
    void startProcess() override;
};

#endif // SCOPEDPROCESSRUNNER_H
