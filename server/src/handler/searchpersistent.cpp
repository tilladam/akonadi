/***************************************************************************
 *   Copyright (C) 2006 by Tobias Koenig <tokoe@kde.org>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "searchpersistent.h"

#include "akonadi.h"
#include "akonadiconnection.h"
#include "response.h"
#include "storage/datastore.h"
#include "storage/entity.h"
#include "storage/transaction.h"
#include "handlerhelper.h"
#include "abstractsearchmanager.h"
#include "imapstreamparser.h"

#include <QtCore/QStringList>

using namespace Akonadi;

SearchPersistent::SearchPersistent()
  : Handler()
{
}

SearchPersistent::~SearchPersistent()
{
}


bool SearchPersistent::parseStream()
{
  QByteArray collectionName = m_streamParser->readString();
  if ( collectionName.isEmpty() )
    return failureResponse( "No name specified" );

  DataStore *db = connection()->storageBackend();
  Transaction transaction( db );

  QByteArray queryString = m_streamParser->readString();
  if ( queryString.isEmpty() )
    return failureResponse( "No query specified" );

  Collection col;
  col.setRemoteId( QString::fromUtf8( queryString ) );
  col.setParentId( 1 ); // search root
  col.setResourceId( 1 ); // search resource
  col.setName( collectionName );
  if ( !db->appendCollection( col ) )
    return failureResponse( "Unable to create persistent search" );

  // work around the fact that we have no clue what might be in there
  MimeType::List mts = MimeType::retrieveAll();
  foreach ( const MimeType &mt, mts ) {
    if ( mt.name() == QLatin1String( "inode/directory" ) )
      continue;
    col.addMimeType( mt );
  }

  if ( !AbstractSearchManager::instance()->addSearch( col ) )
    return failureResponse( "Unable to add search to search manager" );

  if ( !transaction.commit() )
    return failureResponse( "Unable to commit transaction" );

  const QByteArray b = HandlerHelper::collectionToByteArray( col );

  Response colResponse;
  colResponse.setUntagged();
  colResponse.setString( b );
  emit responseAvailable( colResponse );

  Response response;
  response.setTag( tag() );
  response.setSuccess();
  response.setString( "SEARCH_STORE completed" );
  emit responseAvailable( response );
  return true;
}

#include "searchpersistent.moc"
