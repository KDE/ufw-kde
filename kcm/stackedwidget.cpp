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

#include "stackedwidget.h"
#include <KDE/KIcon>
#include <KDE/KIconEffect>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>

namespace UFW
{

static const int constIconSize = 192;
static const int constBorder   = 0;

StackedWidget::StackedWidget(QWidget *parent)
             : QStackedWidget(parent)
{
    QImage img=KIcon("security-high").pixmap(constIconSize, constIconSize, QIcon::Normal).toImage();

    KIconEffect::deSaturate(img, 0.5);
    KIconEffect::semiTransparent(img);
    KIconEffect::semiTransparent(img);
    KIconEffect::semiTransparent(img);
    KIconEffect::semiTransparent(img);
    pixmap=QPixmap::fromImage(img);
}

void StackedWidget::paintEvent(QPaintEvent *ev)
{ 
    QStackedWidget::paintEvent(ev);
    QRect    r(rect());
    QPainter painter(this);

    painter.drawPixmap(Qt::RightToLeft == layoutDirection()
                            ? r.left() + constBorder
                            : r.right() - constIconSize + constBorder,
                        r.top() + constBorder,
                        pixmap);
}

}
