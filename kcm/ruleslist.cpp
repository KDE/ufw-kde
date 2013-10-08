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

#include "ruleslist.h"
#include <KDE/KConfig>
#include <KDE/KConfigGroup>
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <QtCore/QTimer>
#include <QtGui/QDropEvent>
#include <QtGui/QHeaderView>

namespace UFW
{

#define CFG_GROUP "KCM_UFW_RulesList"
#define CFG_STATE "State"

RulesList::RulesList(QWidget *parent)
         : QTreeWidget(parent)
         , headerSizesSet(false)
{
    // Can't restore QHeaderView in constructor, so use a timer - and restore after eventloop starts.
    QTimer::singleShot(0, this, SLOT(restoreState()));
}

RulesList::~RulesList()
{
    KConfigGroup grp(KGlobal::config(), CFG_GROUP);
    grp.writeEntry(CFG_STATE, header()->saveState());
}

QTreeWidgetItem * RulesList::insert(const Rule &rule)
{
    static const QString pad(" "); // Add some padding so that when re-size treeview, there's a bigger gap

    return new QTreeWidgetItem(this, QStringList() << rule.actionStr()+pad
                                                   << rule.fromStr()+pad
                                                   << rule.toStr()+pad
                                                   << rule.ipV6Str()+pad
                                                   << rule.loggingStr()+pad
                                                   << rule.getDescription()+pad);
}

void RulesList::resizeToContents()
{
    if(!headerSizesSet && topLevelItemCount()>0)
    {
        header()->resizeSections(QHeaderView::ResizeToContents);
        headerSizesSet=true;
    }
}

void RulesList::dropEvent(QDropEvent *event)
{
    emit dropped(itemAt(event->pos()));

    event->ignore();
}

void RulesList::restoreState()
{
    KConfigGroup grp(KGlobal::config(), CFG_GROUP);
    QByteArray state=grp.readEntry(CFG_STATE, QByteArray());
    if(!state.isEmpty())
    {
        header()->restoreState(state);
        headerSizesSet=true;
    }
}

}

#include "ruleslist.moc"
