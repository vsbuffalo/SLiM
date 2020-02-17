//
//  QtSLiMWindow.cpp
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
#include "QtSLiMAppDelegate.h"
#include "QtSLiMEidosPrettyprinter.h"
#include "QtSLiMAbout.h"
#include "QtSLiMPreferences.h"
#include "QtSLiMHelpWindow.h"
#include "QtSLiMEidosConsole.h"
#include "QtSLiMSyntaxHighlighting.h"
#include "QtSLiMScriptTextEdit.h"
#include "QtSLiM_SLiMgui.h"

#include <QCoreApplication>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QtDebug>
#include <QMessageBox>
#include <QTextEdit>
#include <QCursor>
#include <QPalette>
#include <QFileDialog>
#include <QSettings>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QStandardPaths>
#include <QToolTip>

#include <unistd.h>

#include "individual.h"


// TO DO:
//
// splitviews for the window: https://stackoverflow.com/questions/28309376/how-to-manage-qsplitter-in-qt-designer
// set up the app icon correctly: this seems to be very complicated, and didn't work on macOS, sigh...
// make the population tableview rows selectable
// implement selection of a subrange in the chromosome view
// enable the more efficient code paths in the chromosome view
// enable the other display types in the individuals view
// implement pop-up menu for graph pop-up button, graph windows
// decide whether to implement the drawer or not
// decide whether to implement the variable browser or not
// associate .slim with QtSLiM; how is this done in Linux, or in Qt?


QtSLiMWindow::QtSLiMWindow(QtSLiMWindow::ModelType modelType) : QMainWindow(nullptr), ui(new Ui::QtSLiMWindow)
{
    init();
    setCurrentFile(QString());
    
    // set up the initial script
    std::string untitledScriptString = (modelType == QtSLiMWindow::ModelType::WF) ? QtSLiMWindow::defaultWFScriptString() : QtSLiMWindow::defaultNonWFScriptString();
    ui->scriptTextEdit->setPlainText(QString::fromStdString(untitledScriptString));
    setScriptStringAndInitializeSimulation(untitledScriptString);
    
    // Update all our UI to reflect the current state of the simulation
    updateAfterTickFull(true);
    resetSLiMChangeCount();     // no recycle change count; the current model is correct
    setWindowModified(false);    // untitled windows consider themselves unmodified
}

QtSLiMWindow::QtSLiMWindow(const QString &fileName) : QMainWindow(nullptr), ui(new Ui::QtSLiMWindow)
{
    init();
    loadFile(fileName);
}

QtSLiMWindow::QtSLiMWindow(const QString &recipeName, const QString &recipeScript) : QMainWindow(nullptr), ui(new Ui::QtSLiMWindow)
{
    init();
    setCurrentFile(QString());
    setWindowFilePath(recipeName);
    isRecipe = true;
    
    // set up the initial script
    ui->scriptTextEdit->setPlainText(recipeScript);
    setScriptStringAndInitializeSimulation(recipeScript.toUtf8().constData());
    
    // Update all our UI to reflect the current state of the simulation
    updateAfterTickFull(true);
    resetSLiMChangeCount();     // no recycle change count; the current model is correct
    setWindowModified(false);    // untitled windows consider themselves unmodified
}

void QtSLiMWindow::init(void)
{
    setAttribute(Qt::WA_DeleteOnClose);
    isUntitled = true;
    isRecipe = false;
    
    // create the window UI
    ui->setupUi(this);
    initializeUI();
    
    // wire up our continuous play and generation play timers
    connect(&continuousPlayInvocationTimer_, &QTimer::timeout, this, &QtSLiMWindow::_continuousPlay);
    connect(&generationPlayInvocationTimer_, &QTimer::timeout, this, &QtSLiMWindow::_generationPlay);
    connect(&continuousProfileInvocationTimer_, &QTimer::timeout, this, &QtSLiMWindow::_continuousProfile);
    
    // wire up deferred display of script errors and termination messages
    connect(this, &QtSLiMWindow::terminationWithMessage, this, &QtSLiMWindow::showTerminationMessage, Qt::QueuedConnection);
    
    // forward option-clicks in our views to the help window
    ui->scriptTextEdit->setOptionClickEnabled(true);
    ui->outputTextEdit->setOptionClickEnabled(false);
    
    // the script textview completes, the output textview does not
    ui->scriptTextEdit->setCodeCompletionEnabled(true);
    ui->outputTextEdit->setCodeCompletionEnabled(false);
    
    // We set the working directory for new windows to ~/Desktop/, since it makes no sense for them to use the location of the app.
    // Each running simulation will track its own working directory, and the user can set it with a button in the SLiMgui window.
    sim_working_dir = Eidos_ResolvedPath("~/Desktop");
    sim_requested_working_dir = sim_working_dir;	// return to Desktop on recycle unless the user overrides it
    
    // Wire up things that set the window to be modified.
    connect(ui->scriptTextEdit, &QTextEdit::textChanged, this, &QtSLiMWindow::documentWasModified);
    connect(ui->scriptTextEdit, &QTextEdit::textChanged, this, &QtSLiMWindow::scriptTexteditChanged);
    
    // Ensure that the generation lineedit does not have the initial keyboard focus and has no selection; hard to do!
    ui->generationLineEdit->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    QTimer::singleShot(0, [this]() { ui->generationLineEdit->setFocusPolicy(Qt::FocusPolicy::StrongFocus); });
    
    // Instantiate the help panel up front so that it responds instantly; slows down our launch, but it seems better to me...
    QtSLiMHelpWindow::instance();
    
    // Create our console window; we want one all the time, so that it keeps live symbols for code completion for us
    if (!consoleController)
    {
        consoleController = new QtSLiMEidosConsole(this);
        if (consoleController)
        {
            // wire ourselves up to monitor the console for closing, to fix our button state
            connect(consoleController, &QtSLiMEidosConsole::willClose, [this]() {
                ui->consoleButton->setChecked(false);
                showConsoleReleased();
            });
        }
        else
        {
            qDebug() << "Could not create console controller";
        }
    }
}

void QtSLiMWindow::initializeUI(void)
{
    glueUI();
    
    // fix the layout of the window
    ui->scriptHeaderLayout->setSpacing(4);
    ui->scriptHeaderLayout->setMargin(0);
    ui->scriptHeaderLabel->setContentsMargins(8, 0, 15, 0);

    ui->outputHeaderLayout->setSpacing(4);
    ui->outputHeaderLayout->setMargin(0);
    ui->outputHeaderLabel->setContentsMargins(8, 0, 15, 0);

    ui->playControlsLayout->setSpacing(8);
    ui->playControlsLayout->setMargin(0);
    
    // substitute a custom layout subclass for playControlsLayout to lay out the profile button specially
    {
        QtSLiMPlayControlsLayout *newPlayControlsLayout = new QtSLiMPlayControlsLayout();
        int indexOfPlayControlsLayout = -1;
        
        // QLayout::indexOf(QLayoutItem *layoutItem) wasn't added until 5.12, oddly
        for (int i = 0; i < ui->topRightLayout->count(); ++i)
            if (ui->topRightLayout->itemAt(i) == ui->playControlsLayout)
                indexOfPlayControlsLayout = i;
        
        if (indexOfPlayControlsLayout >= 0)
        {
            ui->topRightLayout->insertItem(indexOfPlayControlsLayout, newPlayControlsLayout);
            newPlayControlsLayout->setParent(ui->topRightLayout);   // surprising that insertItem() doesn't do this...; but this sets our parentWidget also, correctly
            
            // Transfer over the contents of the old layout
            while (ui->playControlsLayout->count())
            {
                QLayoutItem *layoutItem = ui->playControlsLayout->takeAt(0);
                newPlayControlsLayout->addItem(layoutItem);
            }
            
            // Transfer properties of the old layout
            newPlayControlsLayout->setSpacing(ui->playControlsLayout->spacing());
            newPlayControlsLayout->setMargin(ui->playControlsLayout->margin());
            
            // Get rid of the old layout
            ui->topRightLayout->removeItem(ui->playControlsLayout);
            ui->playControlsLayout = nullptr;
            
            // Remember the new layout
            ui->playControlsLayout = newPlayControlsLayout;
        }
        else
        {
            qDebug() << "Couldn't find playControlsLayout!";
        }
    }
    
    // set the script types and syntax highlighting appropriately
    ui->scriptTextEdit->setScriptType(QtSLiMTextEdit::SLiMScriptType);
    ui->scriptTextEdit->setSyntaxHighlightType(QtSLiMTextEdit::ScriptHighlighting);
    
    ui->outputTextEdit->setScriptType(QtSLiMTextEdit::NoScriptType);
    ui->outputTextEdit->setSyntaxHighlightType(QtSLiMTextEdit::OutputHighlighting);
    
    // set button states
    ui->showChromosomeMapsButton->setChecked(zoomedChromosomeShowsRateMaps);
    ui->showGenomicElementsButton->setChecked(zoomedChromosomeShowsGenomicElements);
    ui->showMutationsButton->setChecked(zoomedChromosomeShowsMutations);
    ui->showFixedSubstitutionsButton->setChecked(zoomedChromosomeShowsFixedSubstitutions);

    // Set up the population table view
    populationTableModel_ = new QtSLiMPopulationTableModel(this);
    ui->subpopTableView->setModel(populationTableModel_);
    ui->subpopTableView->setHorizontalHeader(new QtSLiMPopulationTableHeaderView(Qt::Orientation::Horizontal, this));
    
    QHeaderView *popTableHHeader = ui->subpopTableView->horizontalHeader();
    QHeaderView *popTableVHeader = ui->subpopTableView->verticalHeader();
    
    popTableHHeader->setMinimumSectionSize(1);
    popTableVHeader->setMinimumSectionSize(1);
    
    popTableHHeader->resizeSection(0, 35);
    //popTableHHeader->resizeSection(1, 60);
    popTableHHeader->resizeSection(2, 40);
    popTableHHeader->resizeSection(3, 40);
    popTableHHeader->resizeSection(4, 40);
    popTableHHeader->resizeSection(5, 40);
    popTableHHeader->setSectionsClickable(false);
    popTableHHeader->setSectionsMovable(false);
    popTableHHeader->setSectionResizeMode(0, QHeaderView::Fixed);
    popTableHHeader->setSectionResizeMode(1, QHeaderView::Stretch);
    popTableHHeader->setSectionResizeMode(2, QHeaderView::Fixed);
    popTableHHeader->setSectionResizeMode(3, QHeaderView::Fixed);
    popTableHHeader->setSectionResizeMode(4, QHeaderView::Fixed);
    popTableHHeader->setSectionResizeMode(5, QHeaderView::Fixed);
    
    QFont headerFont = popTableHHeader->font();
    QFont cellFont = ui->subpopTableView->font();
#ifdef __APPLE__
    headerFont.setPointSize(11);
    cellFont.setPointSize(11);
#else
    headerFont.setPointSize(8);
    cellFont.setPointSize(8);
#endif
    popTableHHeader->setFont(headerFont);
    ui->subpopTableView->setFont(cellFont);
    
    popTableVHeader->setSectionResizeMode(QHeaderView::Fixed);
    popTableVHeader->setDefaultSectionSize(18);
    
    // Set up our chromosome views to show the proper stuff
	ui->chromosomeOverview->setReferenceChromosomeView(nullptr);
	ui->chromosomeOverview->setSelectable(true);
	ui->chromosomeOverview->setShouldDrawGenomicElements(true);
	ui->chromosomeOverview->setShouldDrawMutations(false);
	ui->chromosomeOverview->setShouldDrawFixedSubstitutions(false);
	ui->chromosomeOverview->setShouldDrawRateMaps(false);
	
	ui->chromosomeZoomed->setReferenceChromosomeView(ui->chromosomeOverview);
	ui->chromosomeZoomed->setSelectable(false);
	ui->chromosomeZoomed->setShouldDrawGenomicElements(ui->showGenomicElementsButton->isChecked());
	ui->chromosomeZoomed->setShouldDrawMutations(ui->showMutationsButton->isChecked());
	ui->chromosomeZoomed->setShouldDrawFixedSubstitutions(ui->showFixedSubstitutionsButton->isChecked());
	ui->chromosomeZoomed->setShouldDrawRateMaps(ui->showChromosomeMapsButton->isChecked());
    
    // Restore the saved window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMMainWindow");
    resize(settings.value("size", QSize(950, 700)).toSize());
    move(settings.value("pos", QPoint(100, 100)).toPoint());
    settings.endGroup();
    
    // Ask the app delegate to handle the recipes menu for us
    qtSLiMAppDelegate->setUpRecipesMenu(ui->menuOpenRecipe, ui->actionFindRecipe);
    
    // Set up the recent documents submenu
    QMenu *recentMenu = new QMenu("Open Recent");
    ui->actionOpenRecent->setMenu(recentMenu);
    connect(recentMenu, &QMenu::aboutToShow, this, &QtSLiMWindow::updateRecentFileActions);
    
    for (int i = 0; i < MaxRecentFiles; ++i) {
        recentFileActs[i] = recentMenu->addAction(QString(), this, &QtSLiMWindow::openRecentFile);
        recentFileActs[i]->setVisible(false);
    }
    
    recentMenu->addSeparator();
    recentMenu->addAction("Clear Menu", this, &QtSLiMWindow::clearRecentFiles);
    
    setRecentFilesVisible(QtSLiMWindow::hasRecentFiles());
}

QtSLiMWindow::~QtSLiMWindow()
{
    delete ui;

    // Disconnect delegate relationships
    if (consoleController)
        consoleController->parentSLiMWindow = nullptr;
    
    // Free resources
    if (sim)
    {
        delete sim;
        sim = nullptr;
    }
    if (slimgui)
	{
		delete slimgui;
		slimgui = nullptr;
	}

    Eidos_FreeRNG(sim_RNG);

    setInvalidSimulation(true);
    
    // The console is owned by us, and it owns the variable browser.  Since the parent
    // relationships are set up, they should be released by Qt automatically.
    if (consoleController)
    {
        //if (consoleController->browserController)
        //  consoleController->browserController->hide();
        consoleController->hide();
    }
}

std::string QtSLiMWindow::defaultWFScriptString(void)
{
    return std::string(
                "// set up a simple neutral simulation\n"
                "initialize() {\n"
                "	initializeMutationRate(1e-7);\n"
                "	\n"
                "	// m1 mutation type: neutral\n"
                "	initializeMutationType(\"m1\", 0.5, \"f\", 0.0);\n"
                "	\n"
                "	// g1 genomic element type: uses m1 for all mutations\n"
                "	initializeGenomicElementType(\"g1\", m1, 1.0);\n"
                "	\n"
                "	// uniform chromosome of length 100 kb with uniform recombination\n"
                "	initializeGenomicElement(g1, 0, 99999);\n"
                "	initializeRecombinationRate(1e-8);\n"
                "}\n"
                "\n"
                "// create a population of 500 individuals\n"
                "1 {\n"
                "	sim.addSubpop(\"p1\", 500);\n"
                "}\n"
                "\n"
                "// output samples of 10 genomes periodically, all fixed mutations at end\n"
                "1000 late() { p1.outputSample(10); }\n"
                "2000 late() { p1.outputSample(10); }\n"
                "2000 late() { sim.outputFixedMutations(); }\n");
}

std::string QtSLiMWindow::defaultNonWFScriptString(void)
{
    return std::string(
                "// set up a simple neutral nonWF simulation\n"
                "initialize() {\n"
                "	initializeSLiMModelType(\"nonWF\");\n"
                "	defineConstant(\"K\", 500);	// carrying capacity\n"
                "	\n"
                "	// neutral mutations, which are allowed to fix\n"
                "	initializeMutationType(\"m1\", 0.5, \"f\", 0.0);\n"
                "	m1.convertToSubstitution = T;\n"
                "	\n"
                "	initializeGenomicElementType(\"g1\", m1, 1.0);\n"
                "	initializeGenomicElement(g1, 0, 99999);\n"
                "	initializeMutationRate(1e-7);\n"
                "	initializeRecombinationRate(1e-8);\n"
                "}\n"
                "\n"
                "// each individual reproduces itself once\n"
                "reproduction() {\n"
                "	subpop.addCrossed(individual, subpop.sampleIndividuals(1));\n"
                "}\n"
                "\n"
                "// create an initial population of 10 individuals\n"
                "1 early() {\n"
                "	sim.addSubpop(\"p1\", 10);\n"
                "}\n"
                "\n"
                "// provide density-dependent selection\n"
                "early() {\n"
                "	p1.fitnessScaling = K / p1.individualCount;\n"
                "}\n"
                "\n"
                "// output all fixed mutations at end\n"
                "2000 late() { sim.outputFixedMutations(); }\n");
}

const QColor &QtSLiMWindow::blackContrastingColorForIndex(int index)
{
    static std::vector<QColor> colorArray;
	
	if (colorArray.size() == 0)
	{
        colorArray.emplace_back(QtSLiMColorWithHSV(0.65, 0.65, 1.00, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.55, 1.00, 1.00, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.40, 1.00, 0.90, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.16, 1.00, 1.00, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.08, 0.65, 1.00, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.00, 0.65, 1.00, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.80, 0.65, 1.00, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.00, 0.00, 0.80, 1.0));
	}
	
	return ((index >= 0) && (index <= 6)) ? colorArray[static_cast<size_t>(index)] : colorArray[7];
}

void QtSLiMWindow::colorForGenomicElementType(GenomicElementType *elementType, slim_objectid_t elementTypeID, float *p_red, float *p_green, float *p_blue, float *p_alpha)
{
	if (elementType && !elementType->color_.empty())
	{
        *p_red = elementType->color_red_;
        *p_green = elementType->color_green_;
        *p_blue = elementType->color_blue_;
        *p_alpha = 1.0f;
	}
	else
	{
        auto elementColorIter = genomicElementColorRegistry.find(elementTypeID);
		const QColor *elementColor = nullptr;
        
		if (elementColorIter == genomicElementColorRegistry.end())
		{
			elementColor = &QtSLiMWindow::blackContrastingColorForIndex(static_cast<int>(genomicElementColorRegistry.size()));
            
            genomicElementColorRegistry.insert(std::pair<slim_objectid_t, QColor>(elementTypeID, *elementColor));
		}
        else
        {
            elementColor = &elementColorIter->second;
        }
		
        *p_red = static_cast<float>(elementColor->redF());
        *p_green = static_cast<float>(elementColor->greenF());
        *p_blue = static_cast<float>(elementColor->blueF());
        *p_alpha = static_cast<float>(elementColor->alphaF());
	}
}

//
//  Document support
//

void QtSLiMWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave())
    {
        // Save the window position; see https://doc.qt.io/qt-5/qsettings.html#details
        QSettings settings;
        
        settings.beginGroup("QtSLiMMainWindow");
        settings.setValue("size", size());
        settings.setValue("pos", pos());
        settings.endGroup();
        
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void QtSLiMWindow::aboutQtSLiM()
{
    static QtSLiMAbout *aboutWindow = nullptr;
    
    if (!aboutWindow)
        aboutWindow = new QtSLiMAbout(nullptr);     // shared instance with no parent, never freed
    
    aboutWindow->show();
    aboutWindow->raise();
    aboutWindow->activateWindow();
}

void QtSLiMWindow::showPreferences()
{
    QtSLiMPreferences &prefsWindow = QtSLiMPreferences::instance();
    
    prefsWindow.show();
    prefsWindow.raise();
    prefsWindow.activateWindow();
}

void QtSLiMWindow::newFile_WF()
{
    QtSLiMWindow *other = new QtSLiMWindow(QtSLiMWindow::ModelType::WF);
    other->tile(this);
    other->show();
}

void QtSLiMWindow::newFile_nonWF()
{
    QtSLiMWindow *other = new QtSLiMWindow(QtSLiMWindow::ModelType::nonWF);
    other->tile(this);
    other->show();
}

QtSLiMWindow *QtSLiMWindow::runInitialOpenPanel(void)
{
    // This is like open(), but as a static method that makes no reference to an existing window
    QSettings settings;
    QString directory = settings.value("QtSLiMDefaultOpenDirectory", QVariant(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation))).toString();
    
    const QString fileName = QFileDialog::getOpenFileName(nullptr, QString(), directory, "SLiM models (*.slim);;Text files (*.txt)");  // add PDF files eventually
    if (!fileName.isEmpty())
    {
        settings.setValue("QtSLiMDefaultOpenDirectory", QVariant(QFileInfo(fileName).path()));
        
        QtSLiMWindow *other = new QtSLiMWindow(fileName);
        if (other->isUntitled) {
            delete other;
            return nullptr;
        }
        return other;
    }
    
    return nullptr;
}

void QtSLiMWindow::open()
{
    QSettings settings;
    QString directory = settings.value("QtSLiMDefaultOpenDirectory", QVariant(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation))).toString();
    
    const QString fileName = QFileDialog::getOpenFileName(this, QString(), directory, "SLiM models (*.slim);;Text files (*.txt)");  // add PDF files eventually
    if (!fileName.isEmpty())
    {
        settings.setValue("QtSLiMDefaultOpenDirectory", QVariant(QFileInfo(fileName).path()));
        openFile(fileName);
    }
}

void QtSLiMWindow::openFile(const QString &fileName)
{
    QtSLiMWindow *existing = findMainWindow(fileName);
    if (existing) {
        existing->show();
        existing->raise();
        existing->activateWindow();
        return;
    }

    if (isUntitled && !isRecipe && (slimChangeCount == 0) && !isWindowModified()) {
        loadFile(fileName);
        return;
    }

    QtSLiMWindow *other = new QtSLiMWindow(fileName);
    if (other->isUntitled) {
        delete other;
        return;
    }
    other->tile(this);
    other->show();
}

void QtSLiMWindow::openRecipe(const QString &recipeName, const QString &recipeScript)
{
    if (isUntitled && !isRecipe && (slimChangeCount == 0) && !isWindowModified())
    {
        if (consoleController)
            consoleController->invalidateSymbolTableAndFunctionMap();
        
        clearOutputClicked();
        ui->scriptTextEdit->setPlainText(recipeScript);
        setScriptStringAndInitializeSimulation(recipeScript.toUtf8().constData());
        
        if (consoleController)
            consoleController->validateSymbolTableAndFunctionMap();
        
        setWindowFilePath(recipeName);
        isRecipe = true;
        
        // Update all our UI to reflect the current state of the simulation
        updateAfterTickFull(true);
        resetSLiMChangeCount();     // no recycle change count; the current model is correct
        setWindowModified(false);   // loaded windows start unmodified
        return;
    }

    QtSLiMWindow *other = new QtSLiMWindow(recipeName, recipeScript);
    if (!other->isRecipe) {
        delete other;
        return;
    }
    other->tile(this);
    other->show();
}

bool QtSLiMWindow::save()
{
    return isUntitled ? saveAs() : saveFile(curFile);
}

bool QtSLiMWindow::saveAs()
{
    QString fileName;
    
    if (isUntitled)
    {
        QSettings settings;
        QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        QString directory = settings.value("QtSLiMDefaultSaveDirectory", QVariant(desktopPath)).toString();
        QFileInfo fileInfo(QDir(directory), "Untitled.slim");
        QString path = fileInfo.absoluteFilePath();
        
        fileName = QFileDialog::getSaveFileName(this, "Save As", path);
        
        if (!fileName.isEmpty())
            settings.setValue("QtSLiMDefaultSaveDirectory", QVariant(QFileInfo(fileName).path()));
    }
    else
    {
        // propose saving to the existing filename in the existing directory
        fileName = QFileDialog::getSaveFileName(this, "Save As", curFile);
    }
    
    if (fileName.isEmpty())
        return false;

    return saveFile(fileName);
}

void QtSLiMWindow::revert()
{
    if (isUntitled)
    {
        qApp->beep();
    }
    else
    {
        const QMessageBox::StandardButton ret = QMessageBox::warning(this, "QtSLiM", "Are you sure you want to revert?  All changes will be lost.", QMessageBox::Yes | QMessageBox::Cancel);
        
        switch (ret) {
        case QMessageBox::Yes:
            loadFile(curFile);
            break;
        case QMessageBox::Cancel:
            break;
        default:
            break;
        }
    }
}

bool QtSLiMWindow::maybeSave()
{
    // the recycle button change state is irrelevant; the document change state is what matters
    if (!isWindowModified())
        return true;
    
    const QMessageBox::StandardButton ret = QMessageBox::warning(this, "QtSLiM", "The document has been modified.\nDo you want to save your changes?", QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    
    switch (ret) {
    case QMessageBox::Save:
        return save();
    case QMessageBox::Cancel:
        return false;
    default:
        break;
    }
    return true;
}

void QtSLiMWindow::loadFile(const QString &fileName)
{
    QFile file(fileName);
    
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, "QtSLiM", QString("Cannot read file %1:\n%2.").arg(QDir::toNativeSeparators(fileName), file.errorString()));
        return;
    }
    
    QTextStream in(&file);
    QString contents = in.readAll();
    ui->scriptTextEdit->setPlainText(contents);
    
    if (consoleController)
        consoleController->invalidateSymbolTableAndFunctionMap();
    
    clearOutputClicked();
    setScriptStringAndInitializeSimulation(contents.toUtf8().constData());
    
    if (consoleController)
        consoleController->validateSymbolTableAndFunctionMap();
    
    setCurrentFile(fileName);
    
    // Update all our UI to reflect the current state of the simulation
    updateAfterTickFull(true);
    resetSLiMChangeCount();     // no recycle change count; the current model is correct
    setWindowModified(false);   // loaded windows start unmodified
}

bool QtSLiMWindow::saveFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, "QtSLiM", QString("Cannot write file %1:\n%2.").arg(QDir::toNativeSeparators(fileName), file.errorString()));
        return false;
    }

    QTextStream out(&file);
    out << ui->scriptTextEdit->toPlainText();

    setCurrentFile(fileName);
    return true;
}

void QtSLiMWindow::setCurrentFile(const QString &fileName)
{
    static int sequenceNumber = 1;

    isUntitled = fileName.isEmpty();
    
    if (isUntitled) {
        if (sequenceNumber == 1)
            curFile = QString("Untitled");
        else
            curFile = QString("Untitled %1").arg(sequenceNumber);
        sequenceNumber++;
    } else {
        curFile = QFileInfo(fileName).canonicalFilePath();
    }

    ui->scriptTextEdit->document()->setModified(false);
    setWindowModified(false);
    
    if (!isUntitled)
        QtSLiMWindow::prependToRecentFiles(curFile);
    
    setWindowFilePath(curFile);
}

QtSLiMWindow *QtSLiMWindow::findMainWindow(const QString &fileName) const
{
    QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();

    const QList<QWidget *> topLevelWidgets = QApplication::topLevelWidgets();
    for (QWidget *widget : topLevelWidgets) {
        QtSLiMWindow *mainWin = qobject_cast<QtSLiMWindow *>(widget);
        if (mainWin && mainWin->curFile == canonicalFilePath)
            return mainWin;
    }

    return nullptr;
}

void QtSLiMWindow::documentWasModified()
{
    // This method should be called whenever anything happens that makes us want to mark a window as "dirty" – confirm before closing.
    // This is not quite the same as scriptTexteditChanged(), which is called whenever anything happens that makes the recycle
    // button go green; recycling resets the recycle button to gray, whereas saving resets the document state to unmodified.
    // We could be called for things that are saveable but do not trigger a need for recycling.
    setWindowModified(true);
}

void QtSLiMWindow::tile(const QMainWindow *previous)
{
    if (!previous)
        return;
    int topFrameWidth = previous->geometry().top() - previous->pos().y();
    if (!topFrameWidth)
        topFrameWidth = 40;
    const QPoint pos = previous->pos() + 2 * QPoint(topFrameWidth, topFrameWidth);
    if (QApplication::desktop()->availableGeometry(this).contains(rect().bottomRight() + pos))
        move(pos);
}


//
//  Recent documents
//

void QtSLiMWindow::setRecentFilesVisible(bool visible)
{
    ui->actionOpenRecent->setVisible(visible);
}

static inline QString recentFilesKey() { return QStringLiteral("QtSLiMRecentFilesList"); }
static inline QString fileKey() { return QStringLiteral("file"); }

static QStringList readRecentFiles(QSettings &settings)
{
    QStringList result;
    const int count = settings.beginReadArray(recentFilesKey());
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        result.append(settings.value(fileKey()).toString());
    }
    settings.endArray();
    return result;
}

static void writeRecentFiles(const QStringList &files, QSettings &settings)
{
    const int count = files.size();
    settings.beginWriteArray(recentFilesKey());
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        settings.setValue(fileKey(), files.at(i));
    }
    settings.endArray();
}

bool QtSLiMWindow::hasRecentFiles()
{
    QSettings settings;
    const int count = settings.beginReadArray(recentFilesKey());
    settings.endArray();
    return count > 0;
}

void QtSLiMWindow::prependToRecentFiles(const QString &fileName)
{
    QSettings settings;

    const QStringList oldRecentFiles = readRecentFiles(settings);
    QStringList recentFiles = oldRecentFiles;
    recentFiles.removeAll(fileName);
    recentFiles.prepend(fileName);
    if (oldRecentFiles != recentFiles)
        writeRecentFiles(recentFiles, settings);

    setRecentFilesVisible(!recentFiles.isEmpty());
}

void QtSLiMWindow::updateRecentFileActions()
{
    QSettings settings;

    const QStringList recentFiles = readRecentFiles(settings);
    const int count = qMin(int(MaxRecentFiles), recentFiles.size());
    int i = 0;
    for ( ; i < count; ++i) {
        const QString fileName = QFileInfo(recentFiles.at(i)).fileName();
        recentFileActs[i]->setText(fileName);
        recentFileActs[i]->setData(recentFiles.at(i));
        recentFileActs[i]->setVisible(true);
    }
    for ( ; i < MaxRecentFiles; ++i)
        recentFileActs[i]->setVisible(false);
}

void QtSLiMWindow::openRecentFile()
{
    const QAction *action = qobject_cast<const QAction *>(sender());
    
    if (action)
        openFile(action->data().toString());
}

void QtSLiMWindow::clearRecentFiles()
{
    QSettings settings;
    QStringList emptyRecentFiles;
    writeRecentFiles(emptyRecentFiles, settings);
    setRecentFilesVisible(false);
}


//
//  Simulation state
//

std::vector<Subpopulation*> QtSLiMWindow::selectedSubpopulations(void)
{
    std::vector<Subpopulation*> selectedSubpops;
	
	if (!invalidSimulation() && sim)
	{
		Population &population = sim->population_;
		int subpopCount = static_cast<int>(population.subpops_.size());
		auto popIter = population.subpops_.begin();
		
		for (int i = 0; i < subpopCount; ++i)
		{
			Subpopulation *subpop = popIter->second;
			
			if (subpop->gui_selected_)
				selectedSubpops.emplace_back(subpop);
			
			popIter++;
		}
	}
	
	return selectedSubpops;
}         

void QtSLiMWindow::setInvalidSimulation(bool p_invalid)
{
    invalidSimulation_ = p_invalid;
    updateUIEnabling();
}

void QtSLiMWindow::setReachedSimulationEnd(bool p_reachedEnd)
{
    reachedSimulationEnd_ = p_reachedEnd;
    updateUIEnabling();
}

void QtSLiMWindow::setContinuousPlayOn(bool p_flag)
{
    continuousPlayOn_ = p_flag;
    updateUIEnabling();
}

void QtSLiMWindow::setGenerationPlayOn(bool p_flag)
{
    generationPlayOn_ = p_flag;
    updateUIEnabling();
}

void QtSLiMWindow::setProfilePlayOn(bool p_flag)
{
    profilePlayOn_ = p_flag;
    updateUIEnabling();
}

void QtSLiMWindow::setNonProfilePlayOn(bool p_flag)
{
    nonProfilePlayOn_ = p_flag;
    updateUIEnabling();
}

void QtSLiMWindow::showTerminationMessage(QString terminationMessage)
{
    //qDebug() << terminationMessage;
    
    // Depending on the circumstances of the error, we might be able to select a range in our input file to show what caused the error
	if (!changedSinceRecycle())
		ui->scriptTextEdit->selectErrorRange();
    
    // Show an error sheet/panel
    QString fullMessage(terminationMessage);
    
    fullMessage.append("\nThis error has invalidated the simulation; it cannot be run further.  Once the script is fixed, you can recycle the simulation and try again.");
    
    QMessageBox messageBox(this);
    messageBox.setText("Simulation Runtime Error");
    messageBox.setInformativeText(fullMessage);
    messageBox.setIcon(QMessageBox::Warning);
    messageBox.setWindowModality(Qt::WindowModal);
    messageBox.setFixedWidth(700);      // seems to be ignored
    messageBox.exec();
    
    // Show the error in the status bar also
    statusBar()->setStyleSheet("color: #cc0000; font-size: 11px;");
    statusBar()->showMessage(terminationMessage.trimmed());
}

void QtSLiMWindow::checkForSimulationTermination(void)
{
    std::string &&terminationMessage = gEidosTermination.str();

    if (!terminationMessage.empty())
    {
        QString message = QString::fromStdString(terminationMessage);

        gEidosTermination.clear();
        gEidosTermination.str("");

        emit terminationWithMessage(message);
        
        // Now we need to clean up so we are in a displayable state.  Note that we don't even attempt to dispose
        // of the old simulation object; who knows what state it is in, touching it might crash.
        sim = nullptr;
        slimgui = nullptr;

        Eidos_FreeRNG(sim_RNG);

        setReachedSimulationEnd(true);
        setInvalidSimulation(true);
    }
}

void QtSLiMWindow::startNewSimulationFromScript(void)
{
    if (sim)
    {
        delete sim;
        sim = nullptr;
    }
    if (slimgui)
    {
        delete slimgui;
        slimgui = nullptr;
    }

    // Free the old simulation RNG and let SLiM make one for us
    Eidos_FreeRNG(sim_RNG);

    if (EIDOS_GSL_RNG)
        qDebug() << "gEidos_RNG already set up in startNewSimulationFromScript!";

    std::istringstream infile(scriptString);

    try
    {
        sim = new SLiMSim(infile);
        sim->InitializeRNGFromSeed(nullptr);

        // We take over the RNG instance that SLiMSim just made, since each SLiMgui window has its own RNG
        sim_RNG = gEidos_RNG;
        EIDOS_BZERO(&gEidos_RNG, sizeof(Eidos_RNG_State));

        // We also reset various Eidos/SLiM instance state; each SLiMgui window is independent
        sim_next_pedigree_id = 0;
        sim_next_mutation_id = 0;
        sim_suppress_warnings = false;

        // The current working directory was set up in -init to be ~/Desktop, and should not be reset here; if the
        // user has changed it, that change ought to stick across recycles.  So this bounces us back to the last dir chosen.
        sim_working_dir = sim_requested_working_dir;

        setReachedSimulationEnd(false);
        setInvalidSimulation(false);
        hasImported_ = false;
    }
    catch (...)
    {
        if (sim)
            sim->simulation_valid_ = false;
        setReachedSimulationEnd(true);
        checkForSimulationTermination();
    }

    if (sim)
    {
        // make a new SLiMgui instance to represent SLiMgui in Eidos
        slimgui = new SLiMgui(*sim, this);

        // set up the "slimgui" symbol for it immediately
        sim->simulation_constants_->InitializeConstantSymbolEntry(slimgui->SymbolTableEntry());
    }
}

void QtSLiMWindow::setScriptStringAndInitializeSimulation(std::string string)
{
    scriptString = string;
    startNewSimulationFromScript();
}

void QtSLiMWindow::updateOutputTextView(void)
{
    std::string &&newOutput = gSLiMOut.str();
	
	if (!newOutput.empty())
	{
        QString str = QString::fromStdString(newOutput);
		
		// So, ideally we would stay pinned at the bottom if the user had scrolled to the bottom, but would stay
		// at the user's chosen scroll position above the bottom if they chose such a position.  Unfortunately,
		// this doesn't seem to work.  I'm not quite sure why.  Particularly when large amounts of output get
		// added quickly, the scroller doesn't seem to catch up, and then it reads here as not being at the
		// bottom, and so we become unpinned even though we used to be pinned.  I'm going to just give up, for
		// now, and always scroll to the bottom when new output comes out.  That's what many other such apps
		// do anyway; it's a little annoying if you're trying to read old output, but so it goes.
		
		//NSScrollView *enclosingScrollView = [outputTextView enclosingScrollView];
		//BOOL scrolledToBottom = YES; //(![enclosingScrollView hasVerticalScroller] || [[enclosingScrollView verticalScroller] doubleValue] == 1.0);
		
        // ui->outputTextEdit->append(str) would seem the obvious thing to do, but that adds an extra newline (!),
        // so it can't be used.  WTF.  The solution here does not preserve the user's scroll position; see discussion at
        // https://stackoverflow.com/questions/13559990/how-to-append-text-to-qplaintextedit-without-adding-newline-and-keep-scroll-at
        // which has a complex solution involving subclassing QTextEdit... sigh...
        ui->outputTextEdit->moveCursor(QTextCursor::End);
        ui->outputTextEdit->insertPlainText(str);
        ui->outputTextEdit->moveCursor(QTextCursor::End);
        
		//if ([[NSUserDefaults standardUserDefaults] boolForKey:defaultsSyntaxHighlightOutputKey])
		//	[outputTextView recolorAfterChanges];
		
		// if the user was scrolled to the bottom, we keep them there; otherwise, we let them stay where they were
		//if (scrolledToBottom)
		//	[outputTextView scrollRangeToVisible:NSMakeRange([[outputTextView string] length], 0)];
		
		// clear any error flags set on the stream and empty out its string so it is ready to receive new output
		gSLiMOut.clear();
		gSLiMOut.str("");
	}
}

void QtSLiMWindow::updateGenerationCounter(void)
{
    if (!invalidSimulation_)
	{
		if (sim->generation_ == 0)
            ui->generationLineEdit->setText("initialize()");
		else
            ui->generationLineEdit->setText(QString::number(sim->generation_));
	}
	else
        ui->generationLineEdit->setText("");
}

void QtSLiMWindow::updateAfterTickFull(bool fullUpdate)
{
    // fullUpdate is used to suppress some expensive updating to every third update
	if (!fullUpdate)
	{
		if (++partialUpdateCount_ >= 3)
		{
			partialUpdateCount_ = 0;
			fullUpdate = true;
		}
	}
	
	// Check whether the simulation has terminated due to an error; if so, show an error message with a delayed perform
	checkForSimulationTermination();
	
	// The rest of the code here needs to be careful about the invalid state; we do want to update our controls when invalid, but sim is nil.
	bool invalid = invalidSimulation();
	
	if (fullUpdate)
	{
		// FIXME it would be good for this updating to be minimal; reloading the tableview every time, etc., is quite wasteful...
		updateOutputTextView();
		
		// Reloading the subpop tableview is tricky, because we need to preserve the selection across the reload, while also noting that the selection is forced
		// to change when a subpop goes extinct.  The current selection is noted in the gui_selected_ ivar of each subpop.  So what we do here is reload the tableview
		// while suppressing our usual update of our selection state, and then we try to re-impose our selection state on the new tableview content.  If a subpop
		// went extinct, we will fail to notice the selection change; but that is OK, since we force an update of populationView and chromosomeZoomed below anyway.
//		reloadingSubpopTableview = true;
        populationTableModel_->reloadTable();
		
//		if (invalid || !sim)
//		{
//			[subpopTableView deselectAll:nil];
//		}
//		else
//		{
//			Population &population = sim->population_;
//			int subpopCount = (int)population.subpops_.size();
//			auto popIter = population.subpops_.begin();
//			NSMutableIndexSet *indicesToSelect = [NSMutableIndexSet indexSet];
			
//			for (int i = 0; i < subpopCount; ++i)
//			{
//				if (popIter->second->gui_selected_)
//					[indicesToSelect addIndex:i];
				
//				popIter++;
//			}
			
//			[subpopTableView selectRowIndexes:indicesToSelect byExtendingSelection:NO];
//		}
		
//		reloadingSubpopTableview = false;
//		[subpopTableView setNeedsDisplay];
	}
	
	// Now update our other UI, some of which depends upon the state of subpopTableView
    std::vector<Subpopulation*> selectedSubpops = selectedSubpopulations();
    ui->individualsWidget->tileSubpopulations(selectedSubpops);
    ui->individualsWidget->update();
    ui->chromosomeZoomed->update();
	
	if (fullUpdate)
		updateGenerationCounter();
	
	// Update stuff that only needs updating when the script is re-parsed, not after every tick
//	if (invalid || sim->mutation_types_changed_)
//	{
//		[mutTypeTableView reloadData];
//		[mutTypeTableView setNeedsDisplay];
		
//		if (sim)
//			sim->mutation_types_changed_ = false;
//	}
	
//	if (invalid || sim->genomic_element_types_changed_)
//	{
//		[genomicElementTypeTableView reloadData];
//		[genomicElementTypeTableView setNeedsDisplay];
		
//		if (sim)
//			sim->genomic_element_types_changed_ = false;
//	}
	
//	if (invalid || sim->interaction_types_changed_)
//	{
//		[interactionTypeTableView reloadData];
//		[interactionTypeTableView setNeedsDisplay];
		
//		if (sim)
//			sim->interaction_types_changed_ = false;
//	}
	
//	if (invalid || sim->scripts_changed_)
//	{
//		[scriptBlocksTableView reloadData];
//		[scriptBlocksTableView setNeedsDisplay];
		
//		if (sim)
//			sim->scripts_changed_ = false;
//	}
	
	if (invalid || sim->chromosome_changed_)
	{
		ui->chromosomeOverview->restoreLastSelection();
		ui->chromosomeOverview->update();
		
		if (sim)
			sim->chromosome_changed_ = false;
	}
	
	// Update graph windows as well; this will usually trigger a setNeedsDisplay:YES but may do other updating work as well
//	if (fullUpdate)
//		[self sendAllLinkedViewsSelector:@selector(updateAfterTick)];
}

void QtSLiMWindow::updatePlayButtonIcon(bool pressed)
{
    bool highlighted = ui->playButton->isChecked() ^ pressed;
    
    ui->playButton->setIcon(QIcon(highlighted ? ":/buttons/play_H.png" : ":/buttons/play.png"));
}

void QtSLiMWindow::updateProfileButtonIcon(bool pressed)
{
    bool highlighted = ui->profileButton->isChecked() ^ pressed;
    
    if (profilePlayOn_)
        ui->profileButton->setIcon(QIcon(highlighted ? ":/buttons/profile_R.png" : ":/buttons/profile_RH.png"));    // flipped intentionally
    else
        ui->profileButton->setIcon(QIcon(highlighted ? ":/buttons/profile_H.png" : ":/buttons/profile.png"));
}

void QtSLiMWindow::updateRecycleButtonIcon(bool pressed)
{
    if (slimChangeCount)
        ui->recycleButton->setIcon(QIcon(pressed ? ":/buttons/recycle_GH.png" : ":/buttons/recycle_G.png"));
    else
        ui->recycleButton->setIcon(QIcon(pressed ? ":/buttons/recycle_H.png" : ":/buttons/recycle.png"));
}

void QtSLiMWindow::updateUIEnabling(void)
{
    ui->playOneStepButton->setEnabled(!reachedSimulationEnd_ && !continuousPlayOn_ && !generationPlayOn_);
    ui->playButton->setEnabled(!reachedSimulationEnd_ && !profilePlayOn_ && !generationPlayOn_);
    ui->profileButton->setEnabled(!reachedSimulationEnd_ && !nonProfilePlayOn_ && !generationPlayOn_);
    ui->recycleButton->setEnabled(!continuousPlayOn_ && !generationPlayOn_);
    
    ui->playSpeedSlider->setEnabled(!generationPlayOn_ && !invalidSimulation_);
    ui->generationLineEdit->setEnabled(!reachedSimulationEnd_ && !continuousPlayOn_ && !generationPlayOn_);

    ui->showMutationsButton->setEnabled(!invalidSimulation_);
    ui->showChromosomeMapsButton->setEnabled(!invalidSimulation_);
    ui->showGenomicElementsButton->setEnabled(!invalidSimulation_);
    ui->showFixedSubstitutionsButton->setEnabled(!invalidSimulation_);
    
    ui->checkScriptButton->setEnabled(!continuousPlayOn_ && !generationPlayOn_);
    ui->prettyprintButton->setEnabled(!continuousPlayOn_ && !generationPlayOn_);
    ui->scriptHelpButton->setEnabled(true);
    ui->consoleButton->setEnabled(true);
    ui->browserButton->setEnabled(true);
    
    ui->clearOutputButton->setEnabled(!invalidSimulation_);
    ui->dumpPopulationButton->setEnabled(!invalidSimulation_);
    ui->graphPopupButton->setEnabled(!invalidSimulation_);
    ui->changeDirectoryButton->setEnabled(!invalidSimulation_);
    
    ui->scriptTextEdit->setReadOnly(continuousPlayOn_ || generationPlayOn_);
    ui->outputTextEdit->setReadOnly(true);
    
    ui->generationLabel->setEnabled(!invalidSimulation_);
    ui->outputHeaderLabel->setEnabled(!invalidSimulation_);
    
    if (consoleController)
        consoleController->setInterfaceEnabled(!(continuousPlayOn_ || generationPlayOn_));
}

//
//  profiling
//

#if defined(SLIMGUI) && (SLIMPROFILING == 1)

void QtSLiMWindow::colorScriptWithProfileCountsFromNode(const EidosASTNode *node, double elapsedTime, int32_t baseIndex, QTextDocument *doc, QTextCharFormat &baseFormat)
{
    // First color the range for this node
	eidos_profile_t count = node->profile_total_;
	
	if (count > 0)
	{
		int32_t start = 0, end = 0;
		
		node->FullUTF16Range(&start, &end);
		
		start -= baseIndex;
		end -= baseIndex;
		
		QTextCursor colorCursor(doc);
        colorCursor.setPosition(start);
        colorCursor.setPosition(end, QTextCursor::KeepAnchor); // +1?
        
        QColor backgroundColor = slimColorForFraction(Eidos_ElapsedProfileTime(count) / elapsedTime);
		QTextCharFormat colorFormat = baseFormat;
        
        colorFormat.setBackground(backgroundColor);
        colorCursor.setCharFormat(colorFormat);
	}
	
	// Then let child nodes color
	for (const EidosASTNode *child : node->children_)
        colorScriptWithProfileCountsFromNode(child, elapsedTime, baseIndex, doc, baseFormat);
}

void QtSLiMWindow::displayProfileResults(void)
{
    // Make a new window to show the profile results
    QWidget *window = new QWidget(this, Qt::Window);    // the profile window has us as a parent, but is still a standalone window
    QString title = window->windowTitle();
    
    if (title.length() == 0)
        title = "Untitled";
    
    window->setWindowTitle("Profile Report for " + title);
    window->setMinimumSize(500, 200);
    window->resize(500, 600);
    window->move(50, 50);
    
    // Make a QTextEdit to hold the results
    QHBoxLayout *layout = new QHBoxLayout;
    QTextEdit *textEdit = new QTextEdit();
    
    window->setLayout(layout);
    
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(textEdit);
    
    textEdit->setFrameStyle(QFrame::NoFrame);
    textEdit->setReadOnly(true);
    
    QTextDocument *doc = textEdit->document();
    QTextCursor tc = textEdit->textCursor();
    
    doc->setDocumentMargin(10);
    
    // Make the QTextCharFormat objects we will use
    QFont optima18b("Optima", 18, QFont::Bold);
    QFont optima14b("Optima", 14, QFont::Bold);
    QFont optima13("Optima", 13);
    QFont optima13i("Optima", 13, -1, true);
    QFont optima8("Optima", 8);
    QFont optima3("Optima", 3);
    QFont menlo11("Menlo", 11);
    
    QTextCharFormat optima18b_d, optima14b_d, optima13_d, optima13i_d, optima8_d, optima3_d, menlo11_d;
    
    optima18b_d.setFont(optima18b);
    optima14b_d.setFont(optima14b);
    optima13_d.setFont(optima13);
    optima13i_d.setFont(optima13i);
    optima8_d.setFont(optima8);
    optima3_d.setFont(optima3);
    menlo11_d.setFont(menlo11);
    
    // Adjust the tab width to the monospace font we have chosen
    int tabWidth = 0;
    QFontMetrics fm(menlo11);
    
    //tabWidth = fm.horizontalAdvance("   ");   // added in Qt 5.11
    tabWidth = fm.width("   ");                 // deprecated (in 5.11, I assume)
    
    textEdit->setTabStopWidth(tabWidth);
    
    // Build the report attributed string
    QString startDateString = profileStartDate_.toString("M/d/yy, h:mm:ss AP");
    QString endDateString = profileEndDate_.toString("M/d/yy, h:mm:ss AP");
    double elapsedWallClockTime = (profileStartDate_.msecsTo(profileEndDate_)) / 1000.0;
    double elapsedCPUTimeInSLiM = profileElapsedCPUClock / static_cast<double>(CLOCKS_PER_SEC);
	double elapsedWallClockTimeInSLiM = Eidos_ElapsedProfileTime(profileElapsedWallClock);
    
    tc.insertText("Profile Report\n", optima18b_d);
    tc.insertText(" \n", optima3_d);
    
    tc.insertText("Model: " + title + "\n", optima13_d);
    tc.insertText(" \n", optima8_d);
    
    tc.insertText("Run start: " + startDateString + "\n", optima13_d);
    tc.insertText("Run end: " + endDateString + "\n", optima13_d);
    tc.insertText(" \n", optima8_d);
    
    tc.insertText(QString("Elapsed wall clock time: %1 s\n").arg(elapsedWallClockTime, 0, 'f', 2), optima13_d);
    tc.insertText(QString("Elapsed wall clock time inside SLiM core (corrected): %1 s\n").arg(elapsedWallClockTimeInSLiM, 0, 'f', 2), optima13_d);
    tc.insertText(QString("Elapsed CPU time inside SLiM core (uncorrected): %1 s\n").arg(elapsedCPUTimeInSLiM, 0, 'f', 2), optima13_d);
    tc.insertText(QString("Elapsed generations: %1%2\n").arg(continuousPlayGenerationsCompleted_).arg((profileStartGeneration == 0) ? " (including initialize)" : ""), optima13_d);
    tc.insertText(" \n", optima8_d);
    
    tc.insertText(QString("Profile block external overhead: %1 ticks (%2 s)\n").arg(gEidos_ProfileOverheadTicks, 0, 'f', 2).arg(gEidos_ProfileOverheadSeconds, 0, 'g', 4), optima13_d);
    tc.insertText(QString("Profile block internal lag: %1 ticks (%2 s)\n").arg(gEidos_ProfileLagTicks, 0, 'f', 2).arg(gEidos_ProfileLagSeconds, 0, 'g', 4), optima13_d);
    tc.insertText(" \n", optima8_d);
    
    tc.insertText(QString("Average generation SLiM memory use: %1\n").arg(stringForByteCount(sim->profile_total_memory_usage_.totalMemoryUsage / static_cast<size_t>(sim->total_memory_tallies_))), optima13_d);
    tc.insertText(QString("Final generation SLiM memory use: %1\n").arg(stringForByteCount(sim->profile_last_memory_usage_.totalMemoryUsage)), optima13_d);
    
	//
	//	Generation stage breakdown
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		bool isWF = (sim->ModelType() == SLiMModelType::kModelTypeWF);
		double elapsedStage0Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[0]);
		double elapsedStage1Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[1]);
		double elapsedStage2Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[2]);
		double elapsedStage3Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[3]);
		double elapsedStage4Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[4]);
		double elapsedStage5Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[5]);
		double elapsedStage6Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[6]);
		double percentStage0 = (elapsedStage0Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage1 = (elapsedStage1Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage2 = (elapsedStage2Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage3 = (elapsedStage3Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage4 = (elapsedStage4Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage5 = (elapsedStage5Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage6 = (elapsedStage6Time / elapsedWallClockTimeInSLiM) * 100.0;
		int fw = 4;
		
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage0Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage1Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage2Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage3Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage4Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage5Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage6Time)))));
		
		tc.insertText(" \n", optima13_d);
		tc.insertText("Generation stage breakdown\n", optima14b_d);
		tc.insertText(" \n", optima3_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage0Time, fw, 'f', 2).arg(percentStage0, 5, 'f', 2), menlo11_d);
		tc.insertText(" : initialize() callback execution\n", optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage1Time, fw, 'f', 2).arg(percentStage1, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 1 – early() event execution\n" : " : stage 1 – offspring generation\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage2Time, fw, 'f', 2).arg(percentStage2, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 2 – offspring generation\n" : " : stage 2 – early() event execution\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage3Time, fw, 'f', 2).arg(percentStage3, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 3 – bookkeeping (fixed mutation removal, etc.)\n" : " : stage 3 – fitness calculation\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage4Time, fw, 'f', 2).arg(percentStage4, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 4 – generation swap\n" : " : stage 4 – viability/survival selection\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage5Time, fw, 'f', 2).arg(percentStage5, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 5 – late() event execution\n" : " : stage 5 – bookkeeping (fixed mutation removal, etc.)\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage6Time, fw, 'f', 2).arg(percentStage6, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 6 – fitness calculation\n" : " : stage 6 – late() event execution\n"), optima13_d);
	}
	
	//
	//	Callback type breakdown
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		double elapsedType0Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[0]);
		double elapsedType1Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[1]);
		double elapsedType2Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[2]);
		double elapsedType3Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[3]);
		double elapsedType4Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[4]);
		double elapsedType5Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[5]);
		double elapsedType6Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[6]);
		double elapsedType7Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[7]);
		double elapsedType8Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[8]);
		double elapsedType9Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[9]);
		double elapsedType10Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[10]);
		double percentType0 = (elapsedType0Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType1 = (elapsedType1Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType2 = (elapsedType2Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType3 = (elapsedType3Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType4 = (elapsedType4Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType5 = (elapsedType5Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType6 = (elapsedType6Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType7 = (elapsedType7Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType8 = (elapsedType8Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType9 = (elapsedType9Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType10 = (elapsedType10Time / elapsedWallClockTimeInSLiM) * 100.0;
		int fw = 4, fw2 = 4;
		
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType0Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType1Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType2Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType3Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType4Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType5Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType6Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType7Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType8Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType9Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType10Time)))));
		
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType0)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType1)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType2)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType3)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType4)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType5)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType6)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType7)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType8)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType9)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType10)))));
		
		tc.insertText(" \n", optima13_d);
		tc.insertText("Callback type breakdown\n", optima14b_d);
		tc.insertText(" \n", optima3_d);
		
		// Note these are out of numeric order, but in generation-cycle order
		if (sim->ModelType() == SLiMModelType::kModelTypeWF)
		{
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType2Time, fw, 'f', 2).arg(percentType2, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : initialize() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType0Time, fw, 'f', 2).arg(percentType0, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : early() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType6Time, fw, 'f', 2).arg(percentType6, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : mateChoice() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType8Time, fw, 'f', 2).arg(percentType8, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : recombination() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType9Time, fw, 'f', 2).arg(percentType9, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : mutation() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType7Time, fw, 'f', 2).arg(percentType7, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : modifyChild() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType1Time, fw, 'f', 2).arg(percentType1, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : late() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType3Time, fw, 'f', 2).arg(percentType3, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : fitness() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType4Time, fw, 'f', 2).arg(percentType4, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : fitness() callbacks (global)\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType5Time, fw, 'f', 2).arg(percentType5, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : interaction() callbacks\n", optima13_d);
		}
		else
		{
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType2Time, fw, 'f', 2).arg(percentType2, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : initialize() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType10Time, fw, 'f', 2).arg(percentType10, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : reproduction() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType8Time, fw, 'f', 2).arg(percentType8, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : recombination() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType9Time, fw, 'f', 2).arg(percentType9, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : mutation() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType7Time, fw, 'f', 2).arg(percentType7, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : modifyChild() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType0Time, fw, 'f', 2).arg(percentType0, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : early() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType3Time, fw, 'f', 2).arg(percentType3, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : fitness() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType4Time, fw, 'f', 2).arg(percentType4, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : fitness() callbacks (global)\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType1Time, fw, 'f', 2).arg(percentType1, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : late() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType5Time, fw, 'f', 2).arg(percentType5, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : interaction() callbacks\n", optima13_d);
		}
	}
	
	//
	//	Script block profiles
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		{
			std::vector<SLiMEidosBlock*> &script_blocks = sim->AllScriptBlocks();
			
			// Convert the profile counts in all script blocks into self counts (excluding the counts of nodes below them)
			for (SLiMEidosBlock *script_block : script_blocks)
				if (script_block->type_ != SLiMEidosBlockType::SLiMEidosUserDefinedFunction)		// exclude function blocks; not user-visible
					script_block->root_node_->ConvertProfileTotalsToSelfCounts();
		}
		{
			tc.insertText(" \n", optima13_d);
			tc.insertText("Script block profiles (as a fraction of corrected wall clock time)\n", optima14b_d);
			tc.insertText(" \n", optima3_d);
			
			std::vector<SLiMEidosBlock*> &script_blocks = sim->AllScriptBlocks();
			bool firstBlock = true, hiddenInconsequentialBlocks = false;
			
			for (SLiMEidosBlock *script_block : script_blocks)
			{
				if (script_block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
					continue;
				
				const EidosASTNode *profile_root = script_block->root_node_;
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					if (!firstBlock)
						tc.insertText(" \n \n", menlo11_d);
					firstBlock = false;
					
					const std::string &script_std_string = profile_root->token_->token_string_;
					QString script_string = QString::fromStdString(script_std_string);
					
					tc.insertText(QString("%1 s (%2%):\n").arg(total_block_time, 0, 'f', 2).arg(percent_block_time, 0, 'f', 2), menlo11_d);
					tc.insertText(" \n", optima3_d);
					
                    int colorBase = tc.position();
                    tc.insertText(script_string, menlo11_d);
                    colorScriptWithProfileCountsFromNode(profile_root, elapsedWallClockTimeInSLiM, profile_root->token_->token_UTF16_start_ - colorBase, doc, menlo11_d);
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
			{
				tc.insertText(" \n", menlo11_d);
				tc.insertText(" \n", optima3_d);
				tc.insertText("(blocks using < 0.01 s and < 0.01% of total wall clock time are not shown)", optima13i_d);
			}
		}
		{
			tc.insertText(" \n", menlo11_d);
			tc.insertText(" \n", optima13_d);
			tc.insertText("Script block profiles (as a fraction of within-block wall clock time)\n", optima14b_d);
			tc.insertText(" \n", optima3_d);
			
			std::vector<SLiMEidosBlock*> &script_blocks = sim->AllScriptBlocks();
			bool firstBlock = true, hiddenInconsequentialBlocks = false;
			
			for (SLiMEidosBlock *script_block : script_blocks)
			{
				if (script_block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
					continue;
				
				const EidosASTNode *profile_root = script_block->root_node_;
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					if (!firstBlock)
						tc.insertText(" \n \n", menlo11_d);
					firstBlock = false;
					
					const std::string &script_std_string = profile_root->token_->token_string_;
                    QString script_string = QString::fromStdString(script_std_string);
					
					tc.insertText(QString("%1 s (%2%):\n").arg(total_block_time, 0, 'f', 2).arg(percent_block_time, 0, 'f', 2), menlo11_d);
					tc.insertText(" \n", optima3_d);
					
                    int colorBase = tc.position();
                    tc.insertText(script_string, menlo11_d);
                    if (total_block_time > 0.0)
                        colorScriptWithProfileCountsFromNode(profile_root, total_block_time, profile_root->token_->token_UTF16_start_ - colorBase, doc, menlo11_d);
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
			{
				tc.insertText(" \n", menlo11_d);
				tc.insertText(" \n", optima3_d);
				tc.insertText("(blocks using < 0.01 s and < 0.01% of total wall clock time are not shown)", optima13i_d);
			}
		}
	}
	
	//
	//	User-defined functions (if any)
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		EidosFunctionMap &function_map = sim->FunctionMap();
		std::vector<const EidosFunctionSignature *> userDefinedFunctions;
		
		for (auto functionPairIter = function_map.begin(); functionPairIter != function_map.end(); ++functionPairIter)
		{
			const EidosFunctionSignature *signature = functionPairIter->second.get();
			
			if (signature->body_script_ && signature->user_defined_)
			{
				signature->body_script_->AST()->ConvertProfileTotalsToSelfCounts();
				userDefinedFunctions.push_back(signature);
			}
		}
		
		if (userDefinedFunctions.size())
		{
			tc.insertText(" \n", menlo11_d);
			tc.insertText(" \n", optima13_d);
			tc.insertText("User-defined functions (as a fraction of corrected wall clock time)\n", optima14b_d);
			tc.insertText(" \n", optima3_d);
			
			bool firstBlock = true, hiddenInconsequentialBlocks = false;
			
			for (const EidosFunctionSignature *signature : userDefinedFunctions)
			{
				const EidosASTNode *profile_root = signature->body_script_->AST();
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					if (!firstBlock)
						tc.insertText(" \n \n", menlo11_d);
					firstBlock = false;
					
					const std::string &script_std_string = profile_root->token_->token_string_;
					QString script_string = QString::fromStdString(script_std_string);
					const std::string &&signature_string = signature->SignatureString();
					QString signatureString = QString::fromStdString(signature_string);
					
					tc.insertText(QString("%1 s (%2%):\n").arg(total_block_time, 0, 'f', 2).arg(percent_block_time, 0, 'f', 2), menlo11_d);
					tc.insertText(" \n", optima3_d);
					tc.insertText(signatureString + "\n", menlo11_d);
					
                    int colorBase = tc.position();
                    tc.insertText(script_string, menlo11_d);
                    colorScriptWithProfileCountsFromNode(profile_root, elapsedWallClockTimeInSLiM, profile_root->token_->token_UTF16_start_ - colorBase, doc, menlo11_d);
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
			{
				tc.insertText(" \n", menlo11_d);
				tc.insertText(" \n", optima3_d);
				tc.insertText("(functions using < 0.01 s and < 0.01% of total wall clock time are not shown)", optima13i_d);
			}
		}
		if (userDefinedFunctions.size())
		{
			tc.insertText(" \n", menlo11_d);
			tc.insertText(" \n", optima13_d);
			tc.insertText("User-defined functions (as a fraction of within-block wall clock time)\n", optima14b_d);
			tc.insertText(" \n", optima3_d);
			
			bool firstBlock = true, hiddenInconsequentialBlocks = false;
			
			for (const EidosFunctionSignature *signature : userDefinedFunctions)
			{
				const EidosASTNode *profile_root = signature->body_script_->AST();
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					if (!firstBlock)
						tc.insertText(" \n \n", menlo11_d);
					firstBlock = false;
					
					const std::string &script_std_string = profile_root->token_->token_string_;
					QString script_string = QString::fromStdString(script_std_string);
					const std::string &&signature_string = signature->SignatureString();
					QString signatureString = QString::fromStdString(signature_string);
					
					tc.insertText(QString("%1 s (%2%):\n").arg(total_block_time, 0, 'f', 2).arg(percent_block_time, 0, 'f', 2), menlo11_d);
					tc.insertText(" \n", optima3_d);
					tc.insertText(signatureString + "\n", menlo11_d);
					
                    int colorBase = tc.position();
                    tc.insertText(script_string, menlo11_d);
                    if (total_block_time > 0.0)
                        colorScriptWithProfileCountsFromNode(profile_root, total_block_time, profile_root->token_->token_UTF16_start_ - colorBase, doc, menlo11_d);
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
			{
				tc.insertText(" \n", menlo11_d);
				tc.insertText(" \n", optima3_d);
				tc.insertText("(functions using < 0.01 s and < 0.01% of total wall clock time are not shown)", optima13i_d);
			}
		}
	}
	
#if SLIM_USE_NONNEUTRAL_CACHES
	//
	//	MutationRun metrics
	//
	{
		int64_t power_tallies[20];	// we only go up to 1024 mutruns right now, but this gives us some headroom
		int64_t power_tallies_total = static_cast<int>(sim->profile_mutcount_history_.size());
		
		for (int power = 0; power < 20; ++power)
			power_tallies[power] = 0;
		
		for (int32_t count : sim->profile_mutcount_history_)
		{
			int power = static_cast<int>(round(log2(count)));
			
			power_tallies[power]++;
		}
		
		tc.insertText(" \n", menlo11_d);
		tc.insertText(" \n", optima13_d);
		tc.insertText("MutationRun usage\n", optima14b_d);
		tc.insertText(" \n", optima3_d);
		
		for (int power = 0; power < 20; ++power)
		{
			if (power_tallies[power] > 0)
			{
				tc.insertText(QString("%1%").arg((power_tallies[power] / static_cast<double>(power_tallies_total)) * 100.0, 6, 'f', 2), menlo11_d);
				tc.insertText(QString(" of generations : %1 mutation runs per genome\n").arg(static_cast<int>(round(pow(2.0, power)))), optima13_d);
			}
		}
		
		
		int64_t regime_tallies[3];
		int64_t regime_tallies_total = static_cast<int>(sim->profile_nonneutral_regime_history_.size());
		
		for (int regime = 0; regime < 3; ++regime)
			regime_tallies[regime] = 0;
		
		for (int32_t regime : sim->profile_nonneutral_regime_history_)
			if ((regime >= 1) && (regime <= 3))
				regime_tallies[regime - 1]++;
			else
				regime_tallies_total--;
		
		tc.insertText(" \n", optima13_d);
		
		for (int regime = 0; regime < 3; ++regime)
		{
			tc.insertText(QString("%1%").arg((regime_tallies[regime] / static_cast<double>(regime_tallies_total)) * 100.0, 6, 'f', 2), menlo11_d);
			tc.insertText(QString(" of generations : regime %1 (%2)\n").arg(regime + 1).arg(regime == 0 ? "no fitness callbacks" : (regime == 1 ? "constant neutral fitness callbacks only" : "unpredictable fitness callbacks present")), optima13_d);
		}
		
		
		tc.insertText(" \n", optima13_d);
		
		tc.insertText(QString("%1").arg(sim->profile_mutation_total_usage_), menlo11_d);
		tc.insertText(" mutations referenced, summed across all generations\n", optima13_d);
		
		tc.insertText(QString("%1").arg(sim->profile_nonneutral_mutation_total_), menlo11_d);
		tc.insertText(" mutations considered potentially nonneutral\n", optima13_d);
		
		tc.insertText(QString("%1%").arg(((sim->profile_mutation_total_usage_ - sim->profile_nonneutral_mutation_total_) / static_cast<double>(sim->profile_mutation_total_usage_)) * 100.0, 0, 'f', 2), menlo11_d);
		tc.insertText(" of mutations excluded from fitness calculations\n", optima13_d);
		
		tc.insertText(QString("%1").arg(sim->profile_max_mutation_index_), menlo11_d);
		tc.insertText(" maximum simultaneous mutations\n", optima13_d);
		
		
		tc.insertText(" \n", optima13_d);
		
		tc.insertText(QString("%1").arg(sim->profile_mutrun_total_usage_), menlo11_d);
		tc.insertText(" mutation runs referenced, summed across all generations\n", optima13_d);
		
		tc.insertText(QString("%1").arg(sim->profile_unique_mutrun_total_), menlo11_d);
		tc.insertText(" unique mutation runs maintained among those\n", optima13_d);
		
		tc.insertText(QString("%1%").arg((sim->profile_mutrun_nonneutral_recache_total_ / static_cast<double>(sim->profile_unique_mutrun_total_)) * 100.0, 6, 'f', 2), menlo11_d);
		tc.insertText(" of mutation run nonneutral caches rebuilt per generation\n", optima13_d);
		
		tc.insertText(QString("%1%").arg(((sim->profile_mutrun_total_usage_ - sim->profile_unique_mutrun_total_) / static_cast<double>(sim->profile_mutrun_total_usage_)) * 100.0, 6, 'f', 2), menlo11_d);
		tc.insertText(" of mutation runs shared among genomes", optima13_d);
	}
#endif
	
	{
		//
		//	Memory usage metrics
		//
		SLiM_MemoryUsage &mem_tot = sim->profile_total_memory_usage_;
		SLiM_MemoryUsage &mem_last = sim->profile_last_memory_usage_;
		uint64_t div = static_cast<uint64_t>(sim->total_memory_tallies_);
		double ddiv = sim->total_memory_tallies_;
		double average_total = mem_tot.totalMemoryUsage / ddiv;
		double final_total = mem_last.totalMemoryUsage;
		
		tc.insertText(" \n", menlo11_d);
		tc.insertText(" \n", optima13_d);
		tc.insertText("SLiM memory usage (average / final generation)\n", optima14b_d);
		tc.insertText(" \n", optima3_d);
		
        QTextCharFormat colored_menlo = menlo11_d;
        
		// Chromosome
		tc.insertText(attributedStringForByteCount(mem_tot.chromosomeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.chromosomeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : Chromosome object\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.chromosomeMutationRateMaps / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.chromosomeMutationRateMaps, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : mutation rate maps\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.chromosomeRecombinationRateMaps / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.chromosomeRecombinationRateMaps, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : recombination rate maps\n", optima13_d);

		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.chromosomeAncestralSequence / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.chromosomeAncestralSequence, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : ancestral nucleotides\n", optima13_d);
		
		// Genome
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.genomeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.genomeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Genome objects (%1 / %2)\n").arg(mem_tot.genomeObjects_count / ddiv, 0, 'f', 2).arg(mem_last.genomeObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.genomeExternalBuffers / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.genomeExternalBuffers, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : external MutationRun* buffers\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.genomeUnusedPoolSpace / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.genomeUnusedPoolSpace, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool space\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.genomeUnusedPoolBuffers / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.genomeUnusedPoolBuffers, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool buffers\n", optima13_d);
		
		// GenomicElement
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.genomicElementObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.genomicElementObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : GenomicElement objects (%1 / %2)\n").arg(mem_tot.genomicElementObjects_count / ddiv, 0, 'f', 2).arg(mem_last.genomicElementObjects_count), optima13_d);
		
		// GenomicElementType
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.genomicElementTypeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.genomicElementTypeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : GenomicElementType objects (%1 / %2)\n").arg(mem_tot.genomicElementTypeObjects_count / ddiv, 0, 'f', 2).arg(mem_last.genomicElementTypeObjects_count), optima13_d);
		
		// Individual
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.individualObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.individualObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Individual objects (%1 / %2)\n").arg(mem_tot.individualObjects_count / ddiv, 0, 'f', 2).arg(mem_last.individualObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.individualUnusedPoolSpace / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.individualUnusedPoolSpace, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool space\n", optima13_d);
		
		// InteractionType
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.interactionTypeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.interactionTypeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : InteractionType objects (%1 / %2)\n").arg(mem_tot.interactionTypeObjects_count / ddiv, 0, 'f', 2).arg(mem_last.interactionTypeObjects_count), optima13_d);
		
		if (mem_tot.interactionTypeObjects_count || mem_last.interactionTypeObjects_count)
		{
			tc.insertText("   ", menlo11_d);
			tc.insertText(attributedStringForByteCount(mem_tot.interactionTypeKDTrees / div, average_total, colored_menlo), colored_menlo);
			tc.insertText(" / ", optima13_d);
			tc.insertText(attributedStringForByteCount(mem_last.interactionTypeKDTrees, final_total, colored_menlo), colored_menlo);
			tc.insertText(" : k-d trees\n", optima13_d);
			
			tc.insertText("   ", menlo11_d);
			tc.insertText(attributedStringForByteCount(mem_tot.interactionTypePositionCaches / div, average_total, colored_menlo), colored_menlo);
			tc.insertText(" / ", optima13_d);
			tc.insertText(attributedStringForByteCount(mem_last.interactionTypePositionCaches, final_total, colored_menlo), colored_menlo);
			tc.insertText(" : position caches\n", optima13_d);
			
			tc.insertText("   ", menlo11_d);
			tc.insertText(attributedStringForByteCount(mem_tot.interactionTypeSparseArrays / div, average_total, colored_menlo), colored_menlo);
			tc.insertText(" / ", optima13_d);
			tc.insertText(attributedStringForByteCount(mem_last.interactionTypeSparseArrays, final_total, colored_menlo), colored_menlo);
			tc.insertText(" : sparse arrays\n", optima13_d);
		}
		
		// Mutation
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Mutation objects (%1 / %2)\n").arg(mem_tot.mutationObjects_count / ddiv, 0, 'f', 2).arg(mem_last.mutationObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationRefcountBuffer / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationRefcountBuffer, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : refcount buffer\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationUnusedPoolSpace / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationUnusedPoolSpace, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool space\n", optima13_d);
		
		// MutationRun
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationRunObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationRunObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : MutationRun objects (%1 / %2)\n").arg(mem_tot.mutationRunObjects_count / ddiv, 0, 'f', 2).arg(mem_last.mutationRunObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationRunExternalBuffers / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationRunExternalBuffers, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : external MutationIndex buffers\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationRunNonneutralCaches / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationRunNonneutralCaches, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : nonneutral mutation caches\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationRunUnusedPoolSpace / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationRunUnusedPoolSpace, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool space\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationRunUnusedPoolBuffers / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationRunUnusedPoolBuffers, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool buffers\n", optima13_d);
		
		// MutationType
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationTypeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationTypeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : MutationType objects (%1 / %2)\n").arg(mem_tot.mutationTypeObjects_count / ddiv, 0, 'f', 2).arg(mem_last.mutationTypeObjects_count), optima13_d);
		
		// SLiMSim
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.slimsimObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.slimsimObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : SLiMSim object\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.slimsimTreeSeqTables / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.slimsimTreeSeqTables, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : tree-sequence tables\n", optima13_d);
		
		// Subpopulation
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.subpopulationObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.subpopulationObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Subpopulation objects (%1 / %2)\n").arg(mem_tot.subpopulationObjects_count / ddiv, 0, 'f', 2).arg(mem_last.subpopulationObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.subpopulationFitnessCaches / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.subpopulationFitnessCaches, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : fitness caches\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.subpopulationParentTables / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.subpopulationParentTables, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : parent tables\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.subpopulationSpatialMaps / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.subpopulationSpatialMaps, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : spatial maps\n", optima13_d);
		
		if (mem_tot.subpopulationSpatialMapsDisplay || mem_last.subpopulationSpatialMapsDisplay)
		{
			tc.insertText("   ", menlo11_d);
			tc.insertText(attributedStringForByteCount(mem_tot.subpopulationSpatialMapsDisplay / div, average_total, colored_menlo), colored_menlo);
			tc.insertText(" / ", optima13_d);
			tc.insertText(attributedStringForByteCount(mem_last.subpopulationSpatialMapsDisplay, final_total, colored_menlo), colored_menlo);
			tc.insertText(" : spatial map display (QtSLiM only)\n", optima13_d);
		}
		
		// Substitution
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.substitutionObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.substitutionObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Substitution objects (%1 / %2)\n").arg(mem_tot.substitutionObjects_count / ddiv, 0, 'f', 2).arg(mem_last.substitutionObjects_count), optima13_d);
		
		// Eidos
		tc.insertText(" \n", optima8_d);
		tc.insertText("Eidos:\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.eidosASTNodePool / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.eidosASTNodePool, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : EidosASTNode pool\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.eidosSymbolTablePool / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.eidosSymbolTablePool, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : EidosSymbolTable pool\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.eidosValuePool / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.eidosValuePool, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : EidosValue pool", optima13_d);
	}
    
    // Done, show the window
    tc.setPosition(0);
    textEdit->setTextCursor(tc);
    window->show();    
}

void QtSLiMWindow::startProfiling(void)
{
	// prepare for profiling by measuring profile block overhead and lag
	Eidos_PrepareForProfiling();
	
	// initialize counters
	profileElapsedCPUClock = 0;
	profileElapsedWallClock = 0;
	profileStartGeneration = sim->Generation();
	
	// call this first, which has the side effect of emptying out any pending profile counts
	sim->CollectSLiMguiMutationProfileInfo();
	
	// zero out profile counts for generation stages
	sim->profile_stage_totals_[0] = 0;
	sim->profile_stage_totals_[1] = 0;
	sim->profile_stage_totals_[2] = 0;
	sim->profile_stage_totals_[3] = 0;
	sim->profile_stage_totals_[4] = 0;
	sim->profile_stage_totals_[5] = 0;
	sim->profile_stage_totals_[6] = 0;
	
	// zero out profile counts for callback types (note SLiMEidosUserDefinedFunction is excluded; that is not a category we profile)
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosEventEarly)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosEventLate)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosInitializeCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosFitnessCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosInteractionCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosMateChoiceCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosModifyChildCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosRecombinationCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosMutationCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosReproductionCallback)] = 0;
	
	// zero out profile counts for script blocks; dynamic scripts will be zeroed on construction
	std::vector<SLiMEidosBlock*> &script_blocks = sim->AllScriptBlocks();
	
	for (SLiMEidosBlock *script_block : script_blocks)
		if (script_block->type_ != SLiMEidosBlockType::SLiMEidosUserDefinedFunction)	// exclude user-defined functions; not user-visible as blocks
			script_block->root_node_->ZeroProfileTotals();
	
	// zero out profile counts for all user-defined functions
	EidosFunctionMap &function_map = sim->FunctionMap();
	
	for (auto functionPairIter = function_map.begin(); functionPairIter != function_map.end(); ++functionPairIter)
	{
		const EidosFunctionSignature *signature = functionPairIter->second.get();
		
		if (signature->body_script_ && signature->user_defined_)
			signature->body_script_->AST()->ZeroProfileTotals();
	}
	
#if SLIM_USE_NONNEUTRAL_CACHES
	// zero out mutation run metrics
	sim->profile_mutcount_history_.clear();
	sim->profile_nonneutral_regime_history_.clear();
	sim->profile_mutation_total_usage_ = 0;
	sim->profile_nonneutral_mutation_total_ = 0;
	sim->profile_mutrun_total_usage_ = 0;
	sim->profile_unique_mutrun_total_ = 0;
	sim->profile_mutrun_nonneutral_recache_total_ = 0;
	sim->profile_max_mutation_index_ = 0;
#endif
	
	// zero out memory usage metrics
	EIDOS_BZERO(&sim->profile_last_memory_usage_, sizeof(SLiM_MemoryUsage));
	EIDOS_BZERO(&sim->profile_total_memory_usage_, sizeof(SLiM_MemoryUsage));
	sim->total_memory_tallies_ = 0;
}

void QtSLiMWindow::endProfiling(void)
{
    profileEndDate_ = QDateTime::currentDateTime();
}

#endif	// defined(SLIMGUI) && (SLIMPROFILING == 1)


//
//  simulation play mechanics
//

void QtSLiMWindow::willExecuteScript(void)
{
    // Whenever we are about to execute script, we swap in our random number generator; at other times, gEidos_rng is NULL.
    // The goal here is to keep each SLiM window independent in its random number sequence.
    if (EIDOS_GSL_RNG)
        qDebug() << "eidosConsoleWindowControllerWillExecuteScript: gEidos_rng already set up!";

    gEidos_RNG = sim_RNG;

    // We also swap in the pedigree id and mutation id counters; each SLiMgui window is independent
    gSLiM_next_pedigree_id = sim_next_pedigree_id;
    gSLiM_next_mutation_id = sim_next_mutation_id;
    gEidosSuppressWarnings = sim_suppress_warnings;

    // Set the current directory to its value for this window
    errno = 0;
    int retval = chdir(sim_working_dir.c_str());

    if (retval == -1)
        qDebug() << "willExecuteScript: Unable to set the working directory to " << sim_working_dir.c_str() << " (error " << errno << ")";
}

void QtSLiMWindow::didExecuteScript(void)
{
    // Swap our random number generator back out again; see -eidosConsoleWindowControllerWillExecuteScript
    sim_RNG = gEidos_RNG;
    EIDOS_BZERO(&gEidos_RNG, sizeof(Eidos_RNG_State));

    // Swap out our pedigree id and mutation id counters; see -eidosConsoleWindowControllerWillExecuteScript
    // Setting to -100000 here is not necessary, but will maybe help find bugs...
    sim_next_pedigree_id = gSLiM_next_pedigree_id;
    gSLiM_next_pedigree_id = -100000;

    sim_next_mutation_id = gSLiM_next_mutation_id;
    gSLiM_next_mutation_id = -100000;

    sim_suppress_warnings = gEidosSuppressWarnings;
    gEidosSuppressWarnings = false;

    // Get the current working directory; each SLiM window has its own cwd, which may have been changed in script since ...WillExecuteScript:
    sim_working_dir = Eidos_CurrentDirectory();

    // Return to the app's working directory when not running SLiM/Eidos code
    std::string &app_cwd = qtSLiMAppDelegate->QtSLiMCurrentWorkingDirectory();
    errno = 0;
    int retval = chdir(app_cwd.c_str());

    if (retval == -1)
        qDebug() << "didExecuteScript: Unable to set the working directory to " << app_cwd.c_str() << " (error " << errno << ")";
}

bool QtSLiMWindow::runSimOneGeneration(void)
{
    // This method should always be used when calling out to run the simulation, because it swaps the correct random number
    // generator stuff in and out bracketing the call to RunOneGeneration().  This bracketing would need to be done around
    // any other call out to the simulation that caused it to use random numbers, too, such as subsample output.
    bool stillRunning = true;

    willExecuteScript();

#if (defined(SLIMGUI) && (SLIMPROFILING == 1))
	if (profilePlayOn_)
	{
		// We put the wall clock measurements on the inside since we want those to be maximally accurate,
		// as profile report percentages are fractions of the total elapsed wall clock time.
		clock_t startCPUClock = clock();
		SLIM_PROFILE_BLOCK_START();

		stillRunning = sim->RunOneGeneration();

		SLIM_PROFILE_BLOCK_END(profileElapsedWallClock);
		clock_t endCPUClock = clock();

		profileElapsedCPUClock += (endCPUClock - startCPUClock);
	}
    else
#endif
    {
        stillRunning = sim->RunOneGeneration();
    }

    didExecuteScript();

    // We also want to let graphViews know when each generation has finished, in case they need to pull data from the sim.  Note this
    // happens after every generation, not just when we are updating the UI, so drawing and setNeedsDisplay: should not happen here.
    //[self sendAllLinkedViewsSelector:@selector(controllerGenerationFinished)];

    return stillRunning;
}

void QtSLiMWindow::_continuousPlay(void)
{
    // NOTE this code is parallel to the code in _continuousProfile()
	if (!invalidSimulation_)
	{
        QElapsedTimer startTimer;
        startTimer.start();
        
		double speedSliderValue = ui->playSpeedSlider->value() / 100.0;     // scale is 0 to 100, since only integer values are allowed by QSlider
		double intervalSinceStarting = continuousPlayElapsedTimer_.nsecsElapsed() / 1000000000.0;
		
		// Calculate frames per second; this equation must match the equation in playSpeedChanged:
		double maxGenerationsPerSecond = 1000000000.0;	// bounded, to allow -eidos_pauseExecution to interrupt us
		
		if (speedSliderValue < 0.99999)
			maxGenerationsPerSecond = (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * 839;
		
		//qDebug() << "speedSliderValue == " << speedSliderValue << ", maxGenerationsPerSecond == " << maxGenerationsPerSecond;
		
		// We keep a local version of reachedSimulationEnd, because calling setReachedSimulationEnd: every generation
		// can actually be a large drag for simulations that run extremely quickly – it can actually exceed the time
		// spent running the simulation itself!  Moral of the story, KVO is wicked slow.
		bool reachedEnd = reachedSimulationEnd_;
		
		do
		{
			if (continuousPlayGenerationsCompleted_ / intervalSinceStarting >= maxGenerationsPerSecond)
				break;
			
            reachedEnd = !runSimOneGeneration();
			
			continuousPlayGenerationsCompleted_++;
		}
		while (!reachedEnd && (startTimer.nsecsElapsed() / 1000000000.0) < 0.02);
		
		setReachedSimulationEnd(reachedEnd);
		
		if (!reachedSimulationEnd_)
		{
            updateAfterTickFull((startTimer.nsecsElapsed() / 1000000000.0) > 0.04);
			continuousPlayInvocationTimer_.start(0);
		}
		else
		{
			// stop playing
			updateAfterTickFull(true);
			playOrProfile(true);    // click the Play button
			
			// bounce our icon; if we are not the active app, to signal that the run is done
			//[NSApp requestUserAttention:NSInformationalRequest];
		}
	}
}

void QtSLiMWindow::_continuousProfile(void)
{
	// NOTE this code is parallel to the code in _continuousPlay()
	if (!invalidSimulation_)
	{
        QElapsedTimer startTimer;
        startTimer.start();
		
		// We keep a local version of reachedSimulationEnd, because calling setReachedSimulationEnd: every generation
		// can actually be a large drag for simulations that run extremely quickly – it can actually exceed the time
		// spent running the simulation itself!  Moral of the story, KVO is wicked slow.
		bool reachedEnd = reachedSimulationEnd_;
		
		if (!reachedEnd)
		{
			do
			{
                reachedEnd = !runSimOneGeneration();
				
				continuousPlayGenerationsCompleted_++;
			}
            while (!reachedEnd && (startTimer.nsecsElapsed() / 1000000000.0) < 0.02);
			
            setReachedSimulationEnd(reachedEnd);
		}
		
		if (!reachedSimulationEnd_)
		{
            updateAfterTickFull((startTimer.nsecsElapsed() / 1000000000.0) > 0.04);
            continuousProfileInvocationTimer_.start(0);
		}
		else
		{
			// stop profiling
            updateAfterTickFull(true);
			playOrProfile(false);   // click the Profile button
			
			// bounce our icon; if we are not the active app, to signal that the run is done
			//[NSApp requestUserAttention:NSInformationalRequest];
		}
	}
}

void QtSLiMWindow::playOrProfile(bool isPlayAction)
{
    bool isProfileAction = !isPlayAction;	// to avoid having to think in negatives
    
#ifdef DEBUG
	if (isProfileAction)
	{
        ui->profileButton->setChecked(false);
        updateProfileButtonIcon(false);
		
        QMessageBox messageBox(this);
        messageBox.setText("Release build required");
        messageBox.setInformativeText("In order to obtain accurate timing information that is relevant to the actual runtime of a model, profiling requires that you are running a Release build of QtSLiM.");
        messageBox.setIcon(QMessageBox::Warning);
        messageBox.setWindowModality(Qt::WindowModal);
        messageBox.exec();
		
		return;
	}
#endif
    
#if (SLIMPROFILING == 0)
	if (isProfileAction)
	{
        ui->profileButton->setChecked(false);
        updateProfileButtonIcon(false);
		
        QMessageBox messageBox(this);
        messageBox.setText("Profiling disabled");
        messageBox.setInformativeText("Profiling has been disabled in this build of QtSLiM.  Please change the definition of SLIMPROFILING to 1 in the project's .pro files.");
        messageBox.setIcon(QMessageBox::Warning);
        messageBox.setWindowModality(Qt::WindowModal);
        messageBox.exec();
		
		return;
	}
#endif
    
    if (!continuousPlayOn_)
	{
        // log information needed to track our play speed
        continuousPlayElapsedTimer_.restart();
		continuousPlayGenerationsCompleted_ = 0;
        
		setContinuousPlayOn(true);
		if (isProfileAction)
            setProfilePlayOn(true);
        else
            setNonProfilePlayOn(true);
		
		// keep the button on; this works for the button itself automatically, but when the menu item is chosen this is needed
		if (isProfileAction)
		{
            ui->profileButton->setChecked(true);
            updateProfileButtonIcon(false);
            profileStartDate_ = QDateTime::currentDateTime();
		}
		else
		{
            ui->playButton->setChecked(true);
            updatePlayButtonIcon(false);
			//[self placeSubview:playButton aboveSubview:profileButton];
		}
		
		// invalidate the console symbols, and don't validate them until we are done
		if (consoleController)
            consoleController->invalidateSymbolTableAndFunctionMap();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// prepare profiling information if necessary
		if (isProfileAction)
		{
			gEidosProfilingClientCount++;
			startProfiling();
		}
#endif
		
		// start playing/profiling
		if (isPlayAction)
            continuousPlayInvocationTimer_.start(0);
		else
            continuousProfileInvocationTimer_.start(0);
	}
	else
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// close out profiling information if necessary
		if (isProfileAction && sim && !invalidSimulation_)
		{
			endProfiling();
			gEidosProfilingClientCount--;
		}
#endif
		
        // stop our recurring perform request
		if (isPlayAction)
            continuousPlayInvocationTimer_.stop();
		else
            continuousProfileInvocationTimer_.stop();
		
        setContinuousPlayOn(false);
        if (isProfileAction)
            setProfilePlayOn(false);
        else
            setNonProfilePlayOn(false);
		
		// keep the button off; this works for the button itself automatically, but when the menu item is chosen this is needed
		if (isProfileAction)
		{
            ui->profileButton->setChecked(false);
            updateProfileButtonIcon(false);
		}
		else
		{
            ui->playButton->setChecked(false);
            updatePlayButtonIcon(false);
			//[self placeSubview:profileButton aboveSubview:playButton];
		}
		
        // clean up and update UI
		if (consoleController)
            consoleController->validateSymbolTableAndFunctionMap();
        
		updateAfterTickFull(true);
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// If we just finished profiling, display a report
		if (isProfileAction && sim && !invalidSimulation_)
			displayProfileResults();
#endif
	}
}

//
//	Eidos SLiMgui method forwards
//

void QtSLiMWindow::finish_eidos_pauseExecution(void)
{
	// this gets called by performSelectorOnMainThread: after _continuousPlay: has broken out of its loop
	// if the simulation has already ended, or is invalid, or is not in continuous play, it does nothing
	if (!invalidSimulation_ && !reachedSimulationEnd_ && continuousPlayOn_ && nonProfilePlayOn_ && !profilePlayOn_ && !generationPlayOn_)
	{
		playOrProfile(true);	// this will simulate a press of the play button to stop continuous play
		
		// bounce our icon; if we are not the active app, to signal that the run is done
		//[NSApp requestUserAttention:NSInformationalRequest];
	}
}

void QtSLiMWindow::eidos_openDocument(QString __attribute__((unused)) path)
{
    // FIXME needs to be ported, including PDF display...
    //NSURL *pathURL = [NSURL fileURLWithPath:path];
	
	//[[NSDocumentController sharedDocumentController] openDocumentWithContentsOfURL:pathURL display:YES completionHandler:(^ void (NSDocument *typelessDoc, BOOL already_open, NSError *error) { })];
}

void QtSLiMWindow::eidos_pauseExecution(void)
{
    if (!invalidSimulation_ && !reachedSimulationEnd_ && continuousPlayOn_ && nonProfilePlayOn_ && !profilePlayOn_ && !generationPlayOn_)
	{
		continuousPlayGenerationsCompleted_ = UINT64_MAX - 1;			// this will break us out of the loop in _continuousPlay: at the end of this generation
        
        QMetaObject::invokeMethod(this, "finish_eidos_pauseExecution", Qt::QueuedConnection);   // this will actually stop continuous play
	}
}


//
//  change tracking and the recycle button
//

// Do our own tracking of the change count.  We do this so that we know whether the script is in
// the same state it was in when we last recycled, or has been changed.  If it has been changed,
// we add a highlight under the recycle button to suggest to the user that they might want to
// recycle to bring their changes into force.
void QtSLiMWindow::updateChangeCount(void) //:(NSDocumentChangeType)change
{
	//[super updateChangeCount:change];
	
	// Mask off flags in the high bits.  Apple is not explicit about this, but NSChangeDiscardable
	// is 256, and acts as a flag bit, so it seems reasonable to assume this for future compatibility.
//	NSDocumentChangeType maskedChange = (NSDocumentChangeType)(change & 0x00FF);
	
//	if ((maskedChange == NSChangeDone) || (maskedChange == NSChangeRedone))
		slimChangeCount++;
//	else if (maskedChange == NSChangeUndone)
//		slimChangeCount--;
	
	updateRecycleButtonIcon(false);
}

bool QtSLiMWindow::changedSinceRecycle(void)
{
	return !(slimChangeCount == 0);
}

void QtSLiMWindow::resetSLiMChangeCount(void)
{
    slimChangeCount = 0;
	
	updateRecycleButtonIcon(false);
}

// slot receiving the signal QTextEdit::textChanged() from the script textedit
void QtSLiMWindow::scriptTexteditChanged(void)
{
    // Poke the change count.  In SLiMgui we get separate notification types for changes vs. undo/redo,
    // allowing us to know when the document has returned to a checkpoint state due to undo/redo, but
    // there seems to be no way to do that with Qt, so once we register a change, only recycling will
    // bring us back to the unchanged state.
    updateChangeCount();
}


//
//  public slots
//

void QtSLiMWindow::playOneStepClicked(void)
{
    if (!invalidSimulation_)
    {
        if (consoleController)
            consoleController->invalidateSymbolTableAndFunctionMap();
        
        setReachedSimulationEnd(!runSimOneGeneration());
        
        if (consoleController)
            consoleController->validateSymbolTableAndFunctionMap();
        
        ui->generationLineEdit->clearFocus();
        updateAfterTickFull(true);
    }
}

void QtSLiMWindow::_generationPlay(void)
{
	// FIXME would be nice to have a way to stop this prematurely, if an inccorect generation is entered or whatever... BCH 2 Nov. 2017
	if (!invalidSimulation_)
	{
        QElapsedTimer startTimer;
        startTimer.start();
		
		// We keep a local version of reachedSimulationEnd, because calling setReachedSimulationEnd: every generation
		// can actually be a large drag for simulations that run extremely quickly – it can actually exceed the time
		// spent running the simulation itself!  Moral of the story, KVO is wicked slow.
        bool reachedEnd = reachedSimulationEnd_;
		
		do
		{
			if (sim->generation_ >= targetGeneration_)
				break;
			
            reachedEnd = !runSimOneGeneration();
		}
        while (!reachedEnd && (startTimer.nsecsElapsed() / 1000000000.0) < 0.02);
		
        setReachedSimulationEnd(reachedEnd);
		
		if (!reachedSimulationEnd_ && !(sim->generation_ >= targetGeneration_))
		{
            updateAfterTickFull((startTimer.nsecsElapsed() / 1000000000.0) > 0.04);
            generationPlayInvocationTimer_.start(0);
		}
		else
		{
			// stop playing
            updateAfterTickFull(true);
            generationChanged();
			
			// bounce our icon; if we are not the active app, to signal that the run is done
			//[NSApp requestUserAttention:NSInformationalRequest];
		}
	}
}

void QtSLiMWindow::generationChanged(void)
{
	if (!generationPlayOn_)
	{
		QString generationString = ui->generationLineEdit->text();
		
		// Special-case initialize(); we can never advance to it, since it is first, so we just validate it
		if (generationString == "initialize()")
		{
			if (sim->generation_ != 0)
			{
				qApp->beep();
				updateGenerationCounter();
                ui->generationLineEdit->selectAll();
			}
			
			return;
		}
		
		// Get the integer value from the textfield, since it is not "initialize()"
		targetGeneration_ = SLiMClampToGenerationType(static_cast<int64_t>(generationString.toLongLong()));
		
		// make sure the requested generation is in range
		if (sim->generation_ >= targetGeneration_)
		{
			if (sim->generation_ > targetGeneration_)
            {
                qApp->beep();
                updateGenerationCounter();
                ui->generationLineEdit->selectAll();
			}
            
			return;
		}
		
		// update UI
		//[generationProgressIndicator startAnimation:nil];
		setGenerationPlayOn(true);
		
		// invalidate the console symbols, and don't validate them until we are done
		if (consoleController)
            consoleController->invalidateSymbolTableAndFunctionMap();
		
		// get the first responder out of the generation textfield
        ui->generationLineEdit->clearFocus();
		
		// start playing
        generationPlayInvocationTimer_.start(0);
	}
	else
	{
		// stop our recurring perform request
        generationPlayInvocationTimer_.stop();
		
		setGenerationPlayOn(false);
		//[generationProgressIndicator stopAnimation:nil];
		
		if (consoleController)
            consoleController->validateSymbolTableAndFunctionMap();
		
		// Work around a bug that when the simulation ends during menu tracking, menus do not update until menu tracking finishes
		//if (reachedSimulationEnd_)
		//	forceImmediateMenuUpdate();
	}
}

void QtSLiMWindow::recycleClicked(void)
{
    // Converting a QString to a std::string is surprisingly tricky: https://stackoverflow.com/a/4644922/2752221
    std::string utf8_script_string = ui->scriptTextEdit->toPlainText().toUtf8().constData();
    
    if (consoleController)
        consoleController->invalidateSymbolTableAndFunctionMap();
    
    clearOutputClicked();
    setScriptStringAndInitializeSimulation(utf8_script_string);
    
    if (consoleController)
        consoleController->validateSymbolTableAndFunctionMap();
    
    ui->generationLineEdit->clearFocus();
    updateAfterTickFull(true);
    
    // A bit of playing with undo.  We want to break undo coalescing at the point of recycling, so that undo and redo stop
    // at the moment that we recycled.  Then we reset a change counter that we use to know if we have changed relative to
    // the recycle point, so we can highlight the recycle button to show that the executing script is out of date.
    //[scriptTextView breakUndoCoalescing];
    resetSLiMChangeCount();
    
    //[self sendAllLinkedViewsSelector:@selector(controllerRecycled)];
}

void QtSLiMWindow::playSpeedChanged(void)
{
	// We want our speed to be from the point when the slider changed, not from when play started
    continuousPlayElapsedTimer_.restart();
	continuousPlayGenerationsCompleted_ = 1;		// this prevents a new generation from executing every time the slider moves a pixel
	
	// This method is called whenever playSpeedSlider changes, continuously; we want to show the chosen speed in a tooltip-ish window
    double speedSliderValue = ui->playSpeedSlider->value() / 100.0;     // scale is 0 to 100, since only integer values are allowed by QSlider
	
	// Calculate frames per second; this equation must match the equation in _continuousPlay:
	double maxGenerationsPerSecond = static_cast<double>(INFINITY);
	
	if (speedSliderValue < 0.99999)
		maxGenerationsPerSecond = (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * 839;
	
	// Make a tooltip label string
	QString fpsString("∞ fps");
	
	if (!std::isinf(maxGenerationsPerSecond))
	{
		if (maxGenerationsPerSecond < 1.0)
			fpsString = QString::asprintf("%.2f fps", maxGenerationsPerSecond);
		else if (maxGenerationsPerSecond < 10.0)
			fpsString = QString::asprintf("%.1f fps", maxGenerationsPerSecond);
		else
			fpsString = QString::asprintf("%.0f fps", maxGenerationsPerSecond);
		
		//qDebug() << "fps string: " << fpsString;
	}
    
    // Show the tooltip; wow, that was easy...
    QPoint widgetOrigin = ui->playSpeedSlider->mapToGlobal(QPoint());
    QPoint cursorPosition = QCursor::pos();
    QPoint tooltipPosition = QPoint(cursorPosition.x() - 2, widgetOrigin.y() - ui->playSpeedSlider->rect().height() - 8);
    QToolTip::showText(tooltipPosition, fpsString, ui->playSpeedSlider, QRect(), 1000000);  // 1000 seconds; taken down on mouseup automatically
}

void QtSLiMWindow::showMutationsToggled(void)
{
    bool newValue = ui->showMutationsButton->isChecked();
    
    ui->showMutationsButton->setIcon(QIcon(newValue ? ":/buttons/show_mutations_H.png" : ":/buttons/show_mutations.png"));

    if (newValue != zoomedChromosomeShowsMutations)
	{
		zoomedChromosomeShowsMutations = newValue;
		ui->chromosomeZoomed->setShouldDrawMutations(newValue);
        ui->chromosomeZoomed->update();
	}
}

void QtSLiMWindow::showFixedSubstitutionsToggled(void)
{
    bool newValue = ui->showFixedSubstitutionsButton->isChecked();
    
    ui->showFixedSubstitutionsButton->setIcon(QIcon(newValue ? ":/buttons/show_fixed_H.png" : ":/buttons/show_fixed.png"));

    if (newValue != zoomedChromosomeShowsFixedSubstitutions)
	{
		zoomedChromosomeShowsFixedSubstitutions = newValue;
        ui->chromosomeZoomed->setShouldDrawFixedSubstitutions(newValue);
        ui->chromosomeZoomed->update();
    }
}

void QtSLiMWindow::showChromosomeMapsToggled(void)
{
    bool newValue = ui->showChromosomeMapsButton->isChecked();
    
    ui->showChromosomeMapsButton->setIcon(QIcon(newValue ? ":/buttons/show_recombination_H.png" : ":/buttons/show_recombination.png"));

    if (newValue != zoomedChromosomeShowsRateMaps)
	{
		zoomedChromosomeShowsRateMaps = newValue;
		ui->chromosomeZoomed->setShouldDrawRateMaps(newValue);
        ui->chromosomeZoomed->update();
	}
}

void QtSLiMWindow::showGenomicElementsToggled(void)
{
    bool newValue = ui->showGenomicElementsButton->isChecked();
    
    ui->showGenomicElementsButton->setIcon(QIcon(newValue ? ":/buttons/show_genomicelements_H.png" : ":/buttons/show_genomicelements.png"));

    if (newValue != zoomedChromosomeShowsGenomicElements)
	{
		zoomedChromosomeShowsGenomicElements = newValue;
		ui->chromosomeZoomed->setShouldDrawGenomicElements(newValue);
        ui->chromosomeZoomed->update();
	}
}

void QtSLiMWindow::scriptHelpClicked(void)
{
    QtSLiMHelpWindow &helpWindow = QtSLiMHelpWindow::instance();
    
    helpWindow.show();
    helpWindow.raise();
    helpWindow.activateWindow();
}

void QtSLiMWindow::showConsoleClicked(void)
{
    ui->consoleButton->setIcon(QIcon(ui->consoleButton->isChecked() ? ":/buttons/show_console_H.png" : ":/buttons/show_console.png"));

    if (!consoleController)
    {
        qApp->beep();
        return;
    }
    
    if (ui->consoleButton->isChecked())
    {
        consoleController->show();
        consoleController->raise();
        consoleController->activateWindow();
    }
    else
    {
        consoleController->hide();
    }
}

void QtSLiMWindow::showBrowserClicked(void)
{
    ui->browserButton->setIcon(QIcon(ui->browserButton->isChecked() ? ":/buttons/show_browser_H.png" : ":/buttons/show_browser.png"));

    qDebug() << "showBrowserClicked: isChecked() == " << ui->browserButton->isChecked();
}

void QtSLiMWindow::clearOutputClicked(void)
{
    ui->outputTextEdit->setPlainText("");
}

void QtSLiMWindow::dumpPopulationClicked(void)
{
    try
	{
		// dump the population
		SLIM_OUTSTREAM << "#OUT: " << sim->generation_ << " A" << std::endl;
		sim->population_.PrintAll(SLIM_OUTSTREAM, true, true, false);	// output spatial positions and ages if available, but not ancestral sequence
		
		// dump fixed substitutions also; so the dump in SLiMgui is like outputFull() + outputFixedMutations()
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "#OUT: " << sim->generation_ << " F " << std::endl;
		SLIM_OUTSTREAM << "Mutations:" << std::endl;
		
		for (unsigned int i = 0; i < sim->population_.substitutions_.size(); i++)
		{
			SLIM_OUTSTREAM << i << " ";
			sim->population_.substitutions_[i]->PrintForSLiMOutput(SLIM_OUTSTREAM);
		}
		
		// now send SLIM_OUTSTREAM to the output textview
		updateOutputTextView();
	}
	catch (...)
	{
	}
}

void QtSLiMWindow::graphPopupButtonClicked(void)
{
    qDebug() << "graphButtonClicked";
}

void QtSLiMWindow::changeDirectoryClicked(void)
{
    QFileDialog dialog(this);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setViewMode(QFileDialog::List);
    dialog.setDirectory(QString::fromUtf8(sim_working_dir.c_str()));
    
    // FIXME could use QFileDialog::open() to get a sheet instead of an app-model panel...
    if (dialog.exec())
    {
        QStringList fileNames = dialog.selectedFiles();
        
        if (fileNames.size() == 1)
        {
            sim_working_dir = fileNames[0].toUtf8().constData();
            sim_requested_working_dir = sim_working_dir;
        }
    }
}

































