#include "app/undo/undo_move.h"
#include "ui/canvas/inspector/inspector.h"

UndoMoveCommand::UndoMoveCommand(GraphScene* g, Node* n, QPointF a, QPointF b)
    : g(g), n(n), a(a), b(b)
{
    setText("'move inspector'");
}

void UndoMoveCommand::redo()
{
    Q_ASSERT(g);
    Q_ASSERT(n);

    auto i = g->getInspector(n);
    Q_ASSERT(i);

    i->setPos(b);
}

void UndoMoveCommand::undo()
{
    Q_ASSERT(g);
    Q_ASSERT(n);

    auto i = g->getInspector(n);
    Q_ASSERT(i);

    i->setPos(a);
}

void UndoMoveCommand::swapNode(Node* a, Node* b) const
{
    if (a == n.data())
        n = b;
}


