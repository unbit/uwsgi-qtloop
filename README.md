uwsgi-qtloop
============

Qt loop engine for uWSGI

This plugins allows you to integrate Qt apps in uWSGI.

The plugin is 2.x compliant so you can build it in one shot with:

```sh
uwsgi --build-plugin https://github.com/unbit/uwsgi-qtloop
```

By default a QtCoreApplication is instantiated, so you can use QtCore objects in your request handlers.

You can even mix a graphic application (QApplication) by adding ``--qtloop-gui`` flag to your options.

For example you can have a Django self-contained app:

```py
# superdjango.py

# import PyQt4 modules
from PyQt4.Qt import QMainWindow
from PyQt4 import QtCore
from PyQt4 import QtWebKit

# create the main window
win = QMainWindow()
win.resize(1280, 1024)
win.setWindowTitle('Django')

# create the webkit widget
html = QtWebKit.QWebView()
html.load(QtCore.QUrl('http://127.0.0.1:8080/'))
html.show()

# show the main window
win.setCentralWidget(html)
win.show()

# load the django app
# here change with your project name
from project.wsgi import application

```

```ini
[uwsgi]
; be sure to run uWSGI from the django project directory
plugins = python,qtloop
; force gui mode
qtloop-gui = true
; force uWSGI to use the 'qt' loop engine
loop = qt
; load the app
wsgi-file = superdjango.py
; bind on port 8080
http-socket = 127.0.0.1:8080
; spawn 8 threads
threads = 8
```

