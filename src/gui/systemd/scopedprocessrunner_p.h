#ifndef SCOPEDPROCESSRUNNER_P_H
#define SCOPEDPROCESSRUNNER_P_H
#include "kprocessrunner_p.h"

class ScopedProcessRunner : public ForkingProcessRunner
{
  Q_OBJECT
public:
    explicit ScopedProcessRunner();
private Q_SLOTS:
    void slotProcessStarted() override;
};

#endif // SCOPEDPROCESSRUNNER_H
