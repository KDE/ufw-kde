#ifndef UFW_RULES_LIST_H
#define UFW_RULES_LIST_H

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

#include "rule.h"
#include <QtGui/QTreeWidget>

class QDropEvent;

namespace UFW
{

class RulesList : public QTreeWidget
{
    Q_OBJECT

    public:

    enum Columns
    {
        COL_ACTION,
        COL_FROM,
        COL_TO,
        COL_IPV6,
        COL_LOGGING,
        COL_DESCR
    };
    
    RulesList(QWidget *parent);
    virtual ~RulesList();

    void              resizeToContents();
    QTreeWidgetItem * insert(const Rule &rule);
    void              dropEvent(QDropEvent *event);

    public Q_SLOTS:
    
    void restoreState();

    Q_SIGNALS:

    void dropped(const QTreeWidgetItem *item);
    
    private:
    
    bool headerSizesSet;
};

}

#endif
