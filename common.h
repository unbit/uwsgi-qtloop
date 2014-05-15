#include <uwsgi.h>
#include <QtCore/QCoreApplication>
#include <QSocketNotifier>
#include <QThread>

class RequestHandler : public QObject {
        Q_OBJECT
        public:
                RequestHandler(struct wsgi_request *);
        private slots:
                void handle_request(int);
                void handle_signal(int);
        public:
                struct wsgi_request *wsgi_req;
};

