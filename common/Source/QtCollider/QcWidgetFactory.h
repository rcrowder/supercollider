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

#ifndef QC_WIDGET_FACTORY_H
#define QC_WIDGET_FACTORY_H

#include "QcObjectFactory.h"
#include "Common.h"
#include "QWidgetProxy.h"
#include "widgets/QcScrollArea.h"

#include <QLayout>

template <class QWIDGET>
class QcWidgetFactory : public QcObjectFactory<QWIDGET>
{
public:

  virtual QObjectProxy *newInstance( PyrObject *scObject, QList<QVariant> & arguments ) {

    QWIDGET *widget = new QWIDGET();
    QWidget *w = widget; // template parameter type-safety

    // set requested geometry

    int argc = arguments.count();

    QRect r;
    if( argc > 1 ) r = arguments[1].value<QRect>();
    if( r.size().isEmpty() ) r.setSize( w->sizeHint() );

    w->setGeometry( r );

    // do custom initialization

    initialize( widget, arguments ); // use template parameter type

    // set parent

    QObjectProxy *parentProxy = argc > 0 ? arguments[0].value<QObjectProxy*>() : 0;

    if( parentProxy ) {

      QWidget *parentWidget = qobject_cast<QWidget*>( parentProxy->object() );
      if( parentWidget ) {

        // Ok, we have the parent, so stuff the child in
        const QMetaObject *mo = parentWidget->metaObject();
        bool ok = mo->invokeMethod( parentWidget, "addChild", Q_ARG( QWidget*, w ) );

        if( !ok ) w->setParent( parentWidget );

        w->show();
      }
    }

    return new QWidgetProxy ( w, scObject );
  }
};

#endif
