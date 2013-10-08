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

#include "logviewer.h"
#include "types.h"
#include "rule.h"
#include "kcm.h"
#include <kdeversion.h>
#include <KDE/KAction>
#include <KDE/KToolBar>
#include <KDE/KConfig>
#include <KDE/KConfigGroup>
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <QtGui/QVBoxLayout>
#include <QtGui/QTreeWidget>
#include <QtGui/QHeaderView>
#include <QtCore/QTimer>

namespace UFW
{

enum Columns
{
    COL_RAW,
    COL_DATE,
    COL_ACTION,
    COL_FROM,
    COL_TO
};

#define CFG_GROUP      "KCM_UFW_LogViewer"
#define CFG_LIST_STATE "ListState"
#define CFG_SHOW_RAW   "Raw"
#define CFG_SIZE       "Size"

static int strToMonth(const QString &str)
{
    static QMap<QString, int> map;
    
    if(map.isEmpty())
    {
        map["Jan"]=1;
        map["Feb"]=2;
        map["Mar"]=3;
        map["Apr"]=4;
        map["May"]=5;
        map["Jun"]=6;
        map["Jul"]=7;
        map["Aug"]=8;
        map["Sep"]=9;
        map["Oct"]=10;
        map["Nov"]=11;
        map["Dec"]=12;
    }

    return map.contains(str) ? map[str] : -1;
}

static QString parseDate(const QString &monthStr, const QString &dayStr, const QString &timeStr)
{
    int month=strToMonth(monthStr),
        day=-1,
        h=-1,
        m=-1,
        s=-1;
    
    if(-1!=month)
    {
        day=dayStr.toInt();
        h=timeStr.mid(0, 2).toInt();
        m=timeStr.mid(3, 2).toInt();
        s=timeStr.mid(6, 2).toInt();

        if(s>-1)
        {
            QDateTime dateTime(QDate(QDate::currentDate().year(), month, day), QTime(h, m, s));
            if (dateTime.isValid())
                return KGlobal::locale()->formatDateTime(dateTime, KLocale::ShortDate, true);
        }
    }

    return monthStr+QChar(' ')+dayStr+QChar(' ')+timeStr;
}

LogViewer::LogViewer(Kcm *p)
         : KDialog(p)
         , kcm(p)
         , headerSizesSet(false)
{
    setupWidgets();
    setupActions();
    refresh();
    // Can't restore QHeaderView in constructor, so use a timer - and restore after eventloop starts.
    QTimer::singleShot(0, this, SLOT(restoreState()));

    KConfigGroup grp(KGlobal::config(), CFG_GROUP);
    QSize        sz=grp.readEntry(CFG_SIZE, QSize(800, 400));

    if(sz.isValid())
        resize(sz);
}

LogViewer::~LogViewer()
{
    KConfigGroup grp(KGlobal::config(), CFG_GROUP);
    grp.writeEntry(CFG_LIST_STATE, list->header()->saveState());
    grp.writeEntry(CFG_SHOW_RAW, toggleRawAction->isChecked());
    grp.writeEntry(CFG_SIZE, size());
}

void LogViewer::restoreState()
{
    KConfigGroup grp(KGlobal::config(), CFG_GROUP);
    QByteArray state=grp.readEntry(CFG_LIST_STATE, QByteArray());
    if(!state.isEmpty())
    {
        list->header()->restoreState(state);
        headerSizesSet=true;
    }
    
    toggleRawAction->setChecked(grp.readEntry(CFG_SHOW_RAW, false));
    toggleDisplay();
}

void LogViewer::refresh()
{
    QVariantMap args;
    args["lastLine"]=lastLine;
    viewAction.setArguments(args);
    viewAction.execute();
}

void LogViewer::toggleDisplay()
{
    list->setColumnHidden(COL_DATE, toggleRawAction->isChecked());
    list->setColumnHidden(COL_ACTION, toggleRawAction->isChecked());
    list->setColumnHidden(COL_FROM, toggleRawAction->isChecked());
    list->setColumnHidden(COL_TO, toggleRawAction->isChecked());
    list->setColumnHidden(COL_RAW, !toggleRawAction->isChecked());
}

void LogViewer::queryPerformed(ActionReply reply)
{
    QStringList lines=reply.succeeded() ? reply.data()["lines"].toStringList() : QStringList();

    if(!lines.isEmpty())
    {
        QStringList::ConstIterator it(lines.constBegin()),
                                   end(lines.constEnd());
                    
        for(; it!=end; ++it)
        {
            parse(*it);
            lastLine=*it;
        }
        
        if(!headerSizesSet && list->topLevelItemCount()>0)
        {
            list->header()->resizeSections(QHeaderView::ResizeToContents);
            headerSizesSet=true;
        }
    }
}

void LogViewer::setupWidgets()
{
    QWidget     *mainWidget=new QWidget(this);
    QVBoxLayout *layout=new QVBoxLayout(mainWidget);
    KToolBar    *toolbar=new KToolBar(mainWidget);
    KAction     *refreshAction=new KAction(KIcon("view-refresh"), i18n("Refresh"), this);
    toggleRawAction=new KAction(KIcon("flag-red"), i18n("Display Raw"), this);
    toggleRawAction->setCheckable(true);
    createRuleAction=new KAction(KIcon("list-add"), i18n("Create Rule"), this);
    connect(toggleRawAction, SIGNAL(toggled(bool)), SLOT(toggleDisplay()));
    connect(refreshAction, SIGNAL(triggered(bool)), SLOT(refresh()));
    connect(createRuleAction, SIGNAL(triggered(bool)), SLOT(createRule()));
    toolbar->addAction(refreshAction);
    toolbar->addAction(toggleRawAction);
    toolbar->addAction(createRuleAction);
    toolbar->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
    list=new QTreeWidget(this);
    QTreeWidgetItem *headerItem = list->headerItem();
    headerItem->setText(COL_RAW, i18n("Raw"));
    headerItem->setText(COL_DATE, i18n("Date"));
    headerItem->setText(COL_ACTION, i18n("Action"));
    headerItem->setText(COL_FROM, i18n("From"));
    headerItem->setText(COL_TO, i18n("To"));
    list->setRootIsDecorated(false);
    list->setItemsExpandable(false);
    list->setAllColumnsShowFocus(true);
    layout->addWidget(toolbar);
    layout->addWidget(list);
    setMainWidget(mainWidget);
    setCaption(i18n("Log Viewer"));
    setButtons(KDialog::Close);
    
    connect(list, SIGNAL(itemSelectionChanged()), SLOT(selectionChanged()));
    selectionChanged();
}

void LogViewer::setupActions()
{
    viewAction=KAuth::Action("org.kde.ufw.viewlog");
    viewAction.setHelperID("org.kde.ufw");
#if KDE_IS_VERSION(4, 5, 90)
    viewAction.setParentWidget(this);
#endif
//     queryAction.setExecutesAsync(true);
    connect(viewAction.watcher(), SIGNAL(actionPerformed(ActionReply)), SLOT(queryPerformed(ActionReply)));
}

void LogViewer::parse(const QString &line)
{
    // Apr  6 11:42:41 kubuntu-10 kernel: [36122.101381] [UFW BLOCK] IN= OUT=eth0 SRC=1.2.3.4 DST=1.2.3.4 LEN=76 TOS=0x00 PREC=0x00 TTL=64 ID=0 DF PROTO=UDP SPT=1 DPT=1 LEN=56
    QString l(line);
    l=l.replace("[UFW ", "[UFW_");
    l=l.replace("\n", "");
    QStringList parts=l.split(' ', QString::SkipEmptyParts);
    
    if(parts.length()>11)
    {
        QStringList::ConstIterator it(parts.constBegin()),
                                   end(parts.constEnd());
        QString                    destAddress,
                                   sourceAddress,
                                   destPort,
                                   sourcePort,
                                   interfaceIn,
                                   interfaceOut,
                                   action,
                                   date=parseDate(parts[0], parts[1], parts[2]);
        Types::Protocol            protocol=Types::PROTO_BOTH;
        
        for(; it!=end; ++it)
        {           
            if((*it).startsWith(QLatin1String("IN=")))
                interfaceIn=(*it).mid(3);
            else if((*it).startsWith(QLatin1String("OUT=")))
                interfaceOut=(*it).mid(4);
            else if((*it).startsWith(QLatin1String("SRC=")))
                sourceAddress=(*it).mid(4);
            else if((*it).startsWith(QLatin1String("DST=")))
                destAddress=(*it).mid(4);
            else if((*it).startsWith(QLatin1String("PROTO=")))
                protocol=Types::toProtocol((*it).mid(6).toLower());
            else if((*it).startsWith(QLatin1String("SPT=")))
                sourcePort=(*it).mid(4);
            else if((*it).startsWith(QLatin1String("DPT=")))
                destPort=(*it).mid(4);
            else if((*it).startsWith(QLatin1String("[UFW_")))
            {
                if(QLatin1String("[UFW_BLOCK]")==(*it))
                    action=Types::toString(Types::POLICY_DENY); // i18n("Block");
                else if(QLatin1String("[UFW_ALLOW]")==(*it))
                    action=Types::toString(Types::POLICY_ALLOW); // i18n("Allow");
                else
                {
                    action=(*it).mid(5);
                    action.replace("]", "");
                }
            }
        }

        l=l.replace("[UFW_", "[UFW "); // Revert!
        new QTreeWidgetItem(list, QStringList() << l
                                                << date
                                                << action
                                                << Rule::modify(sourceAddress, sourcePort, QString(), interfaceIn, protocol, true)
                                                << Rule::modify(destAddress, destPort, QString(), interfaceOut, protocol, true));
    }
}

void LogViewer::selectionChanged()
{
    createRuleAction->setEnabled(1==list->selectedItems().count());
}

void LogViewer::createRule()
{
    QList<QTreeWidgetItem *> items=list->selectedItems();
    QTreeWidgetItem          *item=items.count() ? items.first() : 0L;

    if(item)
    {
        QString l(item->text(COL_RAW));
        l=l.replace("[UFW ", "[UFW_");
        QStringList                parts=l.split(' ', QString::SkipEmptyParts);
        QStringList::ConstIterator it(parts.constBegin()),
                                   end(parts.constEnd());
        QString                    destAddress,
                                   sourceAddress,
                                   destPort,
                                   sourcePort,
                                   interfaceIn,
                                   interfaceOut,
                                   action;
        Types::Protocol            protocol=Types::PROTO_BOTH;
        Types::Policy              pol=Types::POLICY_DENY;
        
        for(; it!=end; ++it)
        {           
            if((*it).startsWith(QLatin1String("IN=")))
                interfaceIn=(*it).mid(3);
            else if((*it).startsWith(QLatin1String("OUT=")))
                interfaceOut=(*it).mid(4);
            else if((*it).startsWith(QLatin1String("SRC=")))
                sourceAddress=(*it).mid(4);
            else if((*it).startsWith(QLatin1String("DST=")))
                destAddress=(*it).mid(4);
            else if((*it).startsWith(QLatin1String("PROTO=")))
                protocol=Types::toProtocol((*it).mid(6).toLower());
            else if((*it).startsWith(QLatin1String("SPT=")))
                sourcePort=(*it).mid(4);
            else if((*it).startsWith(QLatin1String("DPT=")))
                destPort=(*it).mid(4);
            else if((*it).startsWith(QLatin1String("[UFW_")))
            {
                // Invert rule type - as we are creating the inverse of what the log says!
                pol=QLatin1String("[UFW_BLOCK]")==(*it) ? Types::POLICY_ALLOW : Types::POLICY_DENY;
            }
        }

        kcm->createRule(Rule(pol, interfaceOut.isEmpty(), Types::LOGGING_OFF, protocol, QString(), QString(), 
                            sourceAddress, sourcePort,  destAddress, destPort,
                            interfaceIn, interfaceOut));
    }
}

}

#include "logviewer.moc"
