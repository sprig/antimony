#include <Python.h>

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#include "ui/canvas/connection.h"
#include "ui/canvas/graph_scene.h"
#include "ui/canvas/port.h"
#include "ui/canvas/inspector/inspector.h"
#include "ui/util/colors.h"


#include "app/app.h"
#include "app/undo/undo_add_link.h"

Connection::Connection(OutputPort* source)
    : Connection(source, NULL)
{
    // Nothing to do here
}

Connection::Connection(OutputPort* source, InputPort* target)
    : source(source), target(target), drag_state(target ? CONNECTED : NONE),
      snapping(false), hover(false)
{
    setFlags(QGraphicsItem::ItemIsSelectable|
             QGraphicsItem::ItemIsFocusable);
    setZValue(1);

    setFocus();
    setAcceptHoverEvents(true);

    connect(source, &Port::moved,
            this, &Connection::onPortsMoved);
    connect(source, &Port::hiddenChanged,
            this, &Connection::onHiddenChanged);
    connect(source, &QObject::destroyed,
            this, &Connection::onPortDeleted);

    if (target)
        makeTargetConnections();
}

void Connection::makeTargetConnections()
{
    connect(target, &Port::moved,
            this, &Connection::onPortsMoved);
    connect(target, &Port::hiddenChanged,
            this, &Connection::onHiddenChanged);
    connect(target, &QObject::destroyed,
            this, &Connection::onPortDeleted);
}

void Connection::onPortDeleted()
{
    source = NULL;
    target = NULL;
    deleteLater();
}

void Connection::onPortsMoved()
{
    prepareGeometryChange();
}

void Connection::onHiddenChanged()
{
    if (isHidden())
        hide();
    else
        show();
    prepareGeometryChange();
}

GraphScene* Connection::gscene() const
{
    return static_cast<GraphScene*>(scene());
}

QRectF Connection::boundingRect() const
{
    QPainterPathStroker s;
    s.setWidth(20);
    return s.createStroke(path()).boundingRect();
}

QPainterPath Connection::shape() const
{
    QPainterPathStroker s;
    s.setWidth(20);
    return s.createStroke(path(true));
}

QPointF Connection::startPos() const
{
    Q_ASSERT(source != NULL);
    return source->mapToScene(source->boundingRect().center());
}

QPointF Connection::endPos() const
{
    Q_ASSERT(target != NULL || drag_state != CONNECTED);
    if (drag_state == CONNECTED)
        return target->mapToScene(target->boundingRect().center());
    else
        return (snapping && has_snap_pos) ? snap_pos : drag_pos;
}

bool Connection::isHidden() const
{
    if (!source->isVisible())
        return true;
    if (drag_state == CONNECTED && !target->isVisible())
        return true;
    return false;
}

QPainterPath Connection::path(bool only_bezier) const
{
    if (source == NULL)
        return QPainterPath();

    QPointF start = startPos();
    QPointF end = endPos();

    float length = 50;
    if (end.x() <= start.x())
    {
        length += (start.x() - end.x()) / 2;
    }

    QPainterPath p;
    p.moveTo(start);
    if (only_bezier)
        p.moveTo(start + QPointF(15, 0));
    else
        p.lineTo(start + QPointF(15, 0));

    p.cubicTo(QPointF(start.x() + length, start.y()),
              QPointF(end.x() - length, end.y()),
              QPointF(end.x() - 15, end.y()));

    if (!only_bezier)
        p.lineTo(end);

   return p;
}

void Connection::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    if (hover)
    {
        painter->setPen(QPen(QColor(255, 255, 255, Colors::base02.red()), 20));
        painter->drawPath(path(true));
    }

    QColor color = Colors::getColor(source->getDatum());
    if (drag_state == INVALID)
        color = Colors::red;
    if (isSelected() || drag_state == VALID)
        color = Colors::highlight(color);

    painter->setPen(QPen(color, 4));
    painter->drawPath(path());
}

void Connection::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (drag_state == CONNECTED)
        return;

    drag_pos = event->pos();
    if (snapping)
        updateSnap();

    gscene()->raiseInspectorAt(drag_pos);

    checkDragTarget();
    prepareGeometryChange();
}

void Connection::checkDragTarget()
{
    target = gscene()->getInputPortAt(endPos());

    if (target && target->getDatum()->acceptsLink(source->getDatum()))
        drag_state = VALID;
    else if (target)
        drag_state = INVALID;
    else
        drag_state = NONE;
}

void Connection::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsObject::mouseReleaseEvent(event);
    if (drag_state == CONNECTED)
        return;

    ungrabMouse();
    clearFocus();
    setFlag(QGraphicsItem::ItemIsFocusable, false);

    InputPort* t = gscene()->getInputPortAt(endPos());
    Datum* datum = target ? target->getDatum() : NULL;
    if (t && datum->acceptsLink(source->getDatum()))
    {
        datum->installLink(source->getDatum());
        drag_state = CONNECTED;

        target = t;
        makeTargetConnections();

        //App::instance()->pushStack(new UndoAddLinkCommand(link));
    }
    else
    {
        deleteLater();
    }

    prepareGeometryChange();
}

void Connection::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    if (!hover)
    {
        hover = true;
        update();
    }
}

void Connection::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    if (hover)
    {
        hover = false;
        update();
    }
}

void Connection::updateSnap()
{
    if (Port* p = gscene()->getInputPortNear(drag_pos, source->getDatum()))
    {
        has_snap_pos = true;
        snap_pos = p->mapToScene(p->boundingRect().center());
    }
    else
    {
        has_snap_pos = false;
    }
}

void Connection::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && drag_state != CONNECTED)
    {
        snapping = true;
        updateSnap();
        checkDragTarget();
        prepareGeometryChange();
    }
    else
    {
        event->ignore();
    }
}

void Connection::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && drag_state != CONNECTED)
    {
        snapping = false;
        checkDragTarget();
        prepareGeometryChange();
    }
}
