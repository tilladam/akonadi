/*
    Copyright (c) 2007 Volker Krause <vkrause@kde.org>

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

#ifndef AKONADI_XESAMENGINE_H
#define AKONADI_XESAMENGINE_H

#include "abstractsearchengine.h"

#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QObject>

class OrgFreedesktopXesamSearchInterface;

namespace Akonadi {

class XesamSearchEngine : public QObject, public AbstractSearchEngine
{
  Q_OBJECT
  public:
    XesamSearchEngine( QObject* parent = 0 );
    ~XesamSearchEngine();

    void addSearch( const Collection &collection );
    void removeSearch( qint64 collection );

  private:
    void reloadSearches();
    void stopSearches();
    qint64 uriToItemId( const QString &uri );

  private slots:
    void slotHitsAdded( const QString &search, int count );
    void slotHitsRemoved( const QString &search, const QList<int> &hits );
    void slotHitsModified( const QString &search, const QList<int> &hits );

  private:
    OrgFreedesktopXesamSearchInterface *mInterface;
    QString mSession;
    QHash<QString,int> mSearchMap;
    QHash<int,QString> mInvSearchMap;
    QMutex mMutex;
    bool mValid;
};

}

#endif