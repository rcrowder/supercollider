/************************************************************************
*
* Copyright 2010 Jakob Leben (jakob.leben@gmail.com)
*
* This file is part of SuperCollider Qt GUI.
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
*
************************************************************************/

#include "QcApplication.h"

#include <QThread>

QcApplication * QcApplication::_instance = 0;
QMutex QcApplication::_mutex;

QcApplication::QcApplication( int & argc, char ** argv )
: QApplication( argc, argv )
{
  _mutex.lock();
  _instance = this;
  _mutex.unlock();
}

void QcApplication::postSyncEvent( QcSyncEvent *e, QObject *rcv )
{
  _mutex.lock();
  if( !_instance ) {
    _mutex.unlock();
    return;
  }

  if( QThread::currentThread() == rcv->thread() ) {
    sendEvent( rcv, e );
    delete e;
  }
  else {
    QMutex mutex;
    QWaitCondition cond;

    e->_cond = &cond;
    e->_mutex = &mutex;

    mutex.lock();
    postEvent( rcv, e );
    cond.wait( &mutex );
    mutex.unlock();
  }

  _mutex.unlock();
}

void QcApplication::postSyncEvent( QcSyncEvent *e, EventHandlerFn handler )
{
  _mutex.lock();
  if( !_instance ) {
    _mutex.unlock();
    return;
  }

  if( QThread::currentThread() == _instance->thread() ) {
    (*handler)(e);
    delete(e);
  }
  else {
    QMutex mutex;
    QWaitCondition cond;

    e->_handler = handler;
    e->_cond = &cond;
    e->_mutex = &mutex;

    mutex.lock();
    postEvent( _instance, e );
    cond.wait( &mutex );
    mutex.unlock();
  }

  _mutex.unlock();
}

void QcApplication::customEvent( QEvent *e )
{
  // FIXME properly check event type
  QcSyncEvent *qce = static_cast<QcSyncEvent*>(e);
  if( qce->_handler ) {
    (*qce->_handler) ( qce );
  }
}
