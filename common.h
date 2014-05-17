#include <uwsgi.h>
#include <QtCore/QCoreApplication>
#include <QSocketNotifier>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

class uWSGICoreHandler : public QObject {
        Q_OBJECT
        public:
                uWSGICoreHandler(struct wsgi_request *, int);
        public slots:
                void handle_request(int);
                void handle_signal(int);
        public:
                struct wsgi_request *wsgi_req;
		int core_id;
};

class uWSGIThreadsDispatcher : public QObject {
        Q_OBJECT
        private slots:
                void request_dispatch(int);
                void signal_dispatch(int);
};


class uWSGIThread : public QThread {
	Q_OBJECT
	public:
		void run();
        public:
                uWSGICoreHandler *uch;
};
