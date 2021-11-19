/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "mainframe.h"

#include "toolbars/titlebar.h"
#include "dialogs/dsmessagebox.h"
#include "dialogs/dsdialog.h"
#include "mainwindow.h"

#include <QVBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QHoverEvent>
#include <QPixmap>
#include <QPainter>
#include <QLabel>
#include <QDialogButtonBox>
#include <QBitmap>
#include <QResizeEvent>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QPushButton>
#include <QMessageBox> 
#include <QScreen>
#include <QApplication>
#include <QDebug>

#include "dsvdef.h"
#include "config/appconfig.h"
#include "../ui/msgbox.h"

#include <algorithm>

namespace pv {

MainFrame::MainFrame()
{
    _layout = NULL;
    _bDraging = false;
    _hit_border = None;
    _freezing = false; 
    _titleBar = NULL;
    _mainWindow = NULL;

    setAttribute(Qt::WA_TranslucentBackground);
    // Make this a borderless window which can't
    // be resized or moved via the window system
    #ifdef _WIN32
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint);
    #else
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    #endif

    setMinimumHeight(minHeight);
    setMinimumWidth(minWidth);
  
    // Set the window icon
    QIcon icon;
    icon.addFile(QString::fromUtf8(":/icons/logo.svg"), QSize(), QIcon::Normal, QIcon::Off);
    setWindowIcon(icon);

    app::get_app_window_instance(this, true);
  
    // Title
    _titleBar = new toolbars::TitleBar(true, this);
     
    // MainWindow
    _mainWindow = new MainWindow(this);
    _mainWindow->setWindowFlags(Qt::Widget);
    _titleBar->setTitle(_mainWindow->windowTitle());

    QVBoxLayout *vbox = new QVBoxLayout();
    vbox->setContentsMargins(0,0,0,0);
    vbox->setSpacing(0);
    vbox->addWidget(_titleBar);
    vbox->addWidget(_mainWindow);

    _top_left = new widgets::Border (TopLeft, this);
    _top_left->setFixedSize(Margin, Margin);
    _top_left->installEventFilter(this);
    _top = new widgets::Border (Top, this);
    _top->setFixedHeight(Margin);
    _top->installEventFilter(this);
    _top_right = new widgets::Border (TopRight, this);
    _top_right->setFixedSize(Margin, Margin);
    _top_right->installEventFilter(this);

    _left = new widgets::Border (Left, this);
    _left->setFixedWidth(Margin);
    _left->installEventFilter(this);
    _right = new widgets::Border (Right, this);
    _right->setFixedWidth(Margin);
    _right->installEventFilter(this);

    _bottom_left = new widgets::Border (BottomLeft, this);
    _bottom_left->setFixedSize(Margin, Margin);
    _bottom_left->installEventFilter(this);
    _bottom = new widgets::Border (Bottom, this);
    _bottom->setFixedHeight(Margin);
    _bottom->installEventFilter(this);
    _bottom_right = new widgets::Border (BottomRight, this);
    _bottom_right->setFixedSize(Margin, Margin);
    _bottom_right->installEventFilter(this);

    _layout = new QGridLayout(this);
    _layout->setSpacing(0);
    _layout->setContentsMargins(0,0,0,0);
    _layout->addWidget(_top_left, 0, 0);
    _layout->addWidget(_top, 0, 1);
    _layout->addWidget(_top_right, 0, 2);
    _layout->addWidget(_left, 1, 0);
    _layout->addLayout(vbox, 1, 1);
    _layout->addWidget(_right, 1, 2);
    _layout->addWidget(_bottom_left, 2, 0);
    _layout->addWidget(_bottom, 2, 1);
    _layout->addWidget(_bottom_right, 2, 2);

    connect(&_timer, SIGNAL(timeout()), this, SLOT(unfreezing()));  
}
 
void MainFrame::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);

    if (_layout == NULL){
        return;
    }

    if (isMaximized()) {
        hide_border();
    } else {
        show_border();
    }
    _titleBar->setRestoreButton(isMaximized());
    _layout->update();
}

void MainFrame::closeEvent(QCloseEvent *event)
{
    writeSettings();
    _mainWindow->session_save();
    event->accept();
}

void MainFrame::unfreezing()
{
    _freezing = false;
}

void MainFrame::hide_border()
{
    _top_left->setVisible(false);
    _top_right->setVisible(false);
    _top->setVisible(false);
    _left->setVisible(false);
    _right->setVisible(false);
    _bottom_left->setVisible(false);
    _bottom->setVisible(false);
    _bottom_right->setVisible(false);
}

void MainFrame::show_border()
{ 
    _top_left->setVisible(true);
    _top_right->setVisible(true);
    _top->setVisible(true);
    _left->setVisible(true);
    _right->setVisible(true);
    _bottom_left->setVisible(true);
    _bottom->setVisible(true);
    _bottom_right->setVisible(true);
}

void MainFrame::showNormal()
{
    show_border();  
    QFrame::showNormal();
}

void MainFrame::showMaximized()
{ 
    hide_border();
    QFrame::showMaximized();
}

void MainFrame::showMinimized()
{ 
    writeSettings();
    QFrame::showMinimized();
}

bool MainFrame::eventFilter(QObject *object, QEvent *event)
{
    const QEvent::Type type = event->type();
    const QMouseEvent *const mouse_event = (QMouseEvent*)event;
    int newWidth;
    int newHeight;
    int newLeft;
    int newTop;

    if (type != QEvent::MouseMove 
        && type != QEvent::MouseButtonPress 
        && type != QEvent::MouseButtonRelease
        && type != QEvent::Leave){
        return QFrame::eventFilter(object, event);
    }

    //when window is maximized, or is moving, call return 
    if (isMaximized() || _titleBar->IsMoving()){
       return QFrame::eventFilter(object, event);
    }

    if (!_bDraging && type == QEvent::MouseMove && (!(mouse_event->buttons() || Qt::NoButton))){
           if (object == _top_left) {
                _hit_border = TopLeft;
                setCursor(Qt::SizeFDiagCursor);
            } else if (object == _bottom_right) {
                _hit_border = BottomRight;
                setCursor(Qt::SizeFDiagCursor);
            } else if (object == _top_right) {
                _hit_border = TopRight;
                setCursor(Qt::SizeBDiagCursor);
            } else if (object == _bottom_left) {
                _hit_border = BottomLeft;
                setCursor(Qt::SizeBDiagCursor);
            } else if (object == _left) {
                _hit_border = Left;
                setCursor(Qt::SizeHorCursor);
            } else if (object == _right) {
                _hit_border = Right;
                setCursor(Qt::SizeHorCursor);
            } else if (object == _bottom) {
                _hit_border = Bottom;
                setCursor(Qt::SizeVerCursor);
            } else if (object == _top) {
                _hit_border = Top;
                setCursor(Qt::SizeVerCursor);
            } else {
                _hit_border = None;
                setCursor(Qt::ArrowCursor);
            }

            return QFrame::eventFilter(object, event);
    }

  if (type == QEvent::MouseMove) {
         if(mouse_event->buttons().testFlag(Qt::LeftButton)) {
            if (!_freezing) {
                switch (_hit_border) {
                case TopLeft:
                    newWidth = std::max(_dragStartGeometry.right() - mouse_event->globalX(), minimumWidth());
                    newHeight = std::max(_dragStartGeometry.bottom() - mouse_event->globalY(), minimumHeight());
                    newLeft = geometry().left();
                    newTop = geometry().top();
                    if (newWidth > minimumWidth())
                        newLeft = mouse_event->globalX();
                    if (newHeight > minimumHeight())
                        newTop = mouse_event->globalY();
                    setGeometry(newLeft, newTop, newWidth, newHeight);
                    saveWindowRegion();                    
                   break;

                case BottomLeft:
                    newWidth = std::max(_dragStartGeometry.right() - mouse_event->globalX(), minimumWidth());
                    newHeight = std::max(mouse_event->globalY() - _dragStartGeometry.top(), minimumHeight());
                    newLeft = geometry().left();
                    if (newWidth > minimumWidth())
                        newLeft = mouse_event->globalX();
                    setGeometry(newLeft, _dragStartGeometry.top(), newWidth, newHeight);
                    saveWindowRegion();
                   break;

                case TopRight:
                    newWidth = std::max(mouse_event->globalX() - _dragStartGeometry.left(), minimumWidth());
                    newHeight = std::max(_dragStartGeometry.bottom() - mouse_event->globalY(), minimumHeight());
                    newTop = geometry().top();
                    if (newHeight > minimumHeight())
                        newTop = mouse_event->globalY();
                    setGeometry(_dragStartGeometry.left(), newTop, newWidth, newHeight);
                    saveWindowRegion();
                   break;

                case BottomRight:
                    newWidth = std::max(mouse_event->globalX() - _dragStartGeometry.left(), minimumWidth());
                    newHeight = std::max(mouse_event->globalY() - _dragStartGeometry.top(), minimumHeight());
                    setGeometry(_dragStartGeometry.left(), _dragStartGeometry.top(), newWidth, newHeight);
                    saveWindowRegion();
                   break;

                case Left:
                    newWidth = _dragStartGeometry.right() - mouse_event->globalX();
                    if (newWidth > minimumWidth()){
                         setGeometry(mouse_event->globalX(), _dragStartGeometry.top(), newWidth, height());
                         saveWindowRegion();
                    }                       
                   break;

                case Right:
                    newWidth = mouse_event->globalX() - _dragStartGeometry.left();
                    if (newWidth > minimumWidth()){
                         setGeometry(_dragStartGeometry.left(), _dragStartGeometry.top(), newWidth, height());
                         saveWindowRegion();
                    }                       
                   break;

                case Top:
                    newHeight = _dragStartGeometry.bottom() - mouse_event->globalY();
                    if (newHeight > minimumHeight()){
                        setGeometry(_dragStartGeometry.left(), mouse_event->globalY(),width(), newHeight);
                        saveWindowRegion();
                    }                        
                   break;

                case Bottom:
                    newHeight = mouse_event->globalY() - _dragStartGeometry.top();
                    if (newHeight > minimumHeight()){
                        setGeometry(_dragStartGeometry.left(), _dragStartGeometry.top(), width(), newHeight);
                        saveWindowRegion();
                    }                       
                   break;

                default:
                   break;
                }
                _freezing = true;
            } 
            return true;
        }
    }
     else if (type == QEvent::MouseButtonPress) {
        if (mouse_event->button() == Qt::LeftButton) 
        if (_hit_border != None)
            _bDraging = true;
        _timer.start(50);
        _dragStartGeometry = geometry();
    } 
    else if (type == QEvent::MouseButtonRelease) {
        if (mouse_event->button() == Qt::LeftButton) {
         
            _bDraging = false;
            _timer.stop();
        }
    } else if (!_bDraging && type == QEvent::Leave) {
        _hit_border = None;
        setCursor(Qt::ArrowCursor);
    } 
    
 
    return QFrame::eventFilter(object, event);
}

 void MainFrame::saveWindowRegion()
 {
     AppConfig &app = AppConfig::Instance();    
     QRect rc = geometry();
     app._frameOptions.left = rc.left();
     app._frameOptions.top = rc.top();
     app._frameOptions.right = rc.right();
     app._frameOptions.bottom = rc.bottom();
 }

void MainFrame::writeSettings()
{  
    AppConfig &app = AppConfig::Instance();
    app._frameOptions.isMax = isMaximized(); 

    if (!isMaximized()){
          saveWindowRegion();
    }
  
    app.SaveFrame(); 
}

void MainFrame::readSettings()
{
    if (_layout == NULL)
        return;

    AppConfig &app = AppConfig::Instance(); 
   
    if (app._frameOptions.language > 0){
         _mainWindow->switchLanguage(app._frameOptions.language);
    }   

    if (app._frameOptions.right == 0) {
        QScreen *screen=QGuiApplication::primaryScreen ();
        const QRect availableGeometry = screen->availableGeometry();
        resize(availableGeometry.width() / 2, availableGeometry.height() / 1.5);
        const int origX = std::max(0, (availableGeometry.width() - width()) / 2);
        const int origY = std::max(0, (availableGeometry.height() - height()) / 2);
        move(origX, origY);

    } else {
         if (app._frameOptions.isMax){
            showMaximized(); //show max by system api
         }
         else{
            int left = app._frameOptions.left;
            int top = app._frameOptions.top;
            int right = app._frameOptions.right;
            int bottom = app._frameOptions.bottom;
            resize(right-left, bottom-top);
            move(left, top);         
         }
    }

    // restore dockwidgets
    _mainWindow->restore_dock();
    _titleBar->setRestoreButton(app._frameOptions.isMax);
}

void MainFrame::setTaskbarProgress(int progress)
{
    (void)progress;
}

void MainFrame::show_doc()
{
     AppConfig &app = AppConfig::Instance(); 
     int lan = app._frameOptions.language;
      
    if (app._userHistory.showDocuments) {
        dialogs::DSDialog dlg(this, true);
        dlg.setTitle(tr("Document"));

        QLabel tipsLabel;
        tipsLabel.setPixmap(QPixmap(":/icons/showDoc"+QString::number(lan)+".png"));
        QMessageBox msg;
        msg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
        msg.setContentsMargins(0, 0, 0, 0);
        connect(&msg, SIGNAL(buttonClicked(QAbstractButton*)), &dlg, SLOT(accept()));
        QPushButton *noMoreButton = msg.addButton(tr("Not Show Again"), QMessageBox::ActionRole);
        msg.addButton(tr("Ignore"), QMessageBox::ActionRole);
        QPushButton *openButton = msg.addButton(tr("Open"), QMessageBox::ActionRole);

        QVBoxLayout layout;
        layout.addWidget(&tipsLabel);
        layout.addWidget(&msg, 0, Qt::AlignRight);
        layout.setContentsMargins(0, 0, 0, 0);

        dlg.layout()->addLayout(&layout);
        dlg.exec();

        if (msg.clickedButton() == openButton) {
            _mainWindow->openDoc();
        }
        if (msg.clickedButton() == noMoreButton){
            app._userHistory.showDocuments = false;
            app.SaveHistory();
        }   
    }
}

} // namespace pv
