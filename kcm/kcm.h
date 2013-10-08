#ifndef UFW_KCM_H
#define UFW_KCM_H

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

#include <KDE/KCModule>
#include <kauth.h>
#include <QtCore/QSet>
#include <QtCore/QMap>
#include <QtGui/QMenu>
#include <QtGui/QAction>
#include <QtGui/QCheckBox>
#include "types.h"
#include "rule.h"
#include "profile.h"
#include "blocker.h"
#include "ui_ufw.h"

class QDomNode;

using namespace KAuth;

namespace UFW
{

class LogViewer;
class RuleDialog;

class Kcm : public KCModule, public Ui::Ufw
{
    Q_OBJECT

    public:

    explicit Kcm(QWidget *parent=NULL, const QVariantList &list=QVariantList());
    virtual ~Kcm();

    bool          addRules(const QList<Rule> &rules);
    void          createRule(const Rule &rule);
    void          editRule(Rule rule);
    void          editRuleDescr(const Rule &rule);
    bool          ipV6Enabled() { return ipv6Enabled->isChecked(); }
    bool          isActive()    { return blocker->isActive(); }

    Q_SIGNALS:

    void          status(const QString &msg);
    void          error(const QString &msg);

    protected Q_SLOTS:

    void          defaults();
    void          queryStatus(bool readDefaults=true, bool listProfiles=true);
    void          setStatus();
    void          setIpV6();
    void          createRules();
    void          editRule();
    void          removeRule();
    void          moveRuleUp();
    void          moveRuleDown();
    void          moveTo(const QTreeWidgetItem *item);
    void          setLogLevel();
    void          setDefaultIncomingPolicy();
    void          setDefaultOutgoingPolicy();
    void          queryPerformed(ActionReply reply);
    void          modifyPerformed(ActionReply reply);
    void          ruleSelectionChanged();
    void          ruleDoubleClicked(QTreeWidgetItem *item , int col);
    void          moduleClicked(QTreeWidgetItem *item , int col);
    void          saveProfile();
    void          loadProfile(QAction *profile);
    void          removeProfile(QAction *profile);
    void          importProfile();
    void          exportProfile();
    void          loadMenuShown();
    void          deleteMenuShown();
    void          displayLog();

    private:

    QString       getNewProfileName(const QString &currentName, bool isImport);
    void          listUserProfiles();
    QAction *     getAction(const QString &name);
    QAction *     getCurrentProfile();
    void          saveProfile(const QString &name, const Profile &profile);
    void          refreshProfiles(const QMap<QString, QVariant> &profileList);
    bool          profileExists(const QString &name);
    void          addProfile(const QString &name, const Profile &profile, bool sort=true);
    void          sortActions();
    void          deleteProfile(QAction *profile, bool updateState=true);
    void          deleteProfile(const QString &name);
    void          moveRulePos(int offset);
    void          moveRule(int from, int to);
    void          showCurrentStatus();
    void          setupWidgets();
    void          setupActions();
    void          addModules();
    void          setStatus(const Profile &profile);
    void          setDefaults(const Profile &profile);
    void          setModules(const Profile &profile);
    void          setRules(const Profile &profile);
    QSet<QString> modules();

    private:

    RuleDialog               *addDialog,
                             *editDialog;
    Action                   queryAction,
                             modifyAction;
    QList<Rule>              currentRules;
    QSet<QString>            otherModules;
    unsigned int             moveToPos;
    QMenu                    *loadMenu,
                             *deleteMenu;
    QAction                  *noProfilesAction;
    QMap<QAction *, Profile> profiles;
    bool                     loadMenuWasShown;
    QString                  loadedProfile;
    bool                     activeAction;
    Blocker                  *blocker;
    QSet<QString>            existingProfiles;
    LogViewer                *logViewer;
};

}

#endif
