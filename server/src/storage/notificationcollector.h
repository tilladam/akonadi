/*
    Copyright (c) 2006 Volker Krause <volker.krause@rwth-aachen.de>

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

#ifndef AKONADI_NOTIFICATIONCOLLECTOR_H
#define AKONADI_NOTIFICATIONCOLLECTOR_H

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <storage/entity.h>

namespace Akonadi {

class DataStore;

/**
  This class is used to store notifications within the NotificationCollector.
*/
class NotificationItem
{
  public:
    enum Type {
      Item,
      Collection
    };

    typedef QList<NotificationItem> List;

    /**
      Creates a notification item for PIM items.
    */
    NotificationItem( const PimItem &item, const Location &collection,
                      const QByteArray &mimeType, const QByteArray &resource );

    /**
      Creates a notification item for collections.
    */
    NotificationItem( const QString &collection, const QByteArray &resource );

    /**
      Creates a notification item for collections.
    */
    NotificationItem( const Location &collection, const QByteArray &resource );

    /**
      Returns the type of this item.
    */
    Type type() const { return mType; }

    /**
      Returns the uid of the changed item.
    */
    int uid() const { return mItem.id(); }

    /**
      Returns the PimItem of the changed item.
    */
    PimItem pimItem() const { return mItem; }

    /**
      Returns the changed collection.
    */
    Location collection() const { return mCollection; }

    /**
      Returns the collection name.
    */
    QString collectionName() const;

    /**
      Sets the changed collection.
    */
    void setCollection( const Location &collection ) { mCollection = collection; }

    /**
      Returns the mime-type of the changed item.
    */
    QByteArray mimeType() const { return mMimeType; }

    /**
      Sets the mimetype of the changed item.
    */
    void setMimeType( const QByteArray &mimeType ) { mMimeType = mimeType; }

    /**
      Returns the resource of the changed collection/item.
    */
    QByteArray resource() const { return mResource; }

    /**
      Sets the resource of the changed collection/item.
    */
    void setResource( const QByteArray &resource ) { mResource = resource; }

    /**
      Returns true if all necessary information are available.
    */
    bool isComplete() const;

    /**
      Compares two NotificationItem objects.
    */
    bool operator==( const NotificationItem &item ) const;

  private:
    Type mType;
    PimItem mItem;
    Location mCollection;
    QByteArray mMimeType;
    QByteArray mResource;
    QString mCollectionName;
};


/**
  Part of the DataStore, collects change notifications and emits
  them after the current transaction has been successfully committed.
  Where possible, notifications are compressed.

  @todo The itemRemoved() (and maybe other) signals also needs the remoteid,
        maybe we should use DataReference everywhere?
*/
class NotificationCollector : public QObject
{
  Q_OBJECT

  public:
    /**
      Create a new notification collector for the given DataStore @p db.
    */
    NotificationCollector( DataStore *db );

    /**
      Destroys this notification collector.
    */
    ~NotificationCollector();

    /**
      Notify about an added item.
      Provide as many parameters as you have at hand currently, everything
      that is missing will be looked up in the database later.
    */
    void itemAdded( const PimItem &item, const Location &collection = Location(),
                    const QByteArray &mimeType = QByteArray(),
                    const QByteArray &resource = QByteArray() );

    /**
      Notify about a changed item.
      Provide as many parameters as you have at hand currently, everything
      that is missing will be looked up in the database later.
    */
    void itemChanged( const PimItem &item, const Location &collection = Location(),
                      const QByteArray &mimeType = QByteArray(),
                      const QByteArray &resource = QByteArray() );

    /**
      Notify about a removed item.
      Make sure you either provide all parameters or call this function before
      actually removing the item from database.
    */
    void itemRemoved( const PimItem &item, const Location &collection = Location(),
                      const QByteArray &mimeType = QByteArray(),
                      const QByteArray &resource = QByteArray() );

    /**
      Notify about a added collection.
      Provide as many parameters as you have at hand currently, everything
      that is missing will be looked up in the database later.
     */
    void collectionAdded( const QString &collection,
                          const QByteArray &resource = QByteArray() );

    /**
      Notify about a changed collection.
      Provide as many parameters as you have at hand currently, everything
      that is missing will be looked up in the database later.
    */
    void collectionChanged( const Location &collection,
                            const QByteArray &resource = QByteArray() );

    /**
      Notify about a removed collection.
      Make sure you either provide all parameters or call this function before
      actually removing the item from database.
     */
    void collectionRemoved( const Location &collection,
                            const QByteArray &resource = QByteArray() );

  Q_SIGNALS:
    void itemAddedNotification( int uid, const QString &collection,
                                const QByteArray &mimeType,
                                const QByteArray &resource );
    void itemChangedNotification( int uid, const QString &collection,
                                  const QByteArray &mimeType,
                                  const QByteArray &resource );
    void itemRemovedNotification( int uid, const QString &collection,
                                  const QByteArray &mimeType,
                                  const QByteArray &resource );

    void collectionAddedNotification( const QString &collection,
                                      const QByteArray &resource );
    void collectionChangedNotification( const QString &collection,
                                        const QByteArray &resource );
    void collectionRemovedNotification( const QString &collection,
                                        const QByteArray &resource );

  private:
    void completeItem( NotificationItem &item );
    void clear();

  private Q_SLOTS:
    void transactionCommitted();
    void transactionRolledBack();

  private:
    DataStore *mDb;

    NotificationItem::List mAddedItems;
    NotificationItem::List mChangedItems;
    NotificationItem::List mRemovedItems;

    NotificationItem::List mAddedCollections;
    NotificationItem::List mChangedCollections;
    NotificationItem::List mRemovedCollections;
};


}

#endif