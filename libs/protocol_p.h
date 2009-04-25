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

#ifndef AKONADI_PROTOCOL_P_H
#define AKONADI_PROTOCOL_P_H

/**
  @file protocol_p.h Shared constants used in the communication protocol between
  the Akonadi server and its clients.

  @todo Fill this file with command names, item/collection property names
  item part names, etc. and replace the usages accordingly.
*/

// D-Bus service names
#define AKONADI_DBUS_SERVER_SERVICE "org.freedesktop.Akonadi"
#define AKONADI_DBUS_CONTROL_SERVICE "org.freedesktop.Akonadi.Control"

// Commands
#define AKONADI_CMD_ITEMDELETE "REMOVE"
#define AKONADI_CMD_ITEMFETCH "FETCH"
#define AKONADI_CMD_UID "UID"
#define AKONADI_CMD_RESOURCESELECT "RESSELECT"
#define AKONADI_CMD_RID "RID"

// Command parameters
#define AKONADI_PARAM_FULLPAYLOAD "FULLPAYLOAD"
#define AKONADI_PARAM_ALLATTRIBUTES "ALLATTR"
#define AKONADI_PARAM_CACHEONLY "CACHEONLY"
#define AKONADI_PARAM_EXTERNALPAYLOAD "EXTERNALPAYLOAD"

#endif
