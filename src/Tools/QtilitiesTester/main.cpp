/****************************************************************************
**
** Copyright (c) 2009-2010, Jaco Naude
**
** This file is part of Qtilities which is released under the following
** licensing options.
**
** Option 1: Open Source
** Under this license Qtilities is free software: you can
** redistribute it and/or modify it under the terms of the GNU General
** Public License as published by the Free Software Foundation, either
** version 3 of the License, or (at your option) any later version.
**
** Qtilities is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Qtilities. If not, see http://www.gnu.org/licenses/.
**
** Option 2: Commercial
** Alternatively, this library is also released under a commercial license
** that allows the development of closed source proprietary applications
** without restrictions on licensing. For more information on this option,
** please see the project website's licensing page:
** http://www.qtilities.org/licensing.html
**
** If you are unsure which license is appropriate for your use, please
** contact support@qtilities.org.
**
****************************************************************************/

#include <QApplication>

#include <QtilitiesCoreGui>
using namespace QtilitiesCoreGui;

#include <QtilitiesUnitTests>
using namespace QtilitiesUnitTests;

int main(int argc, char *argv[])
{
    QtilitiesApplication a(argc, argv);
    QtilitiesApplication::setOrganizationName("Jaco Naude");
    QtilitiesApplication::setOrganizationDomain("Qtilities");
    QtilitiesApplication::setApplicationName("Qtilities Tester");
    QtilitiesApplication::setApplicationVersion(QtilitiesApplication::qtilitiesVersion());

    QMainWindow* main_window = new QMainWindow();
    QtilitiesApplication::setMainWindow(main_window);
    LOG_INITIALIZE();
    Log->setGlobalLogLevel(Logger::Debug);
    Log->setIsQtMessageHandler(true);
    Log->toggleQtMsgEngine(false);
    QWidget* log_widget = LoggerGui::createLogWidget("Test Log",true);
    main_window->setCentralWidget(log_widget);
    log_widget->show();
    main_window->resize(900,400);
    main_window->show();

    // Observer Tests:
    TestObserver test1;
    QTest::qExec(&test1);

    return a.exec();
}