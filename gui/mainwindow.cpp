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

#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QMenu>
#include <QDirIterator>
#include <QMenuBar>
#include <QMessageBox>
#include <QToolBar>
#include <QKeySequence>
#include <QFileInfo>
#include <QDir>
#include "aboutdialog.h"
#include "threadhandler.h"
#include "fileviewdialog.h"
#include "projectfile.h"
#include "project.h"
#include "report.h"
#include "statsdialog.h"
#include "logview.h"
#include "filelist.h"
#include "helpwindow.h"

MainWindow::MainWindow() :
    mSettings(new QSettings("Cppcheck", "Cppcheck-GUI", this)),
    mApplications(new ApplicationList(this)),
    mTranslation(new TranslationHandler(this)),
    mLanguages(new QActionGroup(this)),
    mLogView(NULL),
    mHelpWindow(NULL),
    mProject(NULL),
    mExiting(false)
{
    mUI.setupUi(this);
    mUI.mResults->Initialize(mSettings, mApplications);

    mThread = new ThreadHandler(this);
    mLogView = new LogView(mSettings);

    connect(mUI.mActionQuit, SIGNAL(triggered()), this, SLOT(close()));
    connect(mUI.mActionCheckFiles, SIGNAL(triggered()), this, SLOT(CheckFiles()));
    connect(mUI.mActionCheckDirectory, SIGNAL(triggered()), this, SLOT(CheckDirectory()));
    connect(mUI.mActionSettings, SIGNAL(triggered()), this, SLOT(ProgramSettings()));
    connect(mUI.mActionClearResults, SIGNAL(triggered()), this, SLOT(ClearResults()));
    connect(mUI.mActionOpenXML, SIGNAL(triggered()), this, SLOT(OpenXML()));

    connect(mUI.mActionShowStyle, SIGNAL(toggled(bool)), this, SLOT(ShowStyle(bool)));
    connect(mUI.mActionShowErrors, SIGNAL(toggled(bool)), this, SLOT(ShowErrors(bool)));
    connect(mUI.mActionCheckAll, SIGNAL(triggered()), this, SLOT(CheckAll()));
    connect(mUI.mActionUncheckAll, SIGNAL(triggered()), this, SLOT(UncheckAll()));
    connect(mUI.mActionCollapseAll, SIGNAL(triggered()), mUI.mResults, SLOT(CollapseAllResults()));
    connect(mUI.mActionExpandAll, SIGNAL(triggered()), mUI.mResults, SLOT(ExpandAllResults()));
    connect(mUI.mActionViewLog, SIGNAL(triggered()), this, SLOT(ShowLogView()));
    connect(mUI.mActionViewStats, SIGNAL(triggered()), this, SLOT(ShowStatistics()));

    connect(mUI.mActionRecheck, SIGNAL(triggered()), this, SLOT(ReCheck()));

    connect(mUI.mActionStop, SIGNAL(triggered()), this, SLOT(StopChecking()));
    connect(mUI.mActionSave, SIGNAL(triggered()), this, SLOT(Save()));

    connect(mUI.mActionAbout, SIGNAL(triggered()), this, SLOT(About()));
    connect(mUI.mActionLicense, SIGNAL(triggered()), this, SLOT(ShowLicense()));
    connect(mUI.mActionToolBarMain, SIGNAL(toggled(bool)), this, SLOT(ToggleMainToolBar()));
    connect(mUI.mActionToolBarView, SIGNAL(toggled(bool)), this, SLOT(ToggleViewToolBar()));

    connect(mUI.mActionAuthors, SIGNAL(triggered()), this, SLOT(ShowAuthors()));
    connect(mThread, SIGNAL(Done()), this, SLOT(CheckDone()));
    connect(mUI.mResults, SIGNAL(GotResults()), this, SLOT(ResultsAdded()));
    connect(mUI.mMenuView, SIGNAL(aboutToShow()), this, SLOT(AboutToShowViewMenu()));

    connect(mUI.mActionNewProjectFile, SIGNAL(triggered()), this, SLOT(NewProjectFile()));
    connect(mUI.mActionOpenProjectFile, SIGNAL(triggered()), this, SLOT(OpenProjectFile()));
    connect(mUI.mActionCloseProjectFile, SIGNAL(triggered()), this, SLOT(CloseProjectFile()));
    connect(mUI.mActionEditProjectFile, SIGNAL(triggered()), this, SLOT(EditProjectFile()));

    connect(mUI.mActionHelpContents, SIGNAL(triggered()), this, SLOT(OpenHelpContents()));

    CreateLanguageMenuItems();
    LoadSettings();

    mThread->Initialize(mUI.mResults);
    FormatAndSetTitle();

    EnableCheckButtons(true);

    mUI.mActionClearResults->setEnabled(false);
    mUI.mActionSave->setEnabled(false);
    mUI.mActionRecheck->setEnabled(false);
    EnableProjectOpenActions(true);
    EnableProjectActions(false);

    QStringList args = QCoreApplication::arguments();
    //Remove the application itself
    args.removeFirst();
    if (!args.isEmpty())
    {
        DoCheckFiles(args);
    }
}

MainWindow::~MainWindow()
{
    delete mLogView;
    delete mHelpWindow;
    delete mProject;
}

void MainWindow::CreateLanguageMenuItems()
{
    QStringList languages = mTranslation->GetNames();

    for (int i = 0; i < languages.size(); i++)
    {
        //Create an action for each language
        //Language name is pre translated
        QAction *temp = new QAction(languages[i], this);

        temp->setCheckable(true);

        //Add the action to menu
        mUI.mMenuLanguage->addAction(temp);

        //Add action to the group
        mLanguages->addAction(temp);

        //Check it if it's the value stored to settings
        if (i == mSettings->value(SETTINGS_LANGUAGE, 0).toInt())
        {
            temp->setChecked(true);
        }
        else
        {
            temp->setChecked(false);
        }
    }

    connect(mLanguages, SIGNAL(triggered(QAction *)),
            this, SLOT(MapLanguage(QAction *)));
}

void MainWindow::LoadSettings()
{
    if (mSettings->value(SETTINGS_WINDOW_MAXIMIZED, false).toBool())
    {
        showMaximized();
    }
    else
    {
        resize(mSettings->value(SETTINGS_WINDOW_WIDTH, 800).toInt(),
               mSettings->value(SETTINGS_WINDOW_HEIGHT, 600).toInt());
    }

    mUI.mActionShowStyle->setChecked(mSettings->value(SETTINGS_SHOW_STYLE, true).toBool());
    mUI.mActionShowErrors->setChecked(mSettings->value(SETTINGS_SHOW_ERRORS, true).toBool());

    mUI.mResults->ShowResults(SHOW_ERRORS, mUI.mActionShowErrors->isChecked());
    mUI.mResults->ShowResults(SHOW_STYLE, mUI.mActionShowStyle->isChecked());

    mUI.mActionToolBarMain->setChecked(mSettings->value(SETTINGS_TOOLBARS_MAIN_SHOW, true).toBool());
    mUI.mToolBarMain->setVisible(mSettings->value(SETTINGS_TOOLBARS_MAIN_SHOW, true).toBool());
    mUI.mToolBarView->setVisible(mSettings->value(SETTINGS_TOOLBARS_VIEW_SHOW, true).toBool());

    mApplications->LoadSettings(mSettings);

    SetLanguage(mSettings->value(SETTINGS_LANGUAGE, mTranslation->SuggestLanguage()).toInt());
}

void MainWindow::SaveSettings()
{
    //Force toolbar checkbox value to be updated
    AboutToShowViewMenu();

    mSettings->setValue(SETTINGS_WINDOW_WIDTH, size().width());
    mSettings->setValue(SETTINGS_WINDOW_HEIGHT, size().height());
    mSettings->setValue(SETTINGS_WINDOW_MAXIMIZED, isMaximized());

    mSettings->setValue(SETTINGS_SHOW_STYLE, mUI.mActionShowStyle->isChecked());
    mSettings->setValue(SETTINGS_SHOW_ERRORS, mUI.mActionShowErrors->isChecked());
    mSettings->setValue(SETTINGS_TOOLBARS_MAIN_SHOW, mUI.mToolBarMain->isVisible());
    mSettings->setValue(SETTINGS_TOOLBARS_VIEW_SHOW, mUI.mToolBarView->isVisible());

    mApplications->SaveSettings(mSettings);

    mSettings->setValue(SETTINGS_LANGUAGE, mTranslation->GetCurrentLanguage());
    mUI.mResults->SaveSettings();
}

void MainWindow::DoCheckFiles(const QStringList &files)
{
    if (files.isEmpty())
    {
        return;
    }
    ClearResults();

    FileList pathList;
    pathList.AddPathList(files);
    QStringList fileNames = pathList.GetFileList();

    mUI.mResults->Clear();
    mThread->ClearFiles();

    if (fileNames.isEmpty())
    {
        QMessageBox msg(QMessageBox::Warning,
                        tr("Cppcheck"),
                        tr("No suitable files found to check!"),
                        QMessageBox::Ok,
                        this);
        msg.exec();
        return;
    }

    mUI.mResults->CheckingStarted(fileNames.count());

    mThread->SetFiles(fileNames);
    QDir inf(mCurrentDirectory);
    const QString checkPath = inf.canonicalPath();
    mSettings->setValue(SETTINGS_CHECK_PATH, checkPath);
    EnableCheckButtons(false);
    mUI.mActionSettings->setEnabled(false);
    mUI.mActionOpenXML->setEnabled(false);
    mUI.mResults->SetCheckDirectory(checkPath);

    Settings checkSettings = GetCppcheckSettings();
    EnableProjectActions(false);
    EnableProjectOpenActions(false);
    mThread->Check(checkSettings, false);
}

QStringList MainWindow::SelectFilesToCheck(QFileDialog::FileMode mode)
{
    if (mProject)
    {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Cppcheck"));
        const QString msg(tr("You must close the project file before selecting new files or directories!"));
        msgBox.setText(msg);
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.exec();
        return QStringList();
    }

    QStringList selected;

    // NOTE: we use QFileDialog::getOpenFileNames() and
    // QFileDialog::getExistingDirectory() because they show native Windows
    // selection dialog which is a lot more usable than QT:s own dialog.
    if (mode == QFileDialog::ExistingFiles)
    {
        selected = QFileDialog::getOpenFileNames(this,
                   tr("Select files to check"),
                   mSettings->value(SETTINGS_CHECK_PATH, "").toString());
        if (selected.isEmpty())
            mCurrentDirectory.clear();
        else
        {
            QFileInfo inf(selected[0]);
            mCurrentDirectory = inf.absolutePath();
        }
        FormatAndSetTitle();
    }
    else if (mode == QFileDialog::DirectoryOnly)
    {
        QString dir = QFileDialog::getExistingDirectory(this,
                      tr("Select directory to check"),
                      mSettings->value(SETTINGS_CHECK_PATH, "").toString());
        if (!dir.isEmpty())
        {
            qDebug() << "Setting current directory to: " << dir;
            mCurrentDirectory = dir;
            selected.append(dir);
            dir = QDir::toNativeSeparators(dir);
            FormatAndSetTitle(dir);
        }
    }

    return selected;
}

void MainWindow::CheckFiles()
{
    DoCheckFiles(SelectFilesToCheck(QFileDialog::ExistingFiles));
}

void MainWindow::CheckDirectory()
{
    DoCheckFiles(SelectFilesToCheck(QFileDialog::DirectoryOnly));
}

bool MainWindow::GetCheckProject()
{
    // We have succesfully loaded project earlier and use that project
    if (mProject)
        return true;

    if (!mCurrentDirectory.isEmpty())
    {
        // Format project filename (directory name + .cppcheck) and load
        // the project file if it is found.
        QStringList parts = mCurrentDirectory.split("/");
        const QString filename = parts[parts.count() - 1] + ".cppcheck";;
        const QString projfile = mCurrentDirectory + "/" + filename;
        if (QFile::exists(projfile))
        {
            qDebug() << "Opening project file: " << projfile;
            mProject = new Project();
            mProject->SetFilename(projfile);
            FormatAndSetTitle(tr("Project: ") + QString(" ") + filename);
            return mProject->Open();
        }
    }
    return false;
}

Settings MainWindow::GetCppcheckSettings()
{
    Settings result;
    bool projectRead = GetCheckProject();
    if (projectRead)
    {
        ProjectFile *pfile = mProject->GetProjectFile();
        QStringList dirs = pfile->GetIncludeDirs();
        QString dir;
        foreach(dir, dirs)
        {
            QString incdir;
            if (!QDir::isAbsolutePath(dir))
                incdir = mCurrentDirectory + "/";
            incdir += dir;
            incdir = QDir::cleanPath(incdir);

            // include paths must end with '/'
            if (!incdir.endsWith("/"))
                incdir += "/";
            result._includePaths.push_back(incdir.toStdString());
        }
        QStringList defines = pfile->GetDefines();
        QString define;
        foreach(define, defines)
        {
            if (!result.userDefines.empty())
                result.userDefines += ";";
            result.userDefines += define.toStdString();
        }
    }

    result.debug = false;
    result.debugwarnings = mSettings->value(SETTINGS_SHOW_DEBUG_WARNINGS, 0).toBool();
    result._checkCodingStyle = true;
    result._errorsOnly = false;
    result._verbose = true;
    result._force = mSettings->value(SETTINGS_CHECK_FORCE, 1).toBool();
    result._xml = false;
    result._jobs = mSettings->value(SETTINGS_CHECK_THREADS, 1).toInt();

    if (result._jobs <= 0)
    {
        result._jobs = 1;
    }

    return result;
}

void MainWindow::CheckDone()
{
    if (mExiting)
    {
        close();
        return;
    }

    mUI.mResults->CheckingFinished();
    EnableCheckButtons(true);
    mUI.mActionSettings->setEnabled(true);
    mUI.mActionOpenXML->setEnabled(true);
    EnableProjectActions(true);
    EnableProjectOpenActions(true);

    if (mUI.mResults->HasResults())
    {
        mUI.mActionClearResults->setEnabled(true);
        mUI.mActionSave->setEnabled(true);
    }

    // Notify user - if the window is not active - that check is ready
    QApplication::alert(this, 3000);
}

void MainWindow::ProgramSettings()
{
    SettingsDialog dialog(mSettings, mApplications, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        dialog.SaveCheckboxValues();
        mUI.mResults->UpdateSettings(dialog.ShowFullPath(),
                                     dialog.SaveFullPath(),
                                     dialog.SaveAllErrors(),
                                     dialog.ShowNoErrorsMessage());
    }
}

void MainWindow::ReCheck()
{
    ClearResults();
    EnableCheckButtons(false);

    const int filesCount = mThread->GetPreviousFilesCount();
    Q_ASSERT(filesCount > 0); // If no files should not be able to recheck
    mUI.mResults->CheckingStarted(filesCount);

    mThread->Check(GetCppcheckSettings(), true);
}

void MainWindow::ClearResults()
{
    mUI.mResults->Clear();
    mUI.mActionClearResults->setEnabled(false);
    mUI.mActionSave->setEnabled(false);
}

void MainWindow::OpenXML()
{
    QString selectedFilter;
    QString filter(tr("XML files (*.xml)"));
    QString selectedFile = QFileDialog::getOpenFileName(this,
                           tr("Open the report file"),
                           QString(),
                           filter,
                           &selectedFilter);

    if (!selectedFile.isEmpty())
    {
        mUI.mResults->ReadErrorsXml(selectedFile);
    }
}

void MainWindow::EnableCheckButtons(bool enable)
{
    mUI.mActionStop->setEnabled(!enable);
    mUI.mActionCheckFiles->setEnabled(enable);

    if (!enable || mThread->HasPreviousFiles())
        mUI.mActionRecheck->setEnabled(enable);

    mUI.mActionCheckDirectory->setEnabled(enable);
}

void MainWindow::ShowStyle(bool checked)
{
    mUI.mResults->ShowResults(SHOW_STYLE, checked);
}

void MainWindow::ShowErrors(bool checked)
{
    mUI.mResults->ShowResults(SHOW_ERRORS, checked);
}

void MainWindow::CheckAll()
{
    ToggleAllChecked(true);
}

void MainWindow::UncheckAll()
{
    ToggleAllChecked(false);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Check that we aren't checking files
    if (!mThread->IsChecking())
    {
        delete mHelpWindow;
        mHelpWindow = NULL;

        SaveSettings();
        event->accept();
    }
    else
    {
        QString text(tr("Checking is running.\n\n" \
                        "Do you want to stop the checking and exit Cppcheck?."));

        QMessageBox msg(QMessageBox::Warning,
                        tr("Cppcheck"),
                        text,
                        QMessageBox::Yes | QMessageBox::No,
                        this);

        msg.setDefaultButton(QMessageBox::No);
        int rv = msg.exec();
        if (rv == QMessageBox::Yes)
        {
            // This isn't really very clean way to close threads but since the app is
            // exiting it doesn't matter.
            mThread->Stop();
            SaveSettings();
            mExiting = true;
        }
        event->ignore();
    }
}

void MainWindow::ToggleAllChecked(bool checked)
{
    mUI.mActionShowStyle->setChecked(checked);
    ShowStyle(checked);

    mUI.mActionShowErrors->setChecked(checked);
    ShowErrors(checked);
}

void MainWindow::About()
{
    AboutDialog *dlg = new AboutDialog(CppCheck::version(), this);
    dlg->exec();
}

void MainWindow::ShowLicense()
{
    FileViewDialog *dlg = new FileViewDialog(":COPYING", tr("License"), this);
    dlg->resize(570, 400);
    dlg->exec();
}

void MainWindow::ShowAuthors()
{
    FileViewDialog *dlg = new FileViewDialog(":AUTHORS", tr("Authors"), this);
    dlg->resize(350, 400);
    dlg->exec();
}

void MainWindow::Save()
{
    QString selectedFilter;
    QString filter(tr("XML files (*.xml);;Text files (*.txt);;CSV files (*.csv)"));
    QString selectedFile = QFileDialog::getSaveFileName(this,
                           tr("Save the report file"),
                           QString(),
                           filter,
                           &selectedFilter);

    if (!selectedFile.isEmpty())
    {
        Report::Type type = Report::TXT;
        if (selectedFilter == tr("XML files (*.xml)"))
        {
            type = Report::XML;
            if (!selectedFile.endsWith(".xml", Qt::CaseInsensitive))
                selectedFile += ".xml";
        }
        else if (selectedFilter == tr("Text files (*.txt)"))
        {
            type = Report::TXT;
            if (!selectedFile.endsWith(".txt", Qt::CaseInsensitive))
                selectedFile += ".txt";
        }
        else if (selectedFilter == tr("CSV files (*.csv)"))
        {
            type = Report::CSV;
            if (!selectedFile.endsWith(".csv", Qt::CaseInsensitive))
                selectedFile += ".csv";
        }
        else
        {
            if (selectedFile.endsWith(".xml", Qt::CaseInsensitive))
                type = Report::XML;
            else if (selectedFile.endsWith(".txt", Qt::CaseInsensitive))
                type = Report::TXT;
            else if (selectedFile.endsWith(".csv", Qt::CaseInsensitive))
                type = Report::CSV;
        }

        mUI.mResults->Save(selectedFile, type);
    }
}

void MainWindow::ResultsAdded()
{
}

void MainWindow::ToggleMainToolBar()
{
    mUI.mToolBarMain->setVisible(mUI.mActionToolBarMain->isChecked());
}

void MainWindow::ToggleViewToolBar()
{
    mUI.mToolBarView->setVisible(mUI.mActionToolBarView->isChecked());
}

void MainWindow::FormatAndSetTitle(const QString &text)
{
    QString title;
    if (text.isEmpty())
        title = tr("Cppcheck");
    else
        title = QString(tr("Cppcheck - %1")).arg(text);
    setWindowTitle(title);
}


void MainWindow::SetLanguage(int index)
{
    if (mTranslation->GetCurrentLanguage() == index)
    {
        return;
    }

    QString error;
    if (!mTranslation->SetLanguage(index, error))
    {
        QMessageBox msg(QMessageBox::Critical,
                        tr("Cppcheck"),
                        QString(tr("Failed to change the language:\n\n%1\n\n")).arg(error),
                        QMessageBox::Ok,
                        this);

        msg.exec();
    }
    else
    {
        //Translate everything that is visible here
        mUI.retranslateUi(this);
        mUI.mResults->Translate();
        QStringList languages = mTranslation->GetNames();
        QList<QAction *> actions = mLanguages->actions();

        if (languages.size() <= actions.size())
        {
            for (int i = 0; i < languages.size(); i++)
            {
                actions[i]->setText(tr(languages[i].toLatin1()));
            }
        }
    }
}

void MainWindow::MapLanguage(QAction *action)
{
    //Find the action that has the language that user clicked
    QList<QAction *> actions = mLanguages->actions();
    for (int i = 0; i < actions.size(); i++)
    {
        if (actions[i] == action)
        {
            SetLanguage(i);
        }
    }
}

void MainWindow::AboutToShowViewMenu()
{
    mUI.mActionToolBarMain->setChecked(mUI.mToolBarMain->isVisible());
    mUI.mActionToolBarView->setChecked(mUI.mToolBarView->isVisible());
}

void MainWindow::StopChecking()
{
    mThread->Stop();
    mUI.mResults->DisableProgressbar();
}

void MainWindow::OpenHelpContents()
{
    OpenHtmlHelpContents();
}

void MainWindow::OpenHtmlHelpContents()
{
    if (mHelpWindow == NULL)
    {
        const QString fname = qApp->applicationDirPath() + "/online-help.qhc";
        if (!QFileInfo(fname).exists())
        {
            QMessageBox::warning(this, tr("Cppcheck Help"), tr("Failed to load help file (not found)"));
            return;
        }

        mHelpWindow = new HelpWindow;
        if (!mHelpWindow->load(fname))
        {
            delete mHelpWindow;
            mHelpWindow = NULL;
            QMessageBox::warning(this, tr("Cppcheck Help"), tr("Failed to load help file"));
            return;
        }
    }

    mHelpWindow->show();
    if (!mHelpWindow->isActiveWindow())
        mHelpWindow->activateWindow();
}

void MainWindow::OpenProjectFile()
{
    delete mProject;

    const QString filter = tr("Project files (*.cppcheck);;All files(*.*)");
    QString filepath = QFileDialog::getOpenFileName(this,
                       tr("Select Project File"),
                       QString(),
                       filter);

    if (!filepath.isEmpty())
    {
        QFileInfo inf(filepath);
        const QString filename = inf.fileName();
        FormatAndSetTitle(tr("Project: ") + QString(" ") + filename);

        mUI.mActionCloseProjectFile->setEnabled(true);
        mUI.mActionEditProjectFile->setEnabled(true);
        mProject = new Project(filepath, this);
        mProject->Open();
        QString rootpath = mProject->GetProjectFile()->GetRootPath();

        // If root path not give or "current dir" then use project file's directory
        // as check path
        if (rootpath.isEmpty() || rootpath == ".")
            mCurrentDirectory = inf.canonicalPath();
        else
            mCurrentDirectory = rootpath;

        QStringList paths = mProject->GetProjectFile()->GetCheckPaths();
        if (!paths.isEmpty())
        {
            for (int i = 0; i < paths.size(); i++)
            {
                if (!QDir::isAbsolutePath(paths[i]))
                {
                    QString path = mCurrentDirectory + "/";
                    path += paths[i];
                    paths[i] = QDir::cleanPath(path);
                }
            }

            DoCheckFiles(paths);
        }
    }
}

void MainWindow::NewProjectFile()
{
    const QString filter = tr("Project files (*.cppcheck);;All files(*.*)");
    QString filepath = QFileDialog::getSaveFileName(this,
                       tr("Select Project Filename"),
                       QString(),
                       filter);

    if (!filepath.isEmpty())
    {
        EnableProjectActions(true);
        QFileInfo inf(filepath);
        const QString filename = inf.fileName();
        FormatAndSetTitle(tr("Project: ") + QString(" ") + filename);

        delete mProject;
        mProject = new Project(filepath, this);
        mProject->Create();
        mProject->Edit();
    }
}

void MainWindow::CloseProjectFile()
{
    delete mProject;
    mProject = NULL;
    EnableProjectActions(false);
    EnableProjectOpenActions(true);
    FormatAndSetTitle();
}

void MainWindow::EditProjectFile()
{
    if (!mProject)
    {
        QMessageBox msg(QMessageBox::Critical,
                        tr("Cppcheck"),
                        QString(tr("No project file loaded")),
                        QMessageBox::Ok,
                        this);
        msg.exec();
        return;
    }
    mProject->Edit();
}

void MainWindow::ShowLogView()
{
    if (mLogView == NULL)
        mLogView = new LogView(mSettings);

    mLogView->show();
    if (!mLogView->isActiveWindow())
        mLogView->activateWindow();
}

void MainWindow::ShowStatistics()
{
    StatsDialog statsDialog(this);

    // Show a dialog with the previous scan statistics and project information
    if (mProject)
    {
        statsDialog.setProject(*mProject);
    }
    statsDialog.setPathSelected(mCurrentDirectory);
    statsDialog.setNumberOfFilesScanned(mThread->GetPreviousFilesCount());
    statsDialog.setScanDuration(mThread->GetPreviousScanDuration() / 1000.0);

    statsDialog.exec();
}

void MainWindow::Log(const QString &logline)
{
    if (mLogView)
    {
        mLogView->AppendLine(logline);
    }
}

void MainWindow::DebugError(const ErrorItem &item)
{
    if (mLogView)
    {
        mLogView->AppendLine(item.ToString());
    }
}

void MainWindow::EnableProjectActions(bool enable)
{
    mUI.mActionCloseProjectFile->setEnabled(enable);
    mUI.mActionEditProjectFile->setEnabled(enable);
}

void MainWindow::EnableProjectOpenActions(bool enable)
{
    mUI.mActionNewProjectFile->setEnabled(enable);
    mUI.mActionOpenProjectFile->setEnabled(enable);
}
