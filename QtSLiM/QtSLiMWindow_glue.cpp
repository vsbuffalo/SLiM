//
//  QtSLiMWindow_glue.cpp
//  SLiM
//
//  Created by Ben Haller on 7/11/2019.
//  Copyright (c) 2019-2020 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.


#include "QtSLiMWindow.h"
#include "ui_QtSLiMWindow.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QDesktopServices>

#include "QtSLiMScriptTextEdit.h"
#include "QtSLiMEidosConsole.h"
#include "QtSLiMAppDelegate.h"


void QtSLiMWindow::glueUI(void)
{
    // connect all QtSLiMWindow slots
    connect(ui->playOneStepButton, &QPushButton::clicked, this, &QtSLiMWindow::playOneStepClicked);
    connect(ui->playButton, &QPushButton::clicked, [this]() { playOrProfile(true); });
    connect(ui->profileButton, &QPushButton::clicked, [this]() { playOrProfile(false); });
    connect(ui->generationLineEdit, &QLineEdit::returnPressed, this, &QtSLiMWindow::generationChanged);
    connect(ui->recycleButton, &QPushButton::clicked, this, &QtSLiMWindow::recycleClicked);
    connect(ui->playSpeedSlider, &QSlider::valueChanged, this, &QtSLiMWindow::playSpeedChanged);

    connect(ui->showMutationsButton, &QPushButton::clicked, this, &QtSLiMWindow::showMutationsToggled);
    connect(ui->showFixedSubstitutionsButton, &QPushButton::clicked, this, &QtSLiMWindow::showFixedSubstitutionsToggled);
    connect(ui->showChromosomeMapsButton, &QPushButton::clicked, this, &QtSLiMWindow::showChromosomeMapsToggled);
    connect(ui->showGenomicElementsButton, &QPushButton::clicked, this, &QtSLiMWindow::showGenomicElementsToggled);

    connect(ui->checkScriptButton, &QPushButton::clicked, ui->scriptTextEdit, &QtSLiMTextEdit::checkScript);
    connect(ui->prettyprintButton, &QPushButton::clicked, ui->scriptTextEdit, &QtSLiMTextEdit::prettyprint);
    connect(ui->scriptHelpButton, &QPushButton::clicked, this, &QtSLiMWindow::scriptHelpClicked);
    connect(ui->consoleButton, &QPushButton::clicked, this, &QtSLiMWindow::showConsoleClicked);
    connect(ui->browserButton, &QPushButton::clicked, this, &QtSLiMWindow::showBrowserClicked);

    connect(ui->clearOutputButton, &QPushButton::clicked, this, &QtSLiMWindow::clearOutputClicked);
    connect(ui->dumpPopulationButton, &QPushButton::clicked, this, &QtSLiMWindow::dumpPopulationClicked);
    connect(ui->graphPopupButton, &QPushButton::clicked, this, &QtSLiMWindow::graphPopupButtonClicked);
    connect(ui->changeDirectoryButton, &QPushButton::clicked, this, &QtSLiMWindow::changeDirectoryClicked);

    // set up all icon-based QPushButtons to change their icon as they track
    connect(ui->playOneStepButton, &QPushButton::pressed, this, &QtSLiMWindow::playOneStepPressed);
    connect(ui->playOneStepButton, &QPushButton::released, this, &QtSLiMWindow::playOneStepReleased);
    connect(ui->playButton, &QPushButton::pressed, this, &QtSLiMWindow::playPressed);
    connect(ui->playButton, &QPushButton::released, this, &QtSLiMWindow::playReleased);
    connect(ui->profileButton, &QPushButton::pressed, this, &QtSLiMWindow::profilePressed);
    connect(ui->profileButton, &QPushButton::released, this, &QtSLiMWindow::profileReleased);
    connect(ui->recycleButton, &QPushButton::pressed, this, &QtSLiMWindow::recyclePressed);
    connect(ui->recycleButton, &QPushButton::released, this, &QtSLiMWindow::recycleReleased);
    connect(ui->showMutationsButton, &QPushButton::pressed, this, &QtSLiMWindow::showMutationsPressed);
    connect(ui->showMutationsButton, &QPushButton::released, this, &QtSLiMWindow::showMutationsReleased);
    connect(ui->showFixedSubstitutionsButton, &QPushButton::pressed, this, &QtSLiMWindow::showFixedSubstitutionsPressed);
    connect(ui->showFixedSubstitutionsButton, &QPushButton::released, this, &QtSLiMWindow::showFixedSubstitutionsReleased);
    connect(ui->showChromosomeMapsButton, &QPushButton::pressed, this, &QtSLiMWindow::showChromosomeMapsPressed);
    connect(ui->showChromosomeMapsButton, &QPushButton::released, this, &QtSLiMWindow::showChromosomeMapsReleased);
    connect(ui->showGenomicElementsButton, &QPushButton::pressed, this, &QtSLiMWindow::showGenomicElementsPressed);
    connect(ui->showGenomicElementsButton, &QPushButton::released, this, &QtSLiMWindow::showGenomicElementsReleased);
    connect(ui->checkScriptButton, &QPushButton::pressed, this, &QtSLiMWindow::checkScriptPressed);
    connect(ui->checkScriptButton, &QPushButton::released, this, &QtSLiMWindow::checkScriptReleased);
    connect(ui->prettyprintButton, &QPushButton::pressed, this, &QtSLiMWindow::prettyprintPressed);
    connect(ui->prettyprintButton, &QPushButton::released, this, &QtSLiMWindow::prettyprintReleased);
    connect(ui->scriptHelpButton, &QPushButton::pressed, this, &QtSLiMWindow::scriptHelpPressed);
    connect(ui->scriptHelpButton, &QPushButton::released, this, &QtSLiMWindow::scriptHelpReleased);
    connect(ui->consoleButton, &QPushButton::pressed, this, &QtSLiMWindow::showConsolePressed);
    connect(ui->consoleButton, &QPushButton::released, this, &QtSLiMWindow::showConsoleReleased);
    connect(ui->browserButton, &QPushButton::pressed, this, &QtSLiMWindow::showBrowserPressed);
    connect(ui->browserButton, &QPushButton::released, this, &QtSLiMWindow::showBrowserReleased);
    connect(ui->clearOutputButton, &QPushButton::pressed, this, &QtSLiMWindow::clearOutputPressed);
    connect(ui->clearOutputButton, &QPushButton::released, this, &QtSLiMWindow::clearOutputReleased);
    connect(ui->dumpPopulationButton, &QPushButton::pressed, this, &QtSLiMWindow::dumpPopulationPressed);
    connect(ui->dumpPopulationButton, &QPushButton::released, this, &QtSLiMWindow::dumpPopulationReleased);
    connect(ui->graphPopupButton, &QPushButton::pressed, this, &QtSLiMWindow::graphPopupButtonPressed);
    connect(ui->graphPopupButton, &QPushButton::released, this, &QtSLiMWindow::graphPopupButtonReleased);
    connect(ui->changeDirectoryButton, &QPushButton::pressed, this, &QtSLiMWindow::changeDirectoryPressed);
    connect(ui->changeDirectoryButton, &QPushButton::released, this, &QtSLiMWindow::changeDirectoryReleased);
    
    // fix application-level actions to know they are application-level; not sure whether/how this matters
    ui->actionNew->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionNew_nonWF->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionOpen->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionQtSLiMHelp->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionSendFeedback->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionMailingList_slimannounce->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionMailingList_slimdiscuss->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionSLiMHomePage->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionSLiMExtras->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionAboutMesserLab->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionAboutBenHaller->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionAboutStickSoftware->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionQuitQtSLiM->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionFindRecipe->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionPreferences->setShortcutContext(Qt::ApplicationShortcut);
    ui->actionOpenRecent->setShortcutContext(Qt::ApplicationShortcut);
    
    // this action seems to need to be added to the main window in order to function reliably;
    // I'm not sure why, maybe it is because it is connected to an object that is not a widget?
    // adding it as an action here seems to have no visible effect except that the shortcut now works
    addAction(ui->actionFindRecipe);
    
    // connect all menu items with existing slots
    connect(ui->actionQuitQtSLiM, &QAction::triggered, qApp, &QApplication::closeAllWindows, Qt::QueuedConnection);
    connect(ui->actionAboutQtSLiM, &QAction::triggered, this, &QtSLiMWindow::aboutQtSLiM);
    connect(ui->actionPreferences, &QAction::triggered, this, &QtSLiMWindow::showPreferences);
    connect(ui->actionNew, &QAction::triggered, this, &QtSLiMWindow::newFile_WF);
    connect(ui->actionNew_nonWF, &QAction::triggered, this, &QtSLiMWindow::newFile_nonWF);
    connect(ui->actionOpen, &QAction::triggered, this, &QtSLiMWindow::open);
    connect(ui->actionClose, &QAction::triggered, []() {
        // We close the "active" window, which is a bit different from the front window
        // It can be nullptr; in that case it's hard to know what to do
        QWidget *activeWindow = QApplication::activeWindow();
        
        if (activeWindow)
            activeWindow->close();
    });
    connect(ui->actionSave, &QAction::triggered, this, &QtSLiMWindow::save);
    connect(ui->actionSaveAs, &QAction::triggered, this, &QtSLiMWindow::saveAs);
    connect(ui->actionRevertToSaved, &QAction::triggered, this, &QtSLiMWindow::revert);
    connect(ui->actionStep, &QAction::triggered, this, &QtSLiMWindow::playOneStepClicked);
    connect(ui->actionPlay, &QAction::triggered, [this]() { playOrProfile(true); });
    connect(ui->actionProfile, &QAction::triggered, [this]() { playOrProfile(false); });
    connect(ui->actionRecycle, &QAction::triggered, this, &QtSLiMWindow::recycleClicked);
    connect(ui->actionChangeWorkingDirectory, &QAction::triggered, this, &QtSLiMWindow::changeDirectoryClicked);
    connect(ui->actionDumpPopulationState, &QAction::triggered, this, &QtSLiMWindow::dumpPopulationClicked);
    connect(ui->actionCheckScript, &QAction::triggered, ui->scriptTextEdit, &QtSLiMTextEdit::checkScript);
    connect(ui->actionPrettyprintScript, &QAction::triggered, ui->scriptTextEdit, &QtSLiMTextEdit::prettyprint);
    connect(ui->actionShowScriptHelp, &QAction::triggered, this, &QtSLiMWindow::scriptHelpClicked);
    connect(ui->actionQtSLiMHelp, &QAction::triggered, this, &QtSLiMWindow::scriptHelpClicked);
    connect(ui->actionShowEidosConsole, &QAction::triggered, [this]() { ui->consoleButton->toggle(); showConsoleClicked(); });
    connect(ui->actionShowVariableBrowser, &QAction::triggered, this, &QtSLiMWindow::showBrowserClicked);
    connect(ui->actionClearOutput, &QAction::triggered, this, &QtSLiMWindow::clearOutputClicked);
    
    // connect menu items that open a URL
    connect(ui->actionSendFeedback, &QAction::triggered, []() {
        QDesktopServices::openUrl(QUrl("mailto:bhaller@mac.com?subject=SLiM%20Feedback", QUrl::TolerantMode));
    });
    connect(ui->actionMailingList_slimdiscuss, &QAction::triggered, []() {
        QDesktopServices::openUrl(QUrl("https://groups.google.com/d/forum/slim-discuss", QUrl::TolerantMode));
    });
    connect(ui->actionMailingList_slimannounce, &QAction::triggered, []() {
        QDesktopServices::openUrl(QUrl("https://groups.google.com/d/forum/slim-announce", QUrl::TolerantMode));
    });
    connect(ui->actionSLiMHomePage, &QAction::triggered, []() {
        QDesktopServices::openUrl(QUrl("http://messerlab.org/slim/", QUrl::TolerantMode));
    });
    connect(ui->actionSLiMExtras, &QAction::triggered, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/MesserLab/SLiM-Extras", QUrl::TolerantMode));
    });
    connect(ui->actionAboutMesserLab, &QAction::triggered, []() {
        QDesktopServices::openUrl(QUrl("http://messerlab.org/", QUrl::TolerantMode));
    });
    connect(ui->actionAboutBenHaller, &QAction::triggered, []() {
        QDesktopServices::openUrl(QUrl("http://www.benhaller.com/", QUrl::TolerantMode));
    });
    connect(ui->actionAboutStickSoftware, &QAction::triggered, []() {
        QDesktopServices::openUrl(QUrl("http://www.sticksoftware.com/", QUrl::TolerantMode));
    });
    
    // connect custom menu items
    connect(ui->actionShiftLeft, &QAction::triggered, []() {
        QWidget *focusWidget = QApplication::focusWidget();
        QtSLiMScriptTextEdit *scriptEdit = dynamic_cast<QtSLiMScriptTextEdit*>(focusWidget);
        
        if (scriptEdit && scriptEdit->isEnabled() && !scriptEdit->isReadOnly())
            scriptEdit->shiftSelectionLeft();
    });
    connect(ui->actionShiftRight, &QAction::triggered, []() {
        QWidget *focusWidget = QApplication::focusWidget();
        QtSLiMScriptTextEdit *scriptEdit = dynamic_cast<QtSLiMScriptTextEdit*>(focusWidget);
        
        if (scriptEdit && scriptEdit->isEnabled() && !scriptEdit->isReadOnly())
            scriptEdit->shiftSelectionRight();
    });
    connect(ui->actionCommentUncomment, &QAction::triggered, []() {
        QWidget *focusWidget = QApplication::focusWidget();
        QtSLiMScriptTextEdit *scriptEdit = dynamic_cast<QtSLiMScriptTextEdit*>(focusWidget);
        
        if (scriptEdit && scriptEdit->isEnabled() && !scriptEdit->isReadOnly())
            scriptEdit->commentUncommentSelection();
    });
    connect(ui->actionExecuteSelection, &QAction::triggered, []() {
        QWidget *focusWidget = QApplication::focusWidget();
        QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWidget->window());
        
        if (eidosConsole)
            eidosConsole->executeSelectionClicked();
    });
    connect(ui->actionExecuteAll, &QAction::triggered, []() {
        QWidget *focusWidget = QApplication::focusWidget();
        QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWidget->window());
        
        if (eidosConsole)
            eidosConsole->executeAllClicked();
    });
    
    // standard actions that need to be dispatched (I haven't found a better way to do this;
    // this is basically implementing the first responder / event dispatch mechanism)
    // FIXME should enable/disable the menu items using copyAvailable(), undoAvailable(), etc.
    connect(ui->actionUndo, &QAction::triggered, []() {
        QWidget *focusWidget = QApplication::focusWidget();
        QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
        QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
        
        if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
            lineEdit->undo();
        else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
            textEdit->undo();
    });
    connect(ui->actionRedo, &QAction::triggered, []() {
        QWidget *focusWidget = QApplication::focusWidget();
        QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
        QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
        
        if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
            lineEdit->redo();
        else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
            textEdit->redo();
    });
    connect(ui->actionCut, &QAction::triggered, []() {
        QWidget *focusWidget = QApplication::focusWidget();
        QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
        QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
        
        if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
            lineEdit->cut();
        else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
            textEdit->cut();
    });
    connect(ui->actionCopy, &QAction::triggered, []() {
        QWidget *focusWidget = QApplication::focusWidget();
        QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
        QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
        
        if (lineEdit && lineEdit->isEnabled())
            lineEdit->copy();
        else if (textEdit && textEdit->isEnabled())
            textEdit->copy();
    });
    connect(ui->actionPaste, &QAction::triggered, []() {
        QWidget *focusWidget = QApplication::focusWidget();
        QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
        QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
        
        if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
            lineEdit->paste();
        else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
            textEdit->paste();
    });
    connect(ui->actionDelete, &QAction::triggered, []() {
        QWidget *focusWidget = QApplication::focusWidget();
        QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
        QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
        
        if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
            lineEdit->insert("");
        else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
            textEdit->insertPlainText("");
    });
    connect(ui->actionSelectAll, &QAction::triggered, []() {
        QWidget *focusWidget = QApplication::focusWidget();
        QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
        QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
        
        if (lineEdit && lineEdit->isEnabled())
            lineEdit->selectAll();
        else if (textEdit && textEdit->isEnabled())
            textEdit->selectAll();
    });
    /*
    currently unimplemented:
    
    QAction *actionFind_2;
    QAction *actionFind_and_Replace;
    QAction *actionFind_Next;
    QAction *actionFind_Previous;
    QAction *actionUse_Selection_for_Find;
    QAction *actionJump_to_Selection;
    */
}


//
//  private slots
//

void QtSLiMWindow::playOneStepPressed(void)
{
    ui->playOneStepButton->setIcon(QIcon(":/buttons/play_step_H.png"));
}
void QtSLiMWindow::playOneStepReleased(void)
{
    ui->playOneStepButton->setIcon(QIcon(":/buttons/play_step.png"));
}
void QtSLiMWindow::playPressed(void)
{
    updatePlayButtonIcon(true);
}
void QtSLiMWindow::playReleased(void)
{
    updatePlayButtonIcon(false);
}
void QtSLiMWindow::profilePressed(void)
{
    updateProfileButtonIcon(true);
}
void QtSLiMWindow::profileReleased(void)
{
    updateProfileButtonIcon(false);
}
void QtSLiMWindow::recyclePressed(void)
{
    updateRecycleButtonIcon(true);
}
void QtSLiMWindow::recycleReleased(void)
{
    updateRecycleButtonIcon(false);
}
void QtSLiMWindow::showMutationsPressed(void)
{
    ui->showMutationsButton->setIcon(QIcon(ui->showMutationsButton->isChecked() ? ":/buttons/show_mutations.png" : ":/buttons/show_mutations_H.png"));
}
void QtSLiMWindow::showMutationsReleased(void)
{
    ui->showMutationsButton->setIcon(QIcon(ui->showMutationsButton->isChecked() ? ":/buttons/show_mutations_H.png" : ":/buttons/show_mutations.png"));
}
void QtSLiMWindow::showFixedSubstitutionsPressed(void)
{
    ui->showFixedSubstitutionsButton->setIcon(QIcon(ui->showFixedSubstitutionsButton->isChecked() ? ":/buttons/show_fixed.png" : ":/buttons/show_fixed_H.png"));
}
void QtSLiMWindow::showFixedSubstitutionsReleased(void)
{
    ui->showFixedSubstitutionsButton->setIcon(QIcon(ui->showFixedSubstitutionsButton->isChecked() ? ":/buttons/show_fixed_H.png" : ":/buttons/show_fixed.png"));
}
void QtSLiMWindow::showChromosomeMapsPressed(void)
{
    ui->showChromosomeMapsButton->setIcon(QIcon(ui->showChromosomeMapsButton->isChecked() ? ":/buttons/show_recombination.png" : ":/buttons/show_recombination_H.png"));
}
void QtSLiMWindow::showChromosomeMapsReleased(void)
{
    ui->showChromosomeMapsButton->setIcon(QIcon(ui->showChromosomeMapsButton->isChecked() ? ":/buttons/show_recombination_H.png" : ":/buttons/show_recombination.png"));
}
void QtSLiMWindow::showGenomicElementsPressed(void)
{
    ui->showGenomicElementsButton->setIcon(QIcon(ui->showGenomicElementsButton->isChecked() ? ":/buttons/show_genomicelements.png" : ":/buttons/show_genomicelements_H.png"));
}
void QtSLiMWindow::showGenomicElementsReleased(void)
{
    ui->showGenomicElementsButton->setIcon(QIcon(ui->showGenomicElementsButton->isChecked() ? ":/buttons/show_genomicelements_H.png" : ":/buttons/show_genomicelements.png"));
}
void QtSLiMWindow::checkScriptPressed(void)
{
    ui->checkScriptButton->setIcon(QIcon(":/buttons/check_H.png"));
}
void QtSLiMWindow::checkScriptReleased(void)
{
    ui->checkScriptButton->setIcon(QIcon(":/buttons/check.png"));
}
void QtSLiMWindow::prettyprintPressed(void)
{
    ui->prettyprintButton->setIcon(QIcon(":/buttons/prettyprint_H.png"));
}
void QtSLiMWindow::prettyprintReleased(void)
{
    ui->prettyprintButton->setIcon(QIcon(":/buttons/prettyprint.png"));
}
void QtSLiMWindow::scriptHelpPressed(void)
{
    ui->scriptHelpButton->setIcon(QIcon(":/buttons/syntax_help_H.png"));
}
void QtSLiMWindow::scriptHelpReleased(void)
{
    ui->scriptHelpButton->setIcon(QIcon(":/buttons/syntax_help.png"));
}
void QtSLiMWindow::showConsolePressed(void)
{
    ui->consoleButton->setIcon(QIcon(ui->consoleButton->isChecked() ? ":/buttons/show_console.png" : ":/buttons/show_console_H.png"));
}
void QtSLiMWindow::showConsoleReleased(void)
{
    ui->consoleButton->setIcon(QIcon(ui->consoleButton->isChecked() ? ":/buttons/show_console_H.png" : ":/buttons/show_console.png"));
}
void QtSLiMWindow::showBrowserPressed(void)
{
    ui->browserButton->setIcon(QIcon(ui->browserButton->isChecked() ? ":/buttons/show_browser.png" : ":/buttons/show_browser_H.png"));
}
void QtSLiMWindow::showBrowserReleased(void)
{
    ui->browserButton->setIcon(QIcon(ui->browserButton->isChecked() ? ":/buttons/show_browser_H.png" : ":/buttons/show_browser.png"));
}
void QtSLiMWindow::clearOutputPressed(void)
{
    ui->clearOutputButton->setIcon(QIcon(":/buttons/delete_H.png"));
}
void QtSLiMWindow::clearOutputReleased(void)
{
    ui->clearOutputButton->setIcon(QIcon(":/buttons/delete.png"));
}
void QtSLiMWindow::dumpPopulationPressed(void)
{
    ui->dumpPopulationButton->setIcon(QIcon(":/buttons/dump_output_H.png"));
}
void QtSLiMWindow::dumpPopulationReleased(void)
{
    ui->dumpPopulationButton->setIcon(QIcon(":/buttons/dump_output.png"));
}
void QtSLiMWindow::graphPopupButtonPressed(void)
{
    ui->graphPopupButton->setIcon(QIcon(":/buttons/graph_submenu_H.png"));
}
void QtSLiMWindow::graphPopupButtonReleased(void)
{
    ui->graphPopupButton->setIcon(QIcon(":/buttons/graph_submenu.png"));
}
void QtSLiMWindow::changeDirectoryPressed(void)
{
    ui->changeDirectoryButton->setIcon(QIcon(":/buttons/change_folder_H.png"));
}
void QtSLiMWindow::changeDirectoryReleased(void)
{
    ui->changeDirectoryButton->setIcon(QIcon(":/buttons/change_folder.png"));
}