/*
    Copyright (c) 2009 Volker Krause <vkrause@kde.org>
    Copyright (c) 2010 Milian Wolff <mail@milianw.de>

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

#include "itemretriever.h"

#include "akdebug.h"
#include "akonadiconnection.h"
#include "storage/datastore.h"
#include "storage/itemqueryhelper.h"
#include "storage/itemretrievalmanager.h"
#include "storage/itemretrievalrequest.h"
#include "storage/parthelper.h"
#include "storage/parttypehelper.h"
#include "storage/querybuilder.h"
#include "storage/selectquerybuilder.h"
#include "utils.h"

#include <QDebug>

using namespace Akonadi;

ItemRetriever::ItemRetriever( AkonadiConnection *connection ) :
  mScope( Scope::Invalid ),
  mConnection( connection ),
  mFullPayload( false ),
  mRecursive( false )
{ }

ItemRetriever::~ItemRetriever()
{ }

AkonadiConnection *ItemRetriever::connection() const
{
  return mConnection;
}

void ItemRetriever::setRetrieveParts(const QStringList& parts)
{
  mParts = parts;
  // HACK, we need a full payload available flag in PimItem
  if ( mFullPayload && !mParts.contains( QLatin1String( "PLD:RFC822" ) ) )
    mParts.append( QLatin1String( "PLD:RFC822" ) );
}

void ItemRetriever::setItemSet(const ImapSet& set, const Collection &collection )
{
  mItemSet = set;
  mCollection = collection;
}

void ItemRetriever::setItemSet(const ImapSet& set, bool isUid)
{
  Q_ASSERT( mConnection );
  if ( !isUid && mConnection->selectedCollectionId() >= 0 )
    setItemSet( set, mConnection->selectedCollection() );
  else
    setItemSet( set );
}

void ItemRetriever::setItem( const Akonadi::Entity::Id& id )
{
  ImapSet set;
  set.add( ImapInterval( id, id ) );
  mItemSet = set;
  mCollection = Collection();
}

void ItemRetriever::setRetrieveFullPayload(bool fullPayload)
{
  mFullPayload = fullPayload;
  // HACK, we need a full payload available flag in PimItem
  if ( fullPayload && !mParts.contains( QLatin1String( "PLD:RFC822" ) ) )
    mParts.append( QLatin1String( "PLD:RFC822" ) );
}

void ItemRetriever::setCollection(const Collection& collection, bool recursive)
{
  mCollection = collection;
  mItemSet = ImapSet();
  mRecursive = recursive;
}

void ItemRetriever::setScope(const Scope& scope)
{
  mScope = scope;
}

Scope ItemRetriever::scope() const
{
  return mScope;
}

QStringList ItemRetriever::retrieveParts() const
{
  return mParts;
}

enum QueryColumns {
  PimItemIdColumn,
  PimItemRidColumn,

  MimeTypeColumn,

  ResourceColumn,

  PartTypeNameColumn,
  PartDatasizeColumn
};

QSqlQuery ItemRetriever::buildQuery() const
{
  QueryBuilder qb( PimItem::tableName() );

  qb.addJoin( QueryBuilder::InnerJoin, MimeType::tableName(), PimItem::mimeTypeIdFullColumnName(), MimeType::idFullColumnName() );

  qb.addJoin( QueryBuilder::InnerJoin, Collection::tableName(), PimItem::collectionIdFullColumnName(), Collection::idFullColumnName() );

  qb.addJoin( QueryBuilder::InnerJoin, Resource::tableName(), Collection::resourceIdFullColumnName(), Resource::idFullColumnName() );

  Query::Condition partJoinCondition;
  partJoinCondition.addColumnCondition(PimItem::idFullColumnName(), Query::Equals, Part::pimItemIdFullColumnName());
  if ( !mFullPayload && !mParts.isEmpty() ) {
    partJoinCondition.addCondition( PartTypeHelper::conditionFromFqNames( mParts ) );
  }
  partJoinCondition.addValueCondition( PartType::nsFullColumnName(), Query::Equals, QLatin1String( "PLD" ) );
  qb.addJoin( QueryBuilder::LeftJoin, Part::tableName(), partJoinCondition );

  qb.addJoin( QueryBuilder::InnerJoin, PartType::tableName(), Part::partTypeIdFullColumnName(), PartType::idFullColumnName() );

  qb.addColumn( PimItem::idFullColumnName() );
  qb.addColumn( PimItem::remoteIdFullColumnName() );
  qb.addColumn( MimeType::nameFullColumnName() );
  qb.addColumn( Resource::nameFullColumnName() );
  qb.addColumn( PartType::nameFullColumnName() );
  qb.addColumn( Part::datasizeFullColumnName() );

  if ( mScope.scope() != Scope::Invalid )
    ItemQueryHelper::scopeToQuery( mScope, mConnection, qb );
  else
    ItemQueryHelper::itemSetToQuery( mItemSet, qb, mCollection );

  // prevent a resource to trigger item retrieval from itself
  if ( mConnection ) {
    qb.addValueCondition( Resource::nameFullColumnName(), Query::NotEquals,
                          QString::fromLatin1( mConnection->sessionId() ) );
  }

  qb.addSortColumn( PimItem::idFullColumnName(), Query::Ascending );

  if ( !qb.exec() ) {
    mLastError = "Unable to retrieve items";
    throw ItemRetrieverException( mLastError );
  }

  qb.query().next();

  return qb.query();
}


bool ItemRetriever::exec()
{
  if ( mParts.isEmpty() && !mFullPayload )
    return true;

  verifyCache();

  QSqlQuery query = buildQuery();
  ItemRetrievalRequest* lastRequest = 0;
  QList<ItemRetrievalRequest*> requests;

  QStringList parts;
  Q_FOREACH ( const QString &part, mParts ) {
    if ( part.startsWith( QLatin1String( "PLD:" ) ) ) {
      parts << part.mid(4);
    }
  }

  while ( query.isValid() ) {
    const qint64 pimItemId = query.value( PimItemIdColumn ).toLongLong();
    if ( !lastRequest || lastRequest->id != pimItemId ) {
      lastRequest = new ItemRetrievalRequest();
      lastRequest->id = pimItemId;
      lastRequest->remoteId = Utils::variantToByteArray( query.value( PimItemRidColumn ) );
      lastRequest->mimeType = Utils::variantToByteArray( query.value( MimeTypeColumn ) );
      lastRequest->resourceId = Utils::variantToString( query.value( ResourceColumn ) );
      lastRequest->parts = parts;
      requests << lastRequest;
    }

    if ( query.value( PartTypeNameColumn ).isNull() ) {
      // LEFT JOIN did not find anything, retrieve all parts
      query.next();
      continue;
    }

    qint64 datasize = query.value( PartDatasizeColumn ).toLongLong();
    const QString partName = Utils::variantToString( query.value( PartTypeNameColumn ) );
    Q_ASSERT( !partName.startsWith( QLatin1String( "PLD:" ) ) );
    if ( datasize <= 0 ) {
      // request update for this part
      if ( mFullPayload && !lastRequest->parts.contains( partName ) )
        lastRequest->parts << partName;
    } else {
      // data available, don't request update
      lastRequest->parts.removeAll( partName );
    }
    query.next();
  }

  //akDebug() << "Closing queries and sending out requests.";

  query.finish();

  Q_FOREACH ( ItemRetrievalRequest* request, requests ) {
    if ( request->parts.isEmpty() ) {
        delete request;
        continue;
    }
    // TODO: how should we handle retrieval errors here? so far they have been ignored,
    // which makes sense in some cases, do we need a command parameter for this?
    try {
      ItemRetrievalManager::instance()->requestItemDelivery( request );
    } catch ( const ItemRetrieverException &e ) {
      akError() << e.type() << ": " << e.what();
      mLastError = e.what();
      return false;
    }
  }

  // retrieve items in child collections if requested
  bool result = true;
  if ( mRecursive && mCollection.isValid() ) {
    Q_FOREACH ( const Collection &col, mCollection.children() ) {
      ItemRetriever retriever( mConnection );
      retriever.setCollection( col, mRecursive );
      retriever.setRetrieveParts( mParts );
      retriever.setRetrieveFullPayload( mFullPayload );
      result = retriever.exec();
      if (!result) break;
    }
  }

  return result;
}

void ItemRetriever::verifyCache()
{
  if (!connection()->verifyCacheOnRetrieval())
    return;

  SelectQueryBuilder<Part> qb;
  qb.addJoin( QueryBuilder::InnerJoin, PimItem::tableName(), Part::pimItemIdFullColumnName(), PimItem::idFullColumnName() );
  qb.addValueCondition( Part::externalFullColumnName(), Query::Equals, true );
  qb.addValueCondition( Part::dataFullColumnName(), Query::IsNot, QVariant() );
  if ( mScope.scope() != Scope::Invalid )
    ItemQueryHelper::scopeToQuery( mScope, mConnection, qb );
  else
    ItemQueryHelper::itemSetToQuery( mItemSet, qb, mCollection );

  if (!qb.exec()) {
    mLastError = "Unable to query parts.";
    throw ItemRetrieverException( mLastError );
  }

  const Part::List externalParts = qb.result();
  Q_FOREACH ( Part part, externalParts )
    PartHelper::verify( part );
}

QByteArray ItemRetriever::lastError() const
{
  return mLastError;
}
