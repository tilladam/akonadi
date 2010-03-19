/*
    Copyright (c) 2009 Volker Krause <vkrause@kde.org>

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

#include "itemretrievalmanager.h"
#include "itemretrievalrequest.h"
#include "itemretrievaljob.h"

#include "resourceinterface.h"
#include "akdebug.h"

#include <QCoreApplication>
#include <QReadWriteLock>
#include <QWaitCondition>
#include <QDBusConnection>
#include <QDBusConnectionInterface>

using namespace Akonadi;

ItemRetrievalManager* ItemRetrievalManager::sInstance = 0;

ItemRetrievalManager::ItemRetrievalManager( QObject *parent ) :
  QObject( parent )
{
  // make sure we are created from the retrieval thread and only once
  Q_ASSERT( QThread::currentThread() != QCoreApplication::instance()->thread() );
  Q_ASSERT( sInstance == 0 );
  sInstance = this;

  mLock = new QReadWriteLock();
  mWaitCondition = new QWaitCondition();

  connect( QDBusConnection::sessionBus().interface(), SIGNAL(serviceOwnerChanged(QString,QString,QString)),
           this, SLOT(serviceOwnerChanged(QString,QString,QString)) );
  connect( this, SIGNAL(requestAdded()), this, SLOT(processRequest()), Qt::QueuedConnection );
  connect( this, SIGNAL(syncCollection(QString,qint64)), this, SLOT(triggerCollectionSync(QString,qint64)), Qt::QueuedConnection );
  connect( this, SIGNAL( syncResource( const QString& ) ),
           this, SLOT( triggerResourceSync( const QString& ) ), Qt::QueuedConnection );
}

ItemRetrievalManager::~ItemRetrievalManager()
{
  delete mWaitCondition;
  delete mLock;
}

ItemRetrievalManager* ItemRetrievalManager::instance()
{
  Q_ASSERT( sInstance );
  return sInstance;
}

// called within the retrieval thread
void ItemRetrievalManager::serviceOwnerChanged(const QString& serviceName, const QString& oldOwner, const QString& newOwner)
{
  Q_UNUSED( newOwner );
  if ( oldOwner.isEmpty() )
    return;
  if ( !serviceName.startsWith( QLatin1String("org.freedesktop.Akonadi.Resource.") ) )
    return;
  const QString resourceId = serviceName.mid( 33 );
  qDebug() << "Lost connection to resource" << serviceName << ", discarding cached interface";
  mResourceInterfaces.remove( resourceId );
}

// called within the retrieval thread
OrgFreedesktopAkonadiResourceInterface* ItemRetrievalManager::resourceInterface(const QString& id)
{
  if ( id.isEmpty() )
    return 0;

  OrgFreedesktopAkonadiResourceInterface *iface = 0;
  if ( mResourceInterfaces.contains( id ) )
    iface = mResourceInterfaces.value( id );
  if ( iface && iface->isValid() )
    return iface;

  delete iface;
  iface = new OrgFreedesktopAkonadiResourceInterface( QLatin1String("org.freedesktop.Akonadi.Resource.") + id,
                                                      QLatin1String("/"), QDBusConnection::sessionBus(), this );
  if ( !iface || !iface->isValid() ) {
    qDebug() << QString::fromLatin1( "Cannot connect to agent instance with identifier '%1', error message: '%2'" )
                                    .arg( id, iface ? iface->lastError().message() : QString() );
    delete iface;
    return 0;
  }
  mResourceInterfaces.insert( id, iface );
  return iface;
}

// called from any thread
void ItemRetrievalManager::requestItemDelivery( qint64 uid, const QByteArray& remoteId, const QByteArray& mimeType,
                                               const QString& resource, const QStringList& parts )
{
  ItemRetrievalRequest *req = new ItemRetrievalRequest();
  req->id = uid;
  req->remoteId = remoteId;
  req->mimeType = mimeType;
  req->resourceId = resource;
  req->parts = parts;

  requestItemDelivery( req );
}

void ItemRetrievalManager::requestItemDelivery( ItemRetrievalRequest *req )
{
  mLock->lockForWrite();
  qDebug() << "posting retrieval request for item" << req->id << " there are " << mPendingRequests.size() << " queues and " << mPendingRequests[ req->resourceId ].size() << " items in mine";
  mPendingRequests[ req->resourceId ].append( req );
  mLock->unlock();

  emit requestAdded();

  mLock->lockForRead();
  forever {
    qDebug() << "checking if request for item" << req->id << "has been processed...";
    if ( req->processed ) {
      Q_ASSERT( !mPendingRequests[ req->resourceId ].contains( req ) );
      const QString errorMsg = req->errorMsg;
      mLock->unlock();
      qDebug() << "request for item" << req->id << "processed, error:" << errorMsg;
      delete req;
      if ( errorMsg.isEmpty() )
        return;
      else
        throw ItemRetrieverException( errorMsg );
    } else {
      qDebug() << "request for item" << req->id << "still pending - waiting";
      mWaitCondition->wait( mLock );
      qDebug() << "continuing";
    }
  }

  throw ItemRetrieverException( "WTF?" );
}

// called within the retrieval thread
void ItemRetrievalManager::processRequest()
{
  QList<QPair<ItemRetrievalJob*, QString> > newJobs;

  mLock->lockForWrite();
  // look for idle resources
  for ( QHash< QString, QList< ItemRetrievalRequest* > >::iterator it = mPendingRequests.begin(); it != mPendingRequests.end(); ) {
    if ( it.value().isEmpty() ) {
      it = mPendingRequests.erase( it );
      continue;
    }
    if ( !mCurrentJobs.contains( it.key() ) || mCurrentJobs.value( it.key() ) == 0 ) {
      // TODO: check if there is another one for the same uid with more parts requested
      ItemRetrievalRequest* req = it.value().takeFirst();
      Q_ASSERT( req->resourceId == it.key() );
      ItemRetrievalJob *job = new ItemRetrievalJob( req, this );
      connect( job, SIGNAL(requestCompleted(ItemRetrievalRequest*,QString)), SLOT(retrievalJobFinished(ItemRetrievalRequest*,QString)) );
      mCurrentJobs.insert( req->resourceId, job );
      // delay job execution until after we unlocked the mutex, since the job can emit the finished signal immediately in some cases
      newJobs.append( qMakePair( job, req->resourceId ) );
    }
    ++it;
  }

  bool nothingGoingOn = mPendingRequests.isEmpty() && mCurrentJobs.isEmpty() && newJobs.isEmpty();
  mLock->unlock();

  if ( nothingGoingOn ) { // someone asked as to process requests although everything is done already, he might still be waiting
    mWaitCondition->wakeAll();
    return;
  }

  for ( QList< QPair< ItemRetrievalJob*, QString > >::const_iterator it = newJobs.constBegin(); it != newJobs.constEnd(); ++it )
    (*it).first->start( resourceInterface( (*it).second ) );
}

void ItemRetrievalManager::retrievalJobFinished(ItemRetrievalRequest* request, const QString& errorMsg)
{
  mLock->lockForWrite();
  request->errorMsg = errorMsg;
  request->processed = true;
  Q_ASSERT( mCurrentJobs.contains( request->resourceId ) );
  mCurrentJobs.remove( request->resourceId );
  // TODO check if (*it)->parts is a subset of currentRequest->parts
  for ( QList<ItemRetrievalRequest*>::Iterator it = mPendingRequests[ request->resourceId ].begin(); it != mPendingRequests[ request->resourceId ].end(); ) {
    if ( (*it)->id == request->id ) {
      qDebug() << "someone else requested item" << request->id << "as well, marking as processed";
      (*it)->errorMsg = errorMsg;
      (*it)->processed = true;
      it = mPendingRequests[ request->resourceId ].erase( it );
    } else {
      ++it;
    }
  }
  mWaitCondition->wakeAll();
  mLock->unlock();
  emit requestAdded(); // trigger processRequest() again, in case there is more in the queues
}

void ItemRetrievalManager::requestCollectionSync( const Collection& collection )
{
  // if the collection is a resource collection we trigger a synchronization
  // of the collection hierarchy as well
  if ( collection.parentId() == 0 )
    emit syncResource( collection.resource().name() );

  emit syncCollection( collection.resource().name(), collection.id() );
}

void ItemRetrievalManager::triggerCollectionSync(const QString& resource, qint64 colId)
{
  OrgFreedesktopAkonadiResourceInterface *interface = resourceInterface( resource );
  if ( interface )
    interface->synchronizeCollection( colId );
}

void ItemRetrievalManager::triggerResourceSync( const QString& resource )
{
  OrgFreedesktopAkonadiResourceInterface *interface = resourceInterface( resource );
  if ( interface )
    interface->synchronize();
}

#include "itemretrievalmanager.moc"
