#pragma once

#include <QCoreApplication>
#include <QEvent>
#include <QList>
#include <QObject>
#include <QPointer>

// RAII scope guard that swallows QEvent::DeferredDelete events application-wide
// while active, then re-posts them when it goes out of scope.
//
// Intended use: wrap any code path that spins a nested Qt event loop from
// inside a QML signal handler (typically a synchronous RPC via
// QConnectedReplicaImplementation::waitForSource). Without the guard, Qt's
// event dispatcher processes posted DeferredDelete events during the nested
// loop, and if any of those events destroys an ancestor of the QML item
// whose signal handler is on the call stack, QQmlData::destroyed aborts
// with:
//
//   QQmlData::destroyed called qFatal — "Object destroyed while one of
//   its QML signal handlers is in progress"
//
// The guard doesn't lose events — they're held in a QPointer<QObject> list
// and re-posted via deleteLater() when the guard is destroyed, so they
// land on the next event-loop iteration, after the outer signal handler
// has unwound.
//
// Scope: installed on QCoreApplication::instance(), so it sees posted
// events destined for main-thread objects only. That matches where this
// crash happens (QML is main-thread). Cost is one virtual eventFilter call
// per event during the guard's active window (milliseconds to seconds);
// negligible in practice.
//
// QPointer guards against the held target being deleted through another
// path while the guard is active — re-posting deleteLater on a null
// QPointer is a no-op.
class DeferredDeleteGuard : public QObject {
public:
    DeferredDeleteGuard() {
        if (auto* app = QCoreApplication::instance()) {
            app->installEventFilter(this);
        }
    }

    ~DeferredDeleteGuard() override {
        if (auto* app = QCoreApplication::instance()) {
            app->removeEventFilter(this);
        }
        // Re-arm held deletions. Each call posts a fresh DeferredDelete
        // that will be delivered on the next event-loop iteration, which
        // — by the time we're being destructed — is after the caller's
        // signal handler has returned.
        for (const QPointer<QObject>& target : std::as_const(m_held)) {
            if (target) target->deleteLater();
        }
    }

    DeferredDeleteGuard(const DeferredDeleteGuard&) = delete;
    DeferredDeleteGuard& operator=(const DeferredDeleteGuard&) = delete;

    // For tests: number of DeferredDelete events held so far.
    int heldCount() const { return static_cast<int>(m_held.size()); }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event && event->type() == QEvent::DeferredDelete) {
            m_held.append(QPointer<QObject>(watched));
            return true;  // swallow — will be re-posted on dtor
        }
        return false;
    }

private:
    QList<QPointer<QObject>> m_held;
};
