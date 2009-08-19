/*
    Copyright (c) 2006 Volker Krause <vkrause@kde.org>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This library is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to the
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.
*/

#include "transaction.h"
#include "storage/datastore.h"

#include <QtCore/QDebug>
#include <QtCore/QThread>

Akonadi::Transaction::Transaction(DataStore * db, bool beginTransaction ) :
    mDb( db ), mCommitted( false )
{
  if ( beginTransaction ) {
    //mDb->beginTransaction();
    begin();
  }
}

Akonadi::Transaction::~Transaction()
{
  if ( !mCommitted ) {
    qDebug() << '[' << QString::number(QThread::currentThreadId()) << "] ROLLING BACK TRANSACTION";
    mDb->rollbackTransaction();
  } else {
    qDebug() << '[' << QString::number(QThread::currentThreadId()) << "] TRANSACTION SUCCESSFUL COMITTED";
  }
}

bool Akonadi::Transaction::commit()
{
  qDebug( )<< '[' << QString::number(QThread::currentThreadId()) << "] COMMITTING TRANSACTION";
  mCommitted = true;
  return mDb->commitTransaction();
}

void Akonadi::Transaction::begin()
{
  qDebug() << '[' << QString::number(QThread::currentThreadId()) << "] BEGINNING TRANSACTION";
  mDb->beginTransaction();
}
