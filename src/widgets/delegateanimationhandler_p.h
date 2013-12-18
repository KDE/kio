/*
   This file is part of the KDE project

   Copyright © 2007 Fredrik Höglund <fredrik@kde.org>

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

#ifndef DELEGATEANIMATIONHANDLER_P_H
#define DELEGATEANIMATIONHANDLER_P_H

#include <QBasicTimer>
#include <QMap>
#include <QLinkedList>
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
    
    CachedRendering(QStyle::State state, const QSize &size, QModelIndex validityIndex);
    bool checkValidity(QStyle::State current) const { return state == current && valid; }

    QStyle::State state;
    QPixmap regular;
    QPixmap hover;
    
    bool valid;
    QPersistentModelIndex validityIndex;
private Q_SLOTS:
    void dataChanged(const QModelIndex & topLeft, const QModelIndex & bottomRight);
    void modelReset();
};


class AnimationState
{
public:
    ~AnimationState();
    //Progress of the mouse hovering animation
    qreal hoverProgress() const;
    //Progress of the icon fading animation
    qreal fadeProgress() const;
    //Angle of the painter, to paint the animation for a file job on an item
    qreal jobAnimationAngle() const;

    void setJobAnimation(bool value);
    bool hasJobAnimation() const;

    CachedRendering *cachedRendering() const { return renderCache; }
    //The previous render-cache is deleted, if there was one
    void setCachedRendering(CachedRendering *rendering) { delete renderCache; renderCache = rendering; }

    //Returns current cached rendering, and removes it from this state.
    //The caller has the ownership.
    CachedRendering *takeCachedRendering() { CachedRendering* ret = renderCache; renderCache = 0; return ret; }

    CachedRendering* cachedRenderingFadeFrom() const { return fadeFromRenderCache; }
    //The previous render-cache is deleted, if there was one
    void setCachedRenderingFadeFrom(CachedRendering* rendering) { delete fadeFromRenderCache; fadeFromRenderCache = rendering; if(rendering) m_fadeProgress = 0; else m_fadeProgress = 1; }

private:
    AnimationState(const QModelIndex &index);
    bool update();

    QPersistentModelIndex index;
    QTimeLine::Direction direction;
    bool animating;
    bool jobAnimation;
    qreal progress;
    qreal m_fadeProgress;
    qreal m_jobAnimationAngle;
    QTime time;
    QTime creationTime;
    CachedRendering *renderCache;
    CachedRendering *fadeFromRenderCache;

    friend class DelegateAnimationHandler;
};


class DelegateAnimationHandler : public QObject
{
    Q_OBJECT

    typedef QLinkedList<AnimationState*> AnimationList;
    typedef QMapIterator<const QAbstractItemView *, AnimationList*> AnimationListsIterator;
    typedef QMutableMapIterator<const QAbstractItemView *, AnimationList*> MutableAnimationListsIterator;

public:
    DelegateAnimationHandler(QObject *parent = 0);
    ~DelegateAnimationHandler();

    AnimationState *animationState(const QStyleOption &option, const QModelIndex &index, const QAbstractItemView *view);

    void restartAnimation(AnimationState* state);

    void gotNewIcon(const QModelIndex& index);

private Q_SLOTS:
    void viewDeleted(QObject *view);
    void sequenceTimerTimeout();

private:
    void eventuallyStartIteration(QModelIndex index);
    AnimationState *findAnimationState(const QAbstractItemView *view, const QModelIndex &index) const;
    void addAnimationState(AnimationState *state, const QAbstractItemView *view);
    void startAnimation(AnimationState *state);
    int runAnimations(AnimationList *list, const QAbstractItemView *view);
    void timerEvent(QTimerEvent *event);
    void setSequenceIndex(int arg1);

private:
    QMap<const QAbstractItemView*, AnimationList*> animationLists;
    QTime fadeInAddTime;
    QBasicTimer timer;
    //Icon sequence handling:
    QPersistentModelIndex sequenceModelIndex;
    QTimer iconSequenceTimer;
    int currentSequenceIndex;
};

}

#endif
