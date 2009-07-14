######################################################################
# Automatically generated by qmake (2.01a) Mon Jul 13 10:40:01 2009
######################################################################

TEMPLATE = app
TARGET = cppcheck
DEPENDPATH += .
INCLUDEPATH += .
OBJECTS_DIR = temp
CONFIG += warn_on debug
QT -= gui core

# Input
HEADERS += check.h \
           checkautovariables.h \
           checkbufferoverrun.h \
           checkclass.h \
           checkdangerousfunctions.h \
           checkheaders.h \
           checkmemoryleak.h \
           checkother.h \
           checkstl.h \
           checkunusedfunctions.h \
           cppcheck.h \
           cppcheckexecutor.h \
           errorlogger.h \
           filelister.h \
           mathlib.h \
           preprocessor.h \
           resource.h \
           settings.h \
           threadexecutor.h \
           token.h \
           tokenize.h
SOURCES += checkautovariables.cpp \
           checkbufferoverrun.cpp \
           checkclass.cpp \
           checkdangerousfunctions.cpp \
           checkheaders.cpp \
           checkmemoryleak.cpp \
           checkother.cpp \
           checkstl.cpp \
           checkunusedfunctions.cpp \
           cppcheck.cpp \
           cppcheckexecutor.cpp \
           errorlogger.cpp \
           filelister.cpp \
           main.cpp \
           mathlib.cpp \
           preprocessor.cpp \
           settings.cpp \
           threadexecutor.cpp \
           token.cpp \
           tokenize.cpp

win32 {
    CONFIG += embed_manifest_exe console
    RC_FILE = cppcheck.rc
    HEADERS += resource.h
	LIBS += -lshlwapi
}