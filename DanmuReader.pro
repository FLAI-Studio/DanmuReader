QT += core gui websockets texttospeech
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = DanmuReader
TEMPLATE = app
CONFIG += c++17

SOURCES += main.cpp mainwindow.cpp
HEADERS += mainwindow.h