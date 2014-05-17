#include "common.h"

extern struct uwsgi_server uwsgi;

#define UWSGI_QT_EVENT_NULL	0
#define UWSGI_QT_EVENT_REQUEST	1
#define UWSGI_QT_EVENT_SIGNAL	2

QWaitCondition QueueReady;
QWaitCondition QueueFree;
QMutex mutex;

struct qt_uwsgi_status {
	int event;
	int fd;
} qt_uwsgi_status;

void uWSGIThread::run() {

	uwsgi_setup_thread_req(uch->core_id, uch->wsgi_req);

	while (uwsgi.workers[uwsgi.mywid].manage_next_request) {
		mutex.lock();
		if (qt_uwsgi_status.event == UWSGI_QT_EVENT_NULL) {
			QueueReady.wait(&mutex);
		}

		// this is required when using wakeAll()
		if (qt_uwsgi_status.event == UWSGI_QT_EVENT_NULL) {
			mutex.unlock();
			continue;
		}

		struct qt_uwsgi_status qitem;
		qitem.event = qt_uwsgi_status.event;
		qitem.fd = qt_uwsgi_status.fd;

		qt_uwsgi_status.event = UWSGI_QT_EVENT_NULL;

		QueueFree.wakeOne();
	
		mutex.unlock();

		// run the handler

		if (qitem.event == UWSGI_QT_EVENT_REQUEST) {
			uch->handle_request(qitem.fd);
		}
		else if (qitem.event == UWSGI_QT_EVENT_SIGNAL) {
			uch->handle_signal(qitem.fd);
		}
		
	}
}

void uWSGIThreadsDispatcher::request_dispatch(int fd) {
	mutex.lock();
	if (qt_uwsgi_status.event > UWSGI_QT_EVENT_NULL) {
		QueueFree.wait(&mutex);	
	}
	qt_uwsgi_status.event = UWSGI_QT_EVENT_REQUEST;
	qt_uwsgi_status.fd = fd;

	QueueReady.wakeOne();

	mutex.unlock();
	
}

void uWSGIThreadsDispatcher::signal_dispatch(int fd) {
        mutex.lock();
        if (qt_uwsgi_status.event > UWSGI_QT_EVENT_NULL) {
                QueueFree.wait(&mutex);
        }

        qt_uwsgi_status.event = UWSGI_QT_EVENT_SIGNAL;
        qt_uwsgi_status.fd = fd;

        QueueReady.wakeOne();

        mutex.unlock();

}


uWSGICoreHandler::uWSGICoreHandler(struct wsgi_request *wr, int core) {
	wsgi_req = wr;
	core_id = core;
}

// manage signal handlers
void uWSGICoreHandler::handle_signal(int fd) {
	uwsgi_receive_signal(fd, (char *) "worker", uwsgi.mywid);
}

// manage requests
void uWSGICoreHandler::handle_request(int fd) {
	struct uwsgi_socket *uwsgi_sock = uwsgi.sockets;
        while(uwsgi_sock) {
                if (uwsgi_sock->fd == fd) break;
                uwsgi_sock = uwsgi_sock->next;
        }

	if (!uwsgi_sock) return;

	wsgi_req_setup(wsgi_req, wsgi_req->async_id, uwsgi_sock );
	// mark core as used
        uwsgi.workers[uwsgi.mywid].cores[wsgi_req->async_id].in_request = 1;
	if (wsgi_req_simple_accept(wsgi_req, uwsgi_sock->fd)) {
		uwsgi.workers[uwsgi.mywid].cores[wsgi_req->async_id].in_request = 0;
		return;
	}

	wsgi_req->start_of_request = uwsgi_micros();
        wsgi_req->start_of_request_in_sec = wsgi_req->start_of_request/1000000;

	// enter harakiri mode
        if (uwsgi.harakiri_options.workers > 0) {
                set_harakiri(uwsgi.harakiri_options.workers);
        }

	for(;;) {
                int ret = uwsgi.wait_read_hook(wsgi_req->fd, uwsgi.socket_timeout);
                wsgi_req->switches++;

                if (ret <= 0) {
                        goto end;
                }

                int status = wsgi_req->socket->proto(wsgi_req);
                if (status < 0) {
                        goto end;
                }
                else if (status == 0) {
                        break;
                }
        }

#ifdef UWSGI_ROUTING
        if (uwsgi_apply_routes(wsgi_req) == UWSGI_ROUTE_BREAK) {
                goto end;
        }
#endif

        for(;;) {
                if (uwsgi.p[wsgi_req->uh->modifier1]->request(wsgi_req) <= UWSGI_OK) {
                        goto end;
                }
                wsgi_req->switches++;
	}

end:
	uwsgi_close_request(wsgi_req);
	return;
}

extern "C" void qtloop_loop() {
	// ensure SIGPIPE is ignored
        signal(SIGPIPE, SIG_IGN);

	// initialize Qt app
	QCoreApplication app(uwsgi.argc, uwsgi.argv);

        // create a QObject (you need one for each thread, async mode is not supported)
	if (uwsgi.threads > 1) {
		int i;
		// the first thread is used for the event loop
		for(i=1;i<uwsgi.threads;i++) {
			uWSGIThread *t = new uWSGIThread();
			uWSGICoreHandler *uch = new uWSGICoreHandler(&uwsgi.workers[uwsgi.mywid].cores[i].req, i);
			t->uch = uch;
			t->start();
		}

		uWSGIThreadsDispatcher *uts = new uWSGIThreadsDispatcher();

		// monitor signals
                if (uwsgi.signal_socket > -1) {
                        QSocketNotifier *signal_qsn = new QSocketNotifier(uwsgi.signal_socket, QSocketNotifier::Read, &app);
                        QObject::connect(signal_qsn, SIGNAL(activated(int)), uts, SLOT(signal_dispatch(int)));

                        QSocketNotifier *my_signal_qsn = new QSocketNotifier(uwsgi.my_signal_socket, QSocketNotifier::Read, &app);
                        QObject::connect(my_signal_qsn, SIGNAL(activated(int)), uts, SLOT(signal_dispatch(int)));
                }

                // monitor sockets
                struct uwsgi_socket *uwsgi_sock = uwsgi.sockets;
                while(uwsgi_sock) {
                        QSocketNotifier *qsn = new QSocketNotifier(uwsgi_sock->fd, QSocketNotifier::Read, &app);
                        QObject::connect(qsn, SIGNAL(activated(int)), uts, SLOT(request_dispatch(int)));
                        uwsgi_sock = uwsgi_sock->next;
                }
	}
	else {
        	uWSGICoreHandler *uch = new uWSGICoreHandler(&uwsgi.workers[uwsgi.mywid].cores[0].req, 0);

        	// monitor signals
		if (uwsgi.signal_socket > -1) {
			QSocketNotifier *signal_qsn = new QSocketNotifier(uwsgi.signal_socket, QSocketNotifier::Read, &app);
			QObject::connect(signal_qsn, SIGNAL(activated(int)), uch, SLOT(handle_signal(int)));

			QSocketNotifier *my_signal_qsn = new QSocketNotifier(uwsgi.my_signal_socket, QSocketNotifier::Read, &app);
			QObject::connect(my_signal_qsn, SIGNAL(activated(int)), uch, SLOT(handle_signal(int)));
		}

        	// monitor sockets
        	struct uwsgi_socket *uwsgi_sock = uwsgi.sockets;
        	while(uwsgi_sock) {
                	QSocketNotifier *qsn = new QSocketNotifier(uwsgi_sock->fd, QSocketNotifier::Read, &app);
                	QObject::connect(qsn, SIGNAL(activated(int)), uch, SLOT(handle_request(int)));
                	uwsgi_sock = uwsgi_sock->next;
        	}
	}

        // start the qt event loop
        app.exec();
}
