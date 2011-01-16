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

#include "BasicWidgets.h"
#include "../QcWidgetFactory.h"
#include "../Common.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QPainter>

QcWidgetFactory<QcDefaultWidget> defaultWidgetFactory;
QcWidgetFactory<QcHLayoutWidget> hLayoutWidgetFactory;
QcWidgetFactory<QcVLayoutWidget> vLayoutWidgetFactory;
QcWidgetFactory<QLabel> labelFactory;
QcWidgetFactory<QcTextField> textFieldFactory;
QcWidgetFactory<QcCustomPainted> customPaintedFactory;

void QcDefaultLayout::setGeometry ( const QRect & r )
{
  if( !initialized ) {
    initialized = true;
    geom = r;
    return;
  }
  qcDebugMsg(2,"QcDefaultLayout::setGeometry");
  QPoint dPos = r.topLeft() - geom.topLeft();
  QSize dSize = r.size() - geom.size();//_geometry.size();
  //qcDebugMsg(2,"dSize: %i,%i", dSize.width(), dSize.height() );
  Q_FOREACH( QcLayoutItem* item, items ) {
    QRect g = item->qLayoutItem->geometry();
    int x = g.x();
    int y = g.y();
    int w = g.width();
    int h = g.height();
    //qcDebugMsg(2,"geom: %i %i %i %i", x,y,w,h);

    if( !dPos.isNull() ) {
      x += dPos.x();
      y += dPos.y();
    }

    if( !dSize.isNull() ) {
      if( item->hSizePolicy() == QtCollider::StickRight )
        x += dSize.width();
      if( item->vSizePolicy() == QtCollider::StickBottom )
        y += dSize.height();
      if( item->hSizePolicy() == QtCollider::HStretch )
        w += dSize.width();
      if( item->vSizePolicy() == QtCollider::VStretch )
        h += dSize.height();
    }
    item->qLayoutItem->setGeometry( QRect(x, y, w, h) );
  }
  geom = r;
}

void QcHLayout::setGeometry ( const QRect & geom )
{
  int varWidth = geom.width();
  int i = items.size();
  Q_FOREACH( QcLayoutItem *item, items ) {
    QRect r = item->qLayoutItem->geometry();
    if( item->hSizePolicy() != QtCollider::HStretch ) {
      varWidth -= r.width();
      i--;
      if( varWidth < 0 ) {
        varWidth = 0;
        break;
      }
    }
  }
  int partWidth = i > 0 && varWidth > 0 ? varWidth / i : 0;
  int x = 0;
  Q_FOREACH( QcLayoutItem *item, items ) {
    QRect r = item->qLayoutItem->geometry();
    r.setHeight( geom.height() );
    r.moveTo( x, geom.top() );
    if( item->hSizePolicy() == QtCollider::HStretch )
      r.setWidth( partWidth );
    x += r.width();
    item->qLayoutItem->setGeometry( r );
  }
}

void QcVLayout::setGeometry ( const QRect & geom )
{
  int varHeight = geom.height();
  int i = items.size();
  Q_FOREACH( QcLayoutItem *item, items ) {
    QRect r = item->qLayoutItem->geometry();
    if( item->vSizePolicy() != QtCollider::VStretch ) {
      varHeight -= r.height();
      i--;
      if( varHeight < 0 ) {
        varHeight = 0;
        break;
      }
    }
  }
  int partHeight = i > 0 && varHeight > 0 ? varHeight / i : 0;
  int y = 0;
  Q_FOREACH( QcLayoutItem *item, items ) {
    QRect r = item->qLayoutItem->geometry();
    r.setWidth( geom.width() );
    r.moveTo( geom.left(), y );
    if( item->vSizePolicy() == QtCollider::VStretch )
      r.setHeight( partHeight );
    y += r.height();
    item->qLayoutItem->setGeometry( r );
  }
}

//////////////////////////// QcListWidget //////////////////////////////////////

QcWidgetFactory<QcListWidget> listWidgetFactory;

QcListWidget::QcListWidget()
{
  connect( this, SIGNAL( itemClicked( QListWidgetItem* ) ),
           this, SIGNAL( action() ) );
}

void QcListWidget::setItems( const VariantList & items )
{
  clear();
  Q_FOREACH( QVariant item, items.data )
    addItem( item.toString() );
  setCurrentRow( 0 );
}

void QcListWidget::keyPressEvent( QKeyEvent *e )
{
  QListWidget::keyPressEvent( e );
  if( e->key() == Qt::Key_Return )
    Q_EMIT( returnPressed() );
}

////////////////////////// QcPopUpMenu /////////////////////////////////////////

QcWidgetFactory<QcPopUpMenu> popUpMenuFactory;

QcPopUpMenu::QcPopUpMenu()
: lastChoice( -1 )
{
  connect( this, SIGNAL(activated(int)), this, SLOT(doAction(int)) );
}

void QcPopUpMenu::setItems( const VariantList & items )
{
  clear();
  Q_FOREACH( QVariant item, items.data )
      addItem( item.toString() );
}

void QcPopUpMenu::doAction( int choice )
{
  if( choice != lastChoice ) {
    lastChoice = choice;
    Q_EMIT( action() );
  }
}

/////////////////////////////// QcButton ///////////////////////////////////////

QcWidgetFactory<QcButton> buttonFactory;

QcButton::QcButton()
: currentState(0)
{
  connect( this, SIGNAL(clicked()), this, SLOT(doAction()) );
}

void QcButton::setStates( const VariantList & statesArg )
{
  if( !statesArg.data.count() ) return;

  states.clear();

  Q_FOREACH( QVariant var, statesArg.data ) {
    VariantList stateArg = var.value<VariantList>();
    int count = stateArg.data.size();
    State state;
    if( count >= 1 )
      state.text = stateArg.data[0].toString();
    if( count >= 2 )
      state.textColor = stateArg.data[1].value<QColor>();
    if( count >= 3 )
      state.buttonColor = stateArg.data[2].value<QColor>();
    states.append( state );
  }
  setState( 0 );
}

void QcButton::setState( int i )
{
  int c = states.count();
  if( !c ) return;

  i = qMax( 0, qMin( c-1, i ) );

  currentState = i;

  State state = states[i];

  setText( state.text );

  QPalette p = palette();
  if( state.textColor.isValid() )
    p.setColor( QPalette::ButtonText, state.textColor );
  if( state.buttonColor.isValid() )
    p.setColor( QPalette::Button, state.buttonColor );
  setPalette( p );
}

void QcButton::cycleStates()
{
  if( states.size() < 2 ) return;
  int i = currentState + 1;
  if( i >= states.size() ) i = 0;
  setState( i );
}

void QcButton::doAction()
{
  cycleStates();
  Q_EMIT( action() );
}

void QcCustomPainted::doDrawFunc() {
  QApplication::sendEvent( this, new ScMethodCallEvent( "draw", QList<QVariant>(), true ) );
}
