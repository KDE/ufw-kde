#ifndef UFW_STATUS_BOX_H
#define UFW_STATUS_BOX_H

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

#include <QtGui/QGroupBox>
#include <QtGui/QPixmap>

namespace UFW
{

class StatusBox : public QGroupBox
{
    public:

    StatusBox(QWidget *parent=0);
    StatusBox(const QString &title, QWidget *parent=0);
    virtual ~StatusBox() { }

    void setStatus(bool on);
    void paintEvent(QPaintEvent *ev);

    private:
    
    void init();

    private:
    
    bool    current;
    QPixmap pixmaps[2];
};

}

#endif
