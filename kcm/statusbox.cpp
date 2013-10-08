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

#include "statusbox.h"
#include <KDE/KIcon>
#include <KDE/KIconEffect>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>

namespace UFW
{

static const int constIconSize = 160;
static const int constBorder   = 8;

StatusBox::StatusBox(QWidget *parent)
         : QGroupBox(parent)
         , current(true)
{
    init();
}

StatusBox::StatusBox(const QString &title, QWidget *parent)
         : QGroupBox(title, parent)
         , current(true)
{
    init();
}

void StatusBox::init()
{
    QImage off=KIcon("security-low").pixmap(constIconSize, constIconSize, QIcon::Normal).toImage(),
           on=KIcon("security-high").pixmap(constIconSize, constIconSize, QIcon::Normal).toImage();

    KIconEffect::deSaturate(off, 0.8);
    //KIconEffect::deSaturate(on, 0.1);
    KIconEffect::semiTransparent(off);
    KIconEffect::semiTransparent(off);
    KIconEffect::semiTransparent(off);
    KIconEffect::semiTransparent(on);
    KIconEffect::semiTransparent(on);
    pixmaps[0]=QPixmap::fromImage(off);
    pixmaps[1]=QPixmap::fromImage(on);
    setContentsMargins(0, 0, 0, 0);
    resize(constIconSize, constIconSize);
    setStatus(false);
}

void StatusBox::setStatus(bool on)
{
    if(current!=on)
    {
        current=on;
        update();
    }
}

void StatusBox::paintEvent(QPaintEvent *ev)
{ 
    QGroupBox::paintEvent(ev);
    QRect    r(rect());
    QPainter painter(this);

    painter.drawPixmap(Qt::RightToLeft == layoutDirection()
                            ? r.left() + constBorder
                            : r.right() - constIconSize + constBorder,
                        r.top() + constBorder,
                        pixmaps[current ? 1 : 0]);
}

}
