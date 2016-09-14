QT += \
	core \
	gui \
	network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = NRAConnector
TEMPLATE = app

QMAKE_CXXFLAGS += -msse

SOURCES += \
	main.cpp\
	mainwindow.cpp \
	nraconnector.cpp \
	rtlserver.cpp \
	sseinterpolator.cpp

HEADERS += \
	mainwindow.h \
	nraconnector.h \
	rtlserver.h \
	dsptypes.h \
	sseinterpolator.h

FORMS += \
	mainwindow.ui
