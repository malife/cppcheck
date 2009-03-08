/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2009 Daniel Marjamäki, Reijo Tomperi, Nicolas Le Cam,
 * Leandro Penz, Kimmo Varis, Vesa Pikki
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/
 */


#include "checkthread.h"
#include <QDebug>

CheckThread::CheckThread(ThreadResult &result) :
        mResult(result),
        mCppCheck(result)
{
    //ctor
}

CheckThread::~CheckThread()
{
    //dtor
}

void CheckThread::Check(Settings settings)
{
    mCppCheck.settings(settings);
    start();
}

void CheckThread::run()
{
    QString file;
    file = mResult.GetNextFile();

    while (!file.isEmpty())
    {
        qDebug() << "Checking file" << file;
        mCppCheck.addFile(file.toStdString());
        mCppCheck.check();
        mCppCheck.clearFiles();
        emit FileChecked(file);

        file = mResult.GetNextFile();
    }

    emit Done();
}


