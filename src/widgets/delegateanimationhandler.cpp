/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "delegateanimationhandler_p.h"

#include <QAbstractItemView>
#include <QPersistentModelIndex>
#include <QTime>
#include <QDebug>

#include <cmath>
#include "kdirmodel.h"
#include <QAbstractProxyModel>

#include "moc_delegateanimationhandler_p.cpp"

namespace KIO
{

// Needed because state() is a protected method
class ProtectedAccessor : public QAbstractItemView
{
    Q_OBJECT
public:
    bool draggingState() const
    {
        return state() == DraggingState;
    }
};

// Debug output is disabled by default, use kdebugdialog to enable it
//static int animationDebugArea() { static int s_area = KDebug::registerArea("kio (delegateanimationhandler)", false);
//                                  return s_area; }

// ---------------------------------------------------------------------------

CachedRendering::CachedRendering(QStyle::State state, const QSize &size, const QModelIndex &index, qreal devicePixelRatio)
    : state(state), regular(QPixmap(size*devicePixelRatio)), hover(QPixmap(size*devicePixelRatio)), valid(true), validityIndex(index)
{
    regular.setDevicePixelRatio(devicePixelRatio);
    hover.setDevicePixelRatio(devicePixelRatio);
    regular.fill(Qt::transparent);
    hover.fill(Qt::transparent);

    if (index.model()) {
        connect(index.model(), &QAbstractItemModel::dataChanged,
                this, &CachedRendering::dataChanged);
        connect(index.model(), &QAbstractItemModel::modelReset,
                this, &CachedRendering::modelReset);
    }
}

void CachedRendering::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (validityIndex.row() >= topLeft.row() && validityIndex.column() >= topLeft.column() &&
            validityIndex.row() <= bottomRight.row() && validityIndex.column() <= bottomRight.column()) {
        valid = false;
    }
}

void CachedRendering::modelReset()
{
    valid = false;
}

// ---------------------------------------------------------------------------

AnimationState::AnimationState(const QModelIndex &index)
    : index(index), direction(QTimeLine::Forward),
      animating(false), jobAnimation(false), progress(0.0), m_fadeProgress(1.0),
      m_jobAnimationAngle(0.0), renderCache(nullptr), fadeFromRenderCache(nullptr)
{
    creationTime.start();
}

AnimationState::~AnimationState()
{
    delete renderCache;
    delete fadeFromRenderCache;
}

bool AnimationState::update()
{
    const qreal runtime = (direction == QTimeLine::Forward ? 150 : 250); // milliseconds
    const qreal increment = 1000. / runtime / 1000.;
    const qreal delta = increment * time.restart();

    if (direction == QTimeLine::Forward) {
        progress = qMin(qreal(1.0), progress + delta);
        animating = (progress < 1.0);
    } else {
        progress = qMax(qreal(0.0), progress - delta);
        animating = (progress > 0.0);
    }

    if (fadeFromRenderCache) {
        //Icon fading goes always forwards
        m_fadeProgress = qMin(qreal(1.0), m_fadeProgress + delta);
        animating |= (m_fadeProgress < 1.0);
        if (m_fadeProgress == 1) {
            setCachedRenderingFadeFrom(nullptr);
        }
    }

    if (jobAnimation) {
        m_jobAnimationAngle += 1.0;
        if (m_jobAnimationAngle == 360) {
            m_jobAnimationAngle = 0;
        }

        if (index.model()->data(index, KDirModel::HasJobRole).toBool()) {
            animating = true;
            //there is a job here still...
            return false;
        } else {
            animating = false;
            //there's no job here anymore, return true so we stop painting this.
            return true;
        }
    } else {
        return !animating;
    }
}

qreal AnimationState::hoverProgress() const
{
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif
    return qRound(255.0 * std::sin(progress * M_PI_2)) / 255.0;
}

qreal AnimationState::fadeProgress() const
{
    return qRound(255.0 * std::sin(m_fadeProgress * M_PI_2)) / 255.0;
}

qreal AnimationState::jobAnimationAngle() const
{
    return m_jobAnimationAngle;
}

bool AnimationState::hasJobAnimation() const
{
    return jobAnimation;
}

void AnimationState::setJobAnimation(bool value)
{
    jobAnimation = value;
}

// ---------------------------------------------------------------------------

static const int switchIconInterval = 1000; ///@todo Eventually configurable interval?

DelegateAnimationHandler::DelegateAnimationHandler(QObject *parent)
    : QObject(parent)
{
    iconSequenceTimer.setSingleShot(true);
    iconSequenceTimer.setInterval(switchIconInterval);
    connect(&iconSequenceTimer, &QTimer::timeout, this, &DelegateAnimationHandler::sequenceTimerTimeout);;
}

DelegateAnimationHandler::~DelegateAnimationHandler()
{
    timer.stop();

    QMapIterator<const QAbstractItemView *, AnimationList *> i(animationLists);
    while (i.hasNext()) {
        i.next();
        qDeleteAll(*i.value());
        delete i.value();
    }
    animationLists.clear();
}

void DelegateAnimationHandler::sequenceTimerTimeout()
{
    QAbstractItemModel *model = const_cast<QAbstractItemModel *>(sequenceModelIndex.model());
    QAbstractProxyModel *proxy = qobject_cast<QAbstractProxyModel *>(model);
    QModelIndex index = sequenceModelIndex;

    if (proxy) {
        index = proxy->mapToSource(index);
        model = proxy->sourceModel();
    }

    KDirModel *dirModel = dynamic_cast<KDirModel *>(model);
    if (dirModel) {
        //qDebug() << "requesting" << currentSequenceIndex;
        dirModel->requestSequenceIcon(index, currentSequenceIndex);
        iconSequenceTimer.start(); // Some upper-bound interval is needed, in case items are not generated
    }
}

void DelegateAnimationHandler::gotNewIcon(const QModelIndex &index)
{
    Q_UNUSED(index);

    //qDebug() << currentSequenceIndex;
    if (sequenceModelIndex.isValid() && currentSequenceIndex) {
        iconSequenceTimer.start();
    }
//   if(index ==sequenceModelIndex) //Leads to problems
    ++currentSequenceIndex;
}

void DelegateAnimationHandler::setSequenceIndex(int sequenceIndex)
{
    //qDebug() << sequenceIndex;

    if (sequenceIndex > 0) {
        currentSequenceIndex = sequenceIndex;
        iconSequenceTimer.start();
    } else {
        currentSequenceIndex = 0;
        sequenceTimerTimeout(); //Set the icon back to the standard one
        currentSequenceIndex = 0; //currentSequenceIndex was incremented, set it back to 0
        iconSequenceTimer.stop();
    }
}

void DelegateAnimationHandler::eventuallyStartIteration(const QModelIndex &index)
{
//      if (KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects) {
    ///Think about it.

    if (sequenceModelIndex.isValid()) {
        setSequenceIndex(0);    // Stop old iteration, and reset the icon for the old iteration
    }

    // Start sequence iteration
    sequenceModelIndex = index;
    setSequenceIndex(1);
//      }
}

AnimationState *DelegateAnimationHandler::animationState(const QStyleOption &option,
        const QModelIndex &index,
        const QAbstractItemView *view)
{
    // We can't do animations reliably when an item is being dragged, since that
    // item will be drawn in two locations at the same time and hovered in one and
    // not the other. We can't tell them apart because they both have the same index.
    if (!view || static_cast<const ProtectedAccessor *>(view)->draggingState()) {
        return nullptr;
    }

    AnimationState *state = findAnimationState(view, index);
    bool hover = option.state & QStyle::State_MouseOver;

    // If the cursor has entered an item
    if (!state && hover) {
        state = new AnimationState(index);
        addAnimationState(state, view);

        if (!fadeInAddTime.isValid() ||
                (fadeInAddTime.isValid() && fadeInAddTime.elapsed() > 300)) {
            startAnimation(state);
        } else {
            state->animating = false;
            state->progress  = 1.0;
            state->direction = QTimeLine::Forward;
        }

        fadeInAddTime.restart();

        eventuallyStartIteration(index);
    } else if (state) {
        // If the cursor has exited an item
        if (!hover && (!state->animating || state->direction == QTimeLine::Forward)) {
            state->direction = QTimeLine::Backward;

            if (state->creationTime.elapsed() < 200) {
                state->progress = 0.0;
            }

            startAnimation(state);

            // Stop sequence iteration
            if (index == sequenceModelIndex) {
                setSequenceIndex(0);
                sequenceModelIndex = QPersistentModelIndex();
            }
        } else if (hover && state->direction == QTimeLine::Backward) {
            // This is needed to handle the case where an item is dragged within
            // the view, and dropped in a different location. State_MouseOver will
            // initially not be set causing a "hover out" animation to start.
            // This reverses the direction as soon as we see the bit being set.
            state->direction = QTimeLine::Forward;

            if (!state->animating) {
                startAnimation(state);
            }

            eventuallyStartIteration(index);
        }
    } else if (!state && index.model()->data(index, KDirModel::HasJobRole).toBool()) {
        state = new AnimationState(index);
        addAnimationState(state, view);
        startAnimation(state);
        state->setJobAnimation(true);
    }

    return state;
}

AnimationState *DelegateAnimationHandler::findAnimationState(const QAbstractItemView *view,
        const QModelIndex &index) const
{
    // Try to find a list of animation states for the view
    const AnimationList *list = animationLists.value(view);

    if (list) {
        for (AnimationState *state : *list)
            if (state->index == index) {
                return state;
            }
    }

    return nullptr;
}

void DelegateAnimationHandler::addAnimationState(AnimationState *state, const QAbstractItemView *view)
{
    AnimationList *list = animationLists.value(view);

    // If this is the first time we've seen this view
    if (!list) {
        connect(view, &QObject::destroyed, this, &DelegateAnimationHandler::viewDeleted);

        list = new AnimationList;
        animationLists.insert(view, list);
    }

    list->append(state);
}

void DelegateAnimationHandler::restartAnimation(AnimationState *state)
{
    startAnimation(state);
}

void DelegateAnimationHandler::startAnimation(AnimationState *state)
{
    state->time.start();
    state->animating = true;

    if (!timer.isActive()) {
        timer.start(1000 / 30, this);    // 30 fps
    }
}

int DelegateAnimationHandler::runAnimations(AnimationList *list, const QAbstractItemView *view)
{
    int activeAnimations = 0;
    QRegion region;

    QMutableListIterator<AnimationState *> i(*list);
    while (i.hasNext()) {
        AnimationState *state = i.next();

        if (!state->animating) {
            continue;
        }

        // We need to make sure the index is still valid, since it could be removed
        // while the animation is running.
        if (state->index.isValid()) {
            bool finished = state->update();
            region += view->visualRect(state->index);

            if (!finished) {
                activeAnimations++;
                continue;
            }
        }

        // If the direction is Forward, the state object needs to stick around
        // after the animation has finished, so we know that we've already done
        // a "hover in" for the index.
        if (state->direction == QTimeLine::Backward || !state->index.isValid()) {
            delete state;
            i.remove();
        }
    }

    // Trigger a repaint of the animated indexes
    if (!region.isEmpty()) {
        const_cast<QAbstractItemView *>(view)->viewport()->update(region);
    }

    return activeAnimations;
}

void DelegateAnimationHandler::viewDeleted(QObject *view)
{
    AnimationList *list = animationLists.take(static_cast<QAbstractItemView *>(view));
    qDeleteAll(*list);
    delete list;
}

void DelegateAnimationHandler::timerEvent(QTimerEvent *)
{
    int activeAnimations = 0;

    AnimationListsIterator i(animationLists);
    while (i.hasNext()) {
        i.next();
        AnimationList *list = i.value();
        const QAbstractItemView *view = i.key();

        activeAnimations += runAnimations(list, view);
    }

    if (activeAnimations == 0 && timer.isActive()) {
        timer.stop();
    }
}

}

#include "delegateanimationhandler.moc"
