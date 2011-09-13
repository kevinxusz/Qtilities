# ***************************************************************************
# Copyright (c) 2009-2011, Jaco Naude
#
# See http://www.qtilities.org/licensing.html for licensing details.
#
# ***************************************************************************
# Qtilities Unit Tests Library
# ***************************************************************************
QTILITIES += project_management
QTILITIES += extension_system
include(../Qtilities.pri)
INCLUDEPATH += $$QTILITIES_INCLUDE/UnitTests

CONFIG += qt \
    dll \
    ordered

QT += gui
QT += xml
CONFIG += qtestlib

CONFIG(debug, debug|release) {
    TARGET = QtilitiesUnitTestsd$${QTILITIES_FILE_VER}
} else {
    TARGET = QtilitiesUnitTests$${QTILITIES_FILE_VER}
}

TEMPLATE = lib

DEFINES += UNIT_TESTS_LIBRARY
DESTDIR = $$QTILITIES_BIN

OBJECTS_DIR = $$QTILITIES_TEMP/UnitTests
MOC_DIR = $$QTILITIES_TEMP/UnitTests
RCC_DIR = $$QTILITIES_TEMP/UnitTests
UI_DIR = $$QTILITIES_TEMP/UnitTests

# --------------------------
# Files only included when Qtilities unit tests must be part of the library
# --------------------------
contains(DEFINES, QTILITIES_UNIT_TESTS) {
HEADERS += source/UnitTestsConstants.h \
        source/UnitTests_global.h \
        source/TestObserver.h \
        source/TestObserverRelationalTable.h \
        source/TestVersionNumber.h \
        source/TestExporting.h \
        source/TestSubjectIterator.h \
        source/TestTreeIterator.h \
        source/BenchmarkTests.h \
        source/TestNamingPolicyFilter.h \
        source/TestActivityPolicyFilter.h \
        source/TestSubjectTypeFilter.h \
        source/TestTreeFileItem.h \
        source/TestAbstractTreeItem.h \
        source/TestObjectManager.h

SOURCES += source/TestObserver.cpp \
        source/TestObserverRelationalTable.cpp \
        source/TestVersionNumber.cpp \
        source/TestExporting.cpp \
        source/TestSubjectIterator.cpp \
        source/TestTreeIterator.cpp \
        source/BenchmarkTests.cpp \
        source/TestNamingPolicyFilter.cpp \
        source/TestActivityPolicyFilter.cpp \
        source/TestSubjectTypeFilter.cpp \
        source/TestTreeFileItem.cpp \
        source/TestAbstractTreeItem.cpp \
        source/TestObjectManager.cpp
}

# --------------------------
# Extension Library Files
# --------------------------
HEADERS += source/UnitTestsConstants.h \
        source/UnitTests_global.h \
        source/ITestable.h \
        source/TestFrontend.h \
        source/DebugWidget.h \
        source/DropableListWidget.h \
        source/FunctionCallAnalyzer.h

SOURCES += source/TestFrontend.cpp \
        source/DebugWidget.cpp \
        source/DropableListWidget.cpp \
        source/FunctionCallAnalyzer.cpp

FORMS += source/TestFrontend.ui \
        source/DebugWidget.ui

