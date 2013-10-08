#ifndef UFW_LOG_VIEWER_H
#define UFW_LOG_VIEWER_H

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

#include <kauth.h>
#include <KDE/KDialog>
#include <QtCore/QString>

class QTreeWidget;
class KAction;

using namespace KAuth;

namespace UFW
{

class Kcm;

class LogViewer : public KDialog
{
    Q_OBJECT

    public:
    
    LogViewer(Kcm *p);
    virtual ~LogViewer();

    public Q_SLOTS:
    
    void restoreState();
    void refresh();
    void toggleDisplay();
    void queryPerformed(ActionReply reply);
    void createRule();
    void selectionChanged();

    private:

    void setupWidgets();
    void setupActions();
    void parse(const QString &line);

    private:
    
    Kcm         *kcm;
    Action      viewAction;
    QString     lastLine;
    QTreeWidget *list;
    KAction     *toggleRawAction,
                *createRuleAction;
    bool        headerSizesSet;
};

}

#endif
