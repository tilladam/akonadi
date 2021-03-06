/*
    Copyright (c) 2014 Christian Mollekopf <mollekopf@kolabsys.com>

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

#include <QObject>

#include <imapstreamparser.h>
#include <response.h>
#include <storage/selectquerybuilder.h>

#include "fakeakonadiserver.h"
#include "aktest.h"
#include "akdebug.h"
#include "entities.h"
#include "dbinitializer.h"

#include <QtTest/QTest>

using namespace Akonadi;
using namespace Akonadi::Server;

Q_DECLARE_METATYPE(Akonadi::Server::Relation::List);
Q_DECLARE_METATYPE(Akonadi::Server::Relation);

static NotificationMessageV3::List extractNotifications(QSignalSpy *notificationSpy)
{
    NotificationMessageV3::List receivedNotifications;
    for (int q = 0; q < notificationSpy->size(); q++) {
        //Only one notify call
        if (notificationSpy->at(q).count() != 1) {
            qWarning() << "Error: We're assuming only one notify call.";
            return NotificationMessageV3::List();
        }
        const NotificationMessageV3::List n = notificationSpy->at(q).first().value<NotificationMessageV3::List>();
        for (int i = 0; i < n.size(); i++) {
            // qDebug() << n.at(i);
            receivedNotifications.append(n.at(i));
        }
    }
    return receivedNotifications;
}

class RelationHandlerTest : public QObject
{
    Q_OBJECT

public:
    RelationHandlerTest()
        : QObject()
    {
        qRegisterMetaType<Akonadi::Server::Response>();
        qRegisterMetaType<Akonadi::Server::Relation::List>();

        try {
            FakeAkonadiServer::instance()->setPopulateDb(false);
            FakeAkonadiServer::instance()->init();
        } catch (const FakeAkonadiServerException &e) {
            akError() << "Server exception: " << e.what();
            akFatal() << "Fake Akonadi Server failed to start up, aborting test";
        }

        RelationType type;
        type.setName(QLatin1String("type"));
        type.insert();

        RelationType type2;
        type2.setName(QLatin1String("type2"));
        type2.insert();
    }

    ~RelationHandlerTest()
    {
        FakeAkonadiServer::instance()->quit();
    }

    QScopedPointer<DbInitializer> initializer;

    Akonadi::NotificationMessageV3 relationNotification(const Akonadi::NotificationMessageV3 &notificationTemplate, const PimItem &item1, const PimItem &item2, const QByteArray &rid, const QByteArray &type = QByteArray("type"))
    {
        Akonadi::NotificationMessageV3 notification = notificationTemplate;
        QSet<QByteArray> itemParts;
        itemParts << "LEFT " + QByteArray::number(item1.id());
        itemParts << "RIGHT " + QByteArray::number(item2.id());
        itemParts << "TYPE " + type;
        itemParts << "RID " + rid;
        notification.setItemParts(itemParts);
        return notification;
    }

private Q_SLOTS:
    void testStoreRelation_data()
    {
        initializer.reset(new DbInitializer);
        Resource res = initializer->createResource("testresource");
        Collection col1 = initializer->createCollection("col1");
        PimItem item1 = initializer->createItem("item1", col1);
        PimItem item2 = initializer->createItem("item2", col1);

        QTest::addColumn<QList<QByteArray> >("scenario");
        QTest::addColumn<Relation::List>("expectedRelations");
        QTest::addColumn<Akonadi::NotificationMessageV3::List>("expectedNotifications");

        Akonadi::NotificationMessageV3 notificationTemplate;
        notificationTemplate.setType(NotificationMessageV2::Relations);
        notificationTemplate.setOperation(NotificationMessageV2::Add);
        notificationTemplate.setSessionId(FakeAkonadiServer::instanceName().toLatin1());

        {
            QList<QByteArray> scenario;
            scenario << FakeAkonadiServer::defaultScenario()
            << "C: 2 UID RELATIONSTORE LEFT " + QByteArray::number(item1.id()) + " RIGHT " + QByteArray::number(item2.id()) + " TYPE \"type\""
            << "S: 2 OK Store completed";

            Relation rel;
            rel.setLeftId(item1.id());
            rel.setRightId(item2.id());
            RelationType type;
            type.setName(QLatin1String("type"));
            rel.setRelationType(type);

            Akonadi::NotificationMessageV3 itemNotification;
            itemNotification.setType(NotificationMessageV2::Items);
            itemNotification.setOperation(NotificationMessageV2::ModifyRelations);
            itemNotification.setSessionId(FakeAkonadiServer::instanceName().toLatin1());
            itemNotification.setResource("testresource");
            itemNotification.setParentCollection(col1.id());
            itemNotification.addEntity(item1.id(), item1.remoteId(), QString(), item1.mimeType().name());
            itemNotification.addEntity(item2.id(), item2.remoteId(), QString(), item2.mimeType().name());
            itemNotification.setAddedFlags(QSet<QByteArray>() << "RELATION type " + QByteArray::number(item1.id()) + " " + QByteArray::number(item2.id()));

            Akonadi::NotificationMessageV3 notification = relationNotification(notificationTemplate, item1, item2, rel.remoteId().toLatin1());

            QTest::newRow("uid create relation") << scenario << (Relation::List() << rel) << (NotificationMessageV3::List() << notification << itemNotification);
        }
    }

    void testStoreRelation()
    {
        QFETCH(QList<QByteArray>, scenario);
        QFETCH(Relation::List, expectedRelations);
        QFETCH(NotificationMessageV3::List, expectedNotifications);

        FakeAkonadiServer::instance()->setScenario(scenario);
        FakeAkonadiServer::instance()->runTest();

        const NotificationMessageV3::List receivedNotifications = extractNotifications(FakeAkonadiServer::instance()->notificationSpy());
        QCOMPARE(receivedNotifications.size(), expectedNotifications.count());
        for (int i = 0; i < expectedNotifications.size(); i++) {
            QCOMPARE(receivedNotifications.at(i), expectedNotifications.at(i));
        }

        const Relation::List relations = Relation::retrieveAll();
        // Q_FOREACH (const Relation &rel, relations) {
        //     akDebug() << rel.leftId() << rel.rightId();
        // }
        QCOMPARE(relations.size(), expectedRelations.size());
        for (int i = 0; i < relations.size(); i++) {
            QCOMPARE(relations.at(i).leftId(), expectedRelations.at(i).leftId());
            QCOMPARE(relations.at(i).rightId(), expectedRelations.at(i).rightId());
            // QCOMPARE(relations.at(i).typeId(), expectedRelations.at(i).typeId());
            QCOMPARE(relations.at(i).remoteId(), expectedRelations.at(i).remoteId());
        }
        QueryBuilder qb(Relation::tableName(), QueryBuilder::Delete);
        qb.exec();
    }

    void testRemoveRelation_data()
    {
        initializer.reset(new DbInitializer);
        QCOMPARE(Relation::retrieveAll().size(), 0);
        Resource res = initializer->createResource("testresource");
        Collection col1 = initializer->createCollection("col1");
        PimItem item1 = initializer->createItem("item1", col1);
        PimItem item2 = initializer->createItem("item2", col1);

        Relation rel;
        rel.setLeftId(item1.id());
        rel.setRightId(item2.id());
        rel.setRelationType(RelationType::retrieveByName(QLatin1String("type")));
        QVERIFY(rel.insert());

        Relation rel2;
        rel2.setLeftId(item1.id());
        rel2.setRightId(item2.id());
        rel2.setRelationType(RelationType::retrieveByName(QLatin1String("type2")));
        QVERIFY(rel2.insert());

        Akonadi::NotificationMessageV3 notificationTemplate;
        notificationTemplate.setType(NotificationMessageV2::Relations);
        notificationTemplate.setOperation(NotificationMessageV2::Remove);
        notificationTemplate.setSessionId(FakeAkonadiServer::instanceName().toLatin1());

        QTest::addColumn<QList<QByteArray> >("scenario");
        QTest::addColumn<Relation::List>("expectedRelations");
        QTest::addColumn<Akonadi::NotificationMessageV3::List>("expectedNotifications");
        {
            QList<QByteArray> scenario;
            scenario << FakeAkonadiServer::defaultScenario()
            << "C: 2 UID RELATIONREMOVE LEFT " + QByteArray::number(item1.id()) + " RIGHT " + QByteArray::number(item2.id()) + " TYPE \"type\""
            << "S: 2 OK RELATIONREMOVE complete";

            Akonadi::NotificationMessageV3 itemNotification;
            itemNotification.setType(NotificationMessageV2::Items);
            itemNotification.setOperation(NotificationMessageV2::ModifyRelations);
            itemNotification.setSessionId(FakeAkonadiServer::instanceName().toLatin1());
            itemNotification.setResource("testresource");
            itemNotification.setParentCollection(col1.id());
            itemNotification.addEntity(item1.id(), item1.remoteId(), QString(), item1.mimeType().name());
            itemNotification.addEntity(item2.id(), item2.remoteId(), QString(), item2.mimeType().name());
            itemNotification.setRemovedFlags(QSet<QByteArray>() << "RELATION type " + QByteArray::number(item1.id()) + " " + QByteArray::number(item2.id()));

            Akonadi::NotificationMessageV3 notification = relationNotification(notificationTemplate, item1, item2, rel.remoteId().toLatin1());

            QTest::newRow("uid remove relation") << scenario << (Relation::List() << rel2) << (NotificationMessageV3::List() << notification << itemNotification);
        }

        {
            QList<QByteArray> scenario;
            scenario << FakeAkonadiServer::defaultScenario()
            << "C: 2 UID RELATIONREMOVE LEFT " + QByteArray::number(item1.id()) + " RIGHT " + QByteArray::number(item2.id())
            << "S: 2 OK RELATIONREMOVE complete";

            Akonadi::NotificationMessageV3 itemNotification;
            itemNotification.setType(NotificationMessageV2::Items);
            itemNotification.setOperation(NotificationMessageV2::ModifyRelations);
            itemNotification.setSessionId(FakeAkonadiServer::instanceName().toLatin1());
            itemNotification.setResource("testresource");
            itemNotification.setParentCollection(col1.id());
            itemNotification.addEntity(item1.id(), item1.remoteId(), QString(), item1.mimeType().name());
            itemNotification.addEntity(item2.id(), item2.remoteId(), QString(), item2.mimeType().name());
            itemNotification.setRemovedFlags(QSet<QByteArray>() << "RELATION type2 " + QByteArray::number(item1.id()) + " " + QByteArray::number(item2.id()));

            Akonadi::NotificationMessageV3 notification = relationNotification(notificationTemplate, item1, item2, rel.remoteId().toLatin1(), "type2");

            QTest::newRow("uid remove relation without type") << scenario << Relation::List() << (NotificationMessageV3::List() << notification << itemNotification);
        }

    }

    void testRemoveRelation()
    {
        QFETCH(QList<QByteArray>, scenario);
        QFETCH(Relation::List, expectedRelations);
        QFETCH(NotificationMessageV3::List, expectedNotifications);

        FakeAkonadiServer::instance()->setScenario(scenario);
        FakeAkonadiServer::instance()->runTest();

        const NotificationMessageV3::List receivedNotifications = extractNotifications(FakeAkonadiServer::instance()->notificationSpy());
        QCOMPARE(receivedNotifications.size(), expectedNotifications.count());
        for (int i = 0; i < expectedNotifications.size(); i++) {
            QCOMPARE(receivedNotifications.at(i), expectedNotifications.at(i));
        }

        const Relation::List relations = Relation::retrieveAll();
        // Q_FOREACH (const Relation &rel, relations) {
        //     akDebug() << rel.leftId() << rel.rightId() << rel.relationType().name() << rel.remoteId();
        // }
        QCOMPARE(relations.size(), expectedRelations.size());
        for (int i = 0; i < relations.size(); i++) {
            QCOMPARE(relations.at(i).leftId(), expectedRelations.at(i).leftId());
            QCOMPARE(relations.at(i).rightId(), expectedRelations.at(i).rightId());
            QCOMPARE(relations.at(i).typeId(), expectedRelations.at(i).typeId());
            QCOMPARE(relations.at(i).remoteId(), expectedRelations.at(i).remoteId());
        }
    }

    void testListRelation_data()
    {
        QueryBuilder qb(Relation::tableName(), QueryBuilder::Delete);
        qb.exec();

        initializer.reset(new DbInitializer);
        QCOMPARE(Relation::retrieveAll().size(), 0);
        Resource res = initializer->createResource("testresource");
        Collection col1 = initializer->createCollection("col1");
        PimItem item1 = initializer->createItem("item1", col1);
        PimItem item2 = initializer->createItem("item2", col1);
        PimItem item3 = initializer->createItem("item3", col1);
        PimItem item4 = initializer->createItem("item4", col1);

        Relation rel;
        rel.setLeftId(item1.id());
        rel.setRightId(item2.id());
        rel.setRelationType(RelationType::retrieveByName(QLatin1String("type")));
        rel.setRemoteId(QLatin1String("foobar1"));
        QVERIFY(rel.insert());

        Relation rel2;
        rel2.setLeftId(item1.id());
        rel2.setRightId(item2.id());
        rel2.setRelationType(RelationType::retrieveByName(QLatin1String("type2")));
        rel2.setRemoteId(QLatin1String("foobar2"));
        QVERIFY(rel2.insert());

        Relation rel3;
        rel3.setLeftId(item3.id());
        rel3.setRightId(item4.id());
        rel3.setRelationType(RelationType::retrieveByName(QLatin1String("type")));
        rel3.setRemoteId(QLatin1String("foobar3"));
        QVERIFY(rel3.insert());

        Relation rel4;
        rel4.setLeftId(item4.id());
        rel4.setRightId(item3.id());
        rel4.setRelationType(RelationType::retrieveByName(QLatin1String("type")));
        rel4.setRemoteId(QLatin1String("foobar4"));
        QVERIFY(rel4.insert());

        QTest::addColumn<QList<QByteArray> >("scenario");
        {
            QList<QByteArray> scenario;
            scenario << FakeAkonadiServer::defaultScenario()
            << "C: 2 UID RELATIONFETCH (TYPE (\"type\"))"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item1.id()) + " RIGHT " + QByteArray::number(item2.id()) + " TYPE type REMOTEID foobar1)"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item3.id()) + " RIGHT " + QByteArray::number(item4.id()) + " TYPE type REMOTEID foobar3)"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item4.id()) + " RIGHT " + QByteArray::number(item3.id()) + " TYPE type REMOTEID foobar4)"
            << "S: 2 OK RELATIONFETCH complete";

            QTest::newRow("filter by type") << scenario;
        }
        {
            QList<QByteArray> scenario;
            scenario << FakeAkonadiServer::defaultScenario()
            << "C: 2 UID RELATIONFETCH ()"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item1.id()) + " RIGHT " + QByteArray::number(item2.id()) + " TYPE type REMOTEID foobar1)"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item1.id()) + " RIGHT " + QByteArray::number(item2.id()) + " TYPE type2 REMOTEID foobar2)"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item3.id()) + " RIGHT " + QByteArray::number(item4.id()) + " TYPE type REMOTEID foobar3)"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item4.id()) + " RIGHT " + QByteArray::number(item3.id()) + " TYPE type REMOTEID foobar4)"
            << "S: 2 OK RELATIONFETCH complete";

            QTest::newRow("no filter") << scenario;
        }
        {
            QList<QByteArray> scenario;
            scenario << FakeAkonadiServer::defaultScenario()
            << "C: 2 UID RELATIONFETCH (RESOURCE \"testresource\")"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item1.id()) + " RIGHT " + QByteArray::number(item2.id()) + " TYPE type REMOTEID foobar1)"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item1.id()) + " RIGHT " + QByteArray::number(item2.id()) + " TYPE type2 REMOTEID foobar2)"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item3.id()) + " RIGHT " + QByteArray::number(item4.id()) + " TYPE type REMOTEID foobar3)"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item4.id()) + " RIGHT " + QByteArray::number(item3.id()) + " TYPE type REMOTEID foobar4)"
            << "S: 2 OK RELATIONFETCH complete";

            QTest::newRow("filter by resource with matching resource") << scenario;
        }
        {

            Resource res;
            res.setName(QLatin1String("testresource2"));
            res.insert();

            QList<QByteArray> scenario;
            scenario << FakeAkonadiServer::defaultScenario()
            << "C: 2 UID RELATIONFETCH (RESOURCE \"testresource2\")"
            << "S: 2 OK RELATIONFETCH complete";

            QTest::newRow("filter by resource with nonmatching resource") << scenario;
        }
        {
            QList<QByteArray> scenario;
            scenario << FakeAkonadiServer::defaultScenario()
            << "C: 2 UID RELATIONFETCH (LEFT " + QByteArray::number(item1.id()) + "TYPE (\"type\"))"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item1.id()) + " RIGHT " + QByteArray::number(item2.id()) + " TYPE type REMOTEID foobar1)"
            << "S: 2 OK RELATIONFETCH complete";

            QTest::newRow("filter by left and type") << scenario;
        }
        {
            QList<QByteArray> scenario;
            scenario << FakeAkonadiServer::defaultScenario()
            << "C: 2 UID RELATIONFETCH (RIGHT " + QByteArray::number(item2.id()) + "TYPE (\"type\"))"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item1.id()) + " RIGHT " + QByteArray::number(item2.id()) + " TYPE type REMOTEID foobar1)"
            << "S: 2 OK RELATIONFETCH complete";

            QTest::newRow("filter by right and type") << scenario;
        }
        {
            QList<QByteArray> scenario;
            scenario << FakeAkonadiServer::defaultScenario()
            << "C: 2 UID RELATIONFETCH (SIDE " + QByteArray::number(item3.id()) + "TYPE (\"type\"))"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item3.id()) + " RIGHT " + QByteArray::number(item4.id()) + " TYPE type REMOTEID foobar3)"
            << "S: * RELATIONFETCH (LEFT " + QByteArray::number(item4.id()) + " RIGHT " + QByteArray::number(item3.id()) + " TYPE type REMOTEID foobar4)"
            << "S: 2 OK RELATIONFETCH complete";

            QTest::newRow("fetch by side with typefilter") << scenario;
        }
    }

    void testListRelation()
    {
        QFETCH(QList<QByteArray>, scenario);

        FakeAkonadiServer::instance()->setScenario(scenario);
        FakeAkonadiServer::instance()->runTest();
    }

};

AKTEST_FAKESERVER_MAIN(RelationHandlerTest)

#include "relationhandlertest.moc"
