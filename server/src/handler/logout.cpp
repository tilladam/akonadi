/***************************************************************************
 *   Copyright (C) 2006 by Till Adam <adam@kde.org>                        *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.             *
 ***************************************************************************/
#include "logout.h"

#include <QtCore/QDebug>

#include "response.h"

using namespace Akonadi::Server;

Logout::Logout(): Handler()
{
}

Logout::~Logout()
{
}

bool Logout::parseStream()
{
  Response response;
  response.setBye();
  response.setString( "Akonadi server logging out" );
  response.setUntagged();
  Q_EMIT responseAvailable( response );

  response.setSuccess();
  response.setTag( tag() );
  response.setString( "Logout completed" );
  Q_EMIT responseAvailable( response );
  Q_EMIT connectionStateChange( LoggingOut );
  return true;
}
