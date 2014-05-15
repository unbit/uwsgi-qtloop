import os
import inspect

# generate moc file
cwd = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
os.system('moc -o %s/common.moc.cc %s/common.h' % (cwd, cwd))

NAME='qtloop'
LIBS = os.popen('pkg-config --libs QtCore').read().rstrip().split()
CFLAGS = os.popen('pkg-config --cflags QtCore').read().rstrip().split()
LIBS.append('-lstdc++')
GCC_LIST = ['plugin', 'qtloop.cc', 'common.moc.cc']
