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

#include "xesamsearchengine.h"

#include "storage/datastore.h"
#include "entities.h"

#include "xesaminterface.h"
#include "xesamtypes.h"

#include <QDebug>

using namespace Akonadi;

XesamSearchEngine::XesamSearchEngine(QObject * parent) :
    QObject( parent ),
    mValid( true )
{
  mInterface = new OrgFreedesktopXesamSearchInterface(
      QLatin1String("org.freedesktop.xesam.searcher"),
      QLatin1String("/org/freedesktop/xesam/searcher/main"),
      QDBusConnection::sessionBus(), this );

  if ( mInterface->isValid() ) {
    mSession = mInterface->NewSession();
    QDBusVariant result = mInterface->SetProperty( mSession, QLatin1String("search.live"), QDBusVariant(true) );
    mValid = mValid && result.variant().toBool();
    result = mInterface->SetProperty( mSession, QLatin1String("search.blocking"), QDBusVariant(false) );
    mValid = mValid && !result.variant().toBool();
    qDebug() << "XESAM session:" << mSession;

    connect( mInterface, SIGNAL(HitsAdded(QString,int)), SLOT(slotHitsAdded(QString,int)) );
    connect( mInterface, SIGNAL(HitsRemoved(QString,QList<int>)), SLOT(slotHitsRemoved(QString,QList<int>)) );
    connect( mInterface, SIGNAL(HitsModified(QString,QList<int>)), SLOT(slotHitsModified(QString,QList<int>)) );

    reloadSearches();
  } else {
    qWarning() << "XESAM interface not found!";
    mValid = false;
  }

  if ( !mValid )
    qWarning() << "No valid XESAM interface found!";
}

XesamSearchEngine::~XesamSearchEngine()
{
  stopSearches();
  if ( !mSession.isEmpty() )
    mInterface->CloseSession( mSession );
}

void XesamSearchEngine::slotHitsAdded(const QString & search, int count)
{
  qDebug() << "hits added: " << search << count;
  mMutex.lock();
  qint64 colId = mSearchMap.value( search );
  mMutex.unlock();
  if ( colId <= 0 || count <= 0 )
    return;
  qDebug() << "calling GetHits";
  QList<QList<QVariant> > results = mInterface->GetHits( search, count );
  qDebug() << "GetHits returned:" << results.count();
  typedef QList<QVariant> VariantList;
  foreach ( const VariantList &list, results ) {
    if ( list.isEmpty() )
      continue;
    qint64 itemId = uriToItemId( list.first().toString() );
    Entity::addToRelation<CollectionPimItemRelation>( colId, itemId );
  }
}

void XesamSearchEngine::slotHitsRemoved(const QString & search, const QList<int> & hits)
{
  qDebug() << "hits removed: " << search << hits;
  mMutex.lock();
  qint64 colId = mSearchMap.value( search );
  mMutex.unlock();
  if ( colId <= 0 )
    return;
  QList<QList<QVariant> > results = mInterface->GetHitData( search, hits, QStringList( QLatin1String("uri") ) );
  typedef QList<QVariant> VariantList;
  foreach ( const VariantList &list, results ) {
    if ( list.isEmpty() )
      continue;
    qint64 itemId = uriToItemId( list.first().toString() );
    Entity::removeFromRelation<CollectionPimItemRelation>( colId, itemId );
  }
}

void XesamSearchEngine::slotHitsModified(const QString & search, const QList< int > & hits)
{
  qDebug() << "hits modified: " << search << hits;
}

void XesamSearchEngine::reloadSearches()
{
  Resource res = Resource::retrieveByName( QLatin1String("akonadi_search_resource") );
  if ( !res.isValid() ) {
    qWarning() << "No valid search resource found!";
    return;
  }
  Collection::List cols = res.collections();
  foreach ( const Collection &col, cols ) {
    addSearch( col );
  }
}

void XesamSearchEngine::addSearch(const Collection & col)
{
  if ( !mInterface->isValid() || !mValid || col.queryLanguage() != QLatin1String( "XESAM" ) )
    return;
  QMutexLocker lock( &mMutex );
  if ( col.remoteId().isEmpty() )
    return;
  QString searchId = mInterface->NewSearch( mSession, col.remoteId() );
  qDebug() << "XesamSearchEngine::addSeach" << col << searchId;
  mSearchMap[ searchId ] = col.id();
  mInvSearchMap[ col.id() ] = searchId;
  mInterface->StartSearch( searchId );

#if 0
  // ### workaround until HitAdded is emitted by strigi
  lock.unlock();
  int count = mInterface->CountHits( searchId );
  slotHitsAdded( searchId, count );
#endif
}

void XesamSearchEngine::removeSearch(qint64 col)
{
  QMutexLocker lock( &mMutex );
  if ( !mInvSearchMap.contains( col ) )
    return;
  QString searchId = mInvSearchMap.value( col );
  mInvSearchMap.remove( col );
  mSearchMap.remove( searchId );
}

void XesamSearchEngine::stopSearches()
{
  Resource res = Resource::retrieveByName( QLatin1String("akonadi_search_resource") );
  if ( !res.isValid() ) {
    qWarning() << "No valid search resource found!";
    return;
  }
  Collection::List cols = res.collections();
  foreach ( const Collection &col, cols ) {
    removeSearch( col.id() );
  }
}

qint64 XesamSearchEngine::uriToItemId(const QString & uri)
{
  // TODO implement me!
  return uri.toLongLong();
}

#include "xesamsearchengine.moc"