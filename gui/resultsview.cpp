/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2010 Daniel Marjamäki and Cppcheck team.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include "erroritem.h"
#include "resultsview.h"
#include "resultstree.h"
#include "report.h"
#include "txtreport.h"
#include "xmlreport.h"
#include "csvreport.h"

ResultsView::ResultsView(QWidget * parent) :
    QWidget(parent),
    mErrorsFound(false),
    mShowNoErrorsMessage(true)
{
    mUI.setupUi(this);
}

void ResultsView::Initialize(QSettings *settings, ApplicationList *list)
{

    mUI.mProgress->setMinimum(0);
    mUI.mProgress->setVisible(false);
    mShowNoErrorsMessage = settings->value(SETTINGS_SHOW_NO_ERRORS, true).toBool();

    mUI.mTree->Initialize(settings, list);
}


ResultsView::~ResultsView()
{
    //dtor
}


void ResultsView::Clear()
{
    mUI.mTree->Clear();
    mErrorsFound = false;

    //Clear the progressbar
    mUI.mProgress->setMaximum(100);
    mUI.mProgress->setValue(0);
}

void ResultsView::Progress(int value)
{
    mUI.mProgress->setValue(value);
}

void ResultsView::Error(const ErrorItem &item)
{
    mErrorsFound = true;
    mUI.mTree->AddErrorItem(item);
    emit GotResults();
}

void ResultsView::ShowResults(ShowTypes type, bool show)
{
    mUI.mTree->ShowResults(type, show);
}

void ResultsView::CollapseAllResults()
{
    mUI.mTree->collapseAll();
}

void ResultsView::ExpandAllResults()
{
    mUI.mTree->expandAll();
}

void ResultsView::Save(const QString &filename, Report::Type type)
{
    if (!mErrorsFound)
    {
        QMessageBox msgBox;
        msgBox.setText(tr("No errors found, nothing to save."));
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
    }

    Report *report = NULL;

    switch (type)
    {
    case Report::CSV:
        report = new CsvReport(filename, this);
        break;
    case Report::TXT:
        report = new TxtReport(filename, this);
        break;
    case Report::XML:
        report = new XmlReport(filename, this);
        break;
    }

    if (report)
    {
        if (report->Create())
            mUI.mTree->SaveResults(report);
        else
        {
            QMessageBox msgBox;
            msgBox.setText(tr("Failed to save the report."));
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.exec();
        }
        delete report;
        report = NULL;
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText(tr("Failed to save the report."));
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
    }
}

void ResultsView::UpdateSettings(bool showFullPath,
                                 bool saveFullPath,
                                 bool saveAllErrors,
                                 bool showNoErrorsMessage)
{
    mUI.mTree->UpdateSettings(showFullPath, saveFullPath, saveAllErrors);
    mShowNoErrorsMessage = showNoErrorsMessage;
}

void ResultsView::SetCheckDirectory(const QString &dir)
{
    mUI.mTree->SetCheckDirectory(dir);
}

void ResultsView::CheckingStarted(int count)
{
    mUI.mProgress->setVisible(true);
    mUI.mProgress->setMaximum(count);
}

void ResultsView::CheckingFinished()
{
    mUI.mProgress->setVisible(false);
    //Should we inform user of non visible/not found errors?
    if (mShowNoErrorsMessage)
    {
        //Tell user that we found no errors
        if (!mErrorsFound)
        {
            QMessageBox msg(QMessageBox::Information,
                            tr("Cppcheck"),
                            tr("No errors found."),
                            QMessageBox::Ok,
                            this);

            msg.exec();
        } //If we have errors but they aren't visible, tell user about it
        else if (!mUI.mTree->HasVisibleResults())
        {
            QString text = tr("Errors were found, but they are configured to be hidden.\n"\
                              "To toggle what kind of errors are shown, open view menu.");
            QMessageBox msg(QMessageBox::Information,
                            tr("Cppcheck"),
                            text,
                            QMessageBox::Ok,
                            this);

            msg.exec();
        }
    }
}

bool ResultsView::HasVisibleResults() const
{
    return mUI.mTree->HasVisibleResults();
}

bool ResultsView::HasResults() const
{
    return mUI.mTree->HasResults();
}

void ResultsView::SaveSettings()
{
    mUI.mTree->SaveSettings();
}

void ResultsView::Translate()
{
    mUI.mTree->Translate();
}

void ResultsView::DisableProgressbar()
{
    mUI.mProgress->setEnabled(false);
}

void ResultsView::ReadErrorsXml(const QString &filename)
{
    XmlReport *report = new XmlReport(filename, this);
    QList<ErrorLine> errors;
    if (report)
    {
        if (report->Open())
            errors = report->Read();
        else
        {
            QMessageBox msgBox;
            msgBox.setText(tr("Failed to read the report."));
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.exec();
        }
        delete report;
        report = NULL;
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText(tr("Failed to read the report."));
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
    }

    ErrorLine line;
    foreach(line, errors)
    {
        ErrorItem item(line);
        mUI.mTree->AddErrorItem(item);
    }
    mUI.mTree->SetCheckDirectory("");
}
