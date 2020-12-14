/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef DELEGATEANIMATIONHANDLER_P_H
#define DELEGATEANIMATIONHANDLER_P_H

#include <QBasicTimer>
#include <QElapsedTimer>
#include <QMap>
#include <QPersistentModelIndex>
#include <QStyle>
#include <QTimeLine>
#include <QTime>
#include <QTimer>

class QAbstractItemView;

namespace KIO
{

class CachedRendering : public QObject
{
    Q_OBJECT
public:

    CachedRendering(QStyle::State state, const QSize &size, const QModelIndex &validityIndex, qreal devicePixelRatio = 1.0);
    bool checkValidity(QStyle::State current) const
    {
        return state == current && valid;
    }

    QStyle::State state;
    QPixmap regular;
    QPixmap hover;

    bool valid;
    QPersistentModelIndex validityIndex;
private Q_SLOTS:
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void modelReset();
};

class AnimationState
{
public:
    ~AnimationState();
    AnimationState(const AnimationState &) = delete;
    AnimationState &operator=(const AnimationState &) = delete;

    //Progress of the mouse hovering animation
    qreal hoverProgress() const;
    //Progress of the icon fading animation
    qreal fadeProgress() const;
    //Angle of the painter, to paint the animation for a file job on an item
    qreal jobAnimationAngle() const;

    void setJobAnimation(bool value);
    bool hasJobAnimation() const;

    CachedRendering *cachedRendering() const
    {
        return renderCache;
    }
    //The previous render-cache is deleted, if there was one
    void setCachedRendering(CachedRendering *rendering)
    {
        delete renderCache;
        renderCache = rendering;
    }

    //Returns current cached rendering, and removes it from this state.
    //The caller has the ownership.
    CachedRendering *takeCachedRendering()
    {
        CachedRendering *ret = renderCache;
        renderCache = nullptr;
        return ret;
    }

    CachedRendering *cachedRenderingFadeFrom() const
    {
        return fadeFromRenderCache;
    }
    //The previous render-cache is deleted, if there was one
    void setCachedRenderingFadeFrom(CachedRendering *rendering)
    {
        delete fadeFromRenderCache;
        fadeFromRenderCache = rendering;
        if (rendering) {
            m_fadeProgress = 0;
        } else {
            m_fadeProgress = 1;
        }
    }

private:
    explicit AnimationState(const QModelIndex &index);
    bool update();

    QPersistentModelIndex index;
    QTimeLine::Direction direction;
    bool animating;
    bool jobAnimation;
    qreal progress;
    qreal m_fadeProgress;
    qreal m_jobAnimationAngle;
    QElapsedTimer time;
    QElapsedTimer creationTime;
    CachedRendering *renderCache;
    CachedRendering *fadeFromRenderCache;

    friend class DelegateAnimationHandler;
};

class DelegateAnimationHandler : public QObject
{
    Q_OBJECT

    typedef QList<AnimationState *> AnimationList;
    typedef QMapIterator<const QAbstractItemView *, AnimationList *> AnimationListsIterator;
    typedef QMutableMapIterator<const QAbstractItemView *, AnimationList *> MutableAnimationListsIterator;

public:
    explicit DelegateAnimationHandler(QObject *parent = nullptr);
    ~DelegateAnimationHandler();

    AnimationState *animationState(const QStyleOption &option, const QModelIndex &index, const QAbstractItemView *view);

    void restartAnimation(AnimationState *state);

    void gotNewIcon(const QModelIndex &index);

private Q_SLOTS:
    void viewDeleted(QObject *view);
    void sequenceTimerTimeout();

private:
    void eventuallyStartIteration(const QModelIndex &index);
    AnimationState *findAnimationState(const QAbstractItemView *view, const QModelIndex &index) const;
    void addAnimationState(AnimationState *state, const QAbstractItemView *view);
    void startAnimation(AnimationState *state);
    int runAnimations(AnimationList *list, const QAbstractItemView *view);
    void timerEvent(QTimerEvent *event) override;
    void setSequenceIndex(int arg1);

private:
    QMap<const QAbstractItemView *, AnimationList *> animationLists;
    QElapsedTimer fadeInAddTime;
    QBasicTimer timer;
    //Icon sequence handling:
    QPersistentModelIndex sequenceModelIndex;
    QTimer iconSequenceTimer;
    int currentSequenceIndex;
};

}

#endif
