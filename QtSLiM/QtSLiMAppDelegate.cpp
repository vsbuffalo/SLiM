#include "QtSLiMAppDelegate.h"
#include <QApplication>
#include <QOpenGLWidget>
#include <QSurfaceFormat>

#include "eidos_globals.h"
#include "eidos_beep.h"
#include "slim_globals.h"


static std::string Eidos_Beep_QT(std::string p_sound_name);


QtSLiMAppDelegate *qtSLiMAppDelegate = nullptr;

QtSLiMAppDelegate::QtSLiMAppDelegate(QObject *parent) : QObject(parent)
{
    // Install our custom beep handler
    Eidos_Beep = &Eidos_Beep_QT;
    
    // Let Qt know who we are, for QSettings configuration
    QCoreApplication::setOrganizationName("MesserLab");
    QCoreApplication::setOrganizationDomain("edu.MesserLab");
    QCoreApplication::setApplicationName("QtSLiM");
    QCoreApplication::setApplicationVersion(SLIM_VERSION_STRING);
    
    // Warm up our back ends before anything else happens
    Eidos_WarmUp();
    SLiM_WarmUp();
    // FIXME probably want to enable the SLiMgui class at some point
    //gEidosContextClasses.push_back(gSLiM_SLiMgui_Class);			// available only in SLiMgui
    Eidos_FinishWarmUp();

    // Remember our current working directory, to return to whenever we are not inside SLiM/Eidos
    app_cwd_ = Eidos_CurrentDirectory();

    // Set up the format for OpenGL buffers globally, so that it applies to all windows and contexts
    // This defaults to OpenGL 2.0, which is what we want, so right now we don't customize
    QSurfaceFormat format;
    //format.setDepthBufferSize(24);
    //format.setStencilBufferSize(8);
    //format.setVersion(3, 2);
    //format.setProfile(QSurfaceFormat::CompatibilityProfile);
    QSurfaceFormat::setDefaultFormat(format);
    
    // FIXME create recipes submenu once we have a document model
    // Create the Open Recipes menu
    //[self setUpRecipesMenu];

    // Connect to the app to find out when we're terminating
    QApplication *app = qApp;

    connect(app, &QApplication::lastWindowClosed, this, &QtSLiMAppDelegate::lastWindowClosed);
    connect(app, &QApplication::aboutToQuit, this, &QtSLiMAppDelegate::aboutToQuit);

    // We assume we are the global instance; FIXME singleton pattern would be good
    qtSLiMAppDelegate = this;
}


//
//  public slots
//

void QtSLiMAppDelegate::lastWindowClosed(void)
{
}

void QtSLiMAppDelegate::aboutToQuit(void)
{
}

void QtSLiMAppDelegate::showAboutWindow(void)
{
    // FIXME implement
}

void QtSLiMAppDelegate::showHelp(void)
{
    // FIXME implement
}


// This is declared in eidos_beep.h, but in QtSLiM it is actually defined here,
// so that we can produce the beep sound with Qt
std::string Eidos_Beep_QT(std::string __attribute__((__unused__)) p_sound_name)
{
    std::string return_string;
    
    qApp->beep();
    
    return return_string;
}




























