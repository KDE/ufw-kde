#ifndef UFW_BLOCKER_H
#define UFW_BLOCKER_H

/*
 * UFW KControl Module
 *
 * Copyright 2011 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <QtCore/QObject>

namespace UFW
{

class Blocker : public QObject
{
    Q_OBJECT

    public:
    
    Blocker(QObject *parent) : QObject(parent), active(false) { }
    virtual ~Blocker()                                        { }
    
    void add(QObject *object);
    bool eventFilter(QObject *object, QEvent *event);
    void setActive(bool b)                                    { active=b; }
    bool isActive() const                                     { return active; }

    private:
    
    bool active;
};

}

#endif
