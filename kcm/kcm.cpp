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

#include "kcm.h"
#include "logviewer.h"
#include "ruledialog.h"
#include "config.h"
#include "types.h"
#include "strings.h"
#include "statusbox.h"
#include <KDE/KAboutData>
#include <KDE/KLocale>
#include <KDE/KMessageBox>
#include <KDE/KPluginFactory>
#include <KDE/KColorScheme>
#include <KDE/KFileDialog>
#include <KDE/KInputDialog>
#include <KDE/KStandardDirs>
#include <KDE/KTemporaryFile>
#include <KDE/KIO/NetAccess>
#include <QtGui/QLabel>
#include <QtGui/QTreeWidget>
#include <QtGui/QValidator>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>
#include <sys/utsname.h>

#define EXTENSION ".ufw"
#define FOLDER    "kcm_ufw"

K_PLUGIN_FACTORY(UfwFactory, registerPlugin<UFW::Kcm>(); )
K_EXPORT_PLUGIN(UfwFactory("kcm_ufw"))

namespace UFW
{

enum ModuleCol
{
    MOD_COL_NAME,
    MOD_COL_CONN_TRACK,
    MOD_COL_NAT
};

enum ModuleState
{
    MOD_STATE_CONN_TRACK_ENABLED = 0x01,
    MOD_STATE_NAT_ENABLED        = 0x02
};

struct KernelModule
{
    KernelModule(const QString &p, const QString &c=QString(), const QString &n=QString(), const QString &t=QString())
        : protocol(p), conntrack(c), nat(n), tooltip(t) { }

    bool operator<(const KernelModule &o) const
    {
        return protocol.localeAwareCompare(o.protocol)<0;
    }

    bool operator==(const KernelModule &o) const
    {
        return protocol==o.protocol ||
               (!conntrack.isEmpty() && conntrack==o.conntrack) ||
               (!nat.isEmpty() && nat==o.nat);
    }

    QString protocol,
            conntrack,
            nat,
            tooltip;
};

static QString kernerlName()
{
    struct utsname un;

    return 0==uname(&un) ? QString::fromLatin1(un.release) : QString();
}

QSet<QString> getModuleNames(const QString &kernel, const QString &subDir, const QString &filter)
{
    QStringList                entries=QDir(QString("/lib/modules/")+kernel+subDir).entryList(QStringList() << filter+"*.ko");
    QSet<QString>              names;
    QStringList::ConstIterator it(entries.constBegin()),
                               end(entries.constEnd());

    for(; it!=end; ++it)
        names.insert((*it).left((*it).length()-3)  // Remove ".ko"
                          .mid(filter.length()));  // Remove nf_XXX_

    return names;
}

static void addOtherModules(QList<KernelModule> &modules)
{
    QString kernel(kernerlName());

    if(!kernel.isEmpty())
    {
        QSet<QString>                conntrack=getModuleNames(kernel, "/kernel/net/netfilter/", "nf_conntrack_");
        QSet<QString>                nat=getModuleNames(kernel, "/kernel/net/ipv4/netfilter/", "nf_nat_");
        QSet<QString>                names=conntrack+nat;
        QSet<QString>::ConstIterator it(names.constBegin()),
                                     end(names.constEnd());
        QMap<QString, QString>       stdNames;

        stdNames["amanda"]=i18nc("kernel module name", "Amanda");
        stdNames["h323"]=i18nc("kernel module name", "H.323");
        stdNames["netlink"]=i18nc("kernel module name", "NetLink");
        stdNames["proto_dccp"]=i18nc("kernel module name", "Proto DCCP");
        stdNames["proto_gre"]=i18nc("kernel module name", "Proto GRE");
        stdNames["proto_sctp"]=i18nc("kernel module name", "Proto SCTP");
        stdNames["proto_udplite"]=i18nc("kernel module name", "Proto UDP Lite");
        stdNames["slp"]=i18nc("kernel module name", "SLP");
        stdNames["snmp_basic"]=i18nc("kernel module name", "SNMP Basic");
        stdNames["tftp"]=i18nc("kernel module name", "TFTP");

        for(; it!=end; ++it)
        {
            KernelModule mod(stdNames.contains(*it) ? stdNames[*it] : *it,
                             conntrack.contains(*it) ? "nf_conntrack_"+(*it) : QString(),
                             nat.contains(*it) ? "nf_nat_"+(*it) : QString());

            if(!modules.contains(mod))
                modules.append(mod);
        }
    }
}

static QString moduleTooltip(const QString module, const QString &tt=QString())
{
    return tt.isEmpty()
            ? i18n("<p>Enables the <b>%1</b> kernel module</p>", module)
            : i18n("<p>Enables the <b>%1</b> kernel module (for %2)</p>", module, tt);
}

static void addModule(QTreeWidget *tree, const KernelModule &mod)
{
    QTreeWidgetItem *item=new QTreeWidgetItem(tree, QStringList() << mod.protocol+QString("  "));

    if(!mod.conntrack.isEmpty())
    {
        item->setCheckState(MOD_COL_CONN_TRACK, Qt::Unchecked);
        item->setData(MOD_COL_CONN_TRACK, Qt::UserRole, mod.conntrack);
        item->setToolTip(MOD_COL_CONN_TRACK, moduleTooltip(mod.conntrack, mod.tooltip));
    }
    if(!mod.nat.isEmpty())
    {
        item->setCheckState(MOD_COL_NAT, Qt::Unchecked);
        item->setData(MOD_COL_NAT, Qt::UserRole, mod.nat);
        item->setToolTip(MOD_COL_NAT, moduleTooltip(mod.nat, mod.tooltip));
    }
}

static inline QString profileFileName(const QString &name)
{
    return KGlobal::dirs()->saveLocation("data", FOLDER"/", KStandardDirs::NoDuplicates)+name+EXTENSION;
}

static inline QString profileName(const QAction *profile)
{
    return profile->data().toString();
}

Kcm::Kcm(QWidget *parent, const QVariantList&)
   : KCModule(UfwFactory::componentData(), parent)
   , addDialog(0L)
   , editDialog(0L)
   , moveToPos(0)
   , logViewer(0L)
{
    setButtons(Help|Default);

    KAboutData *about = new KAboutData("kcm_ufw", 0, ki18n("UFW Settings"), VERSION, ki18n("GUI front-end for Uncomplicated FireWall"),
                                       KAboutData::License_GPL, ki18n("(C) Craig Drummond, 2011"), KLocalizedString(), QByteArray(),
                                       "craig.p.drummond@gmail.com");
    about->addAuthor(ki18n("Craig Drummond"), ki18n("Developer and maintainer"), "craig.p.drummond@gmail.com");
    setAboutData(about);

    setupUi(this);
    setupWidgets();
    setupActions();
    QTimer::singleShot(0, this, SLOT(queryStatus()));
}

Kcm::~Kcm()
{
    // Disconnect from KAuth signals, as there might be an action in flight - and we can no longer handle this, due to
    // being terminated!!!
    disconnect(queryAction.watcher(), SIGNAL(actionPerformed(ActionReply)), this, SLOT(queryPerformed(ActionReply)));
    disconnect(modifyAction.watcher(), SIGNAL(actionPerformed(ActionReply)), this, SLOT(modifyPerformed(ActionReply)));
}

bool Kcm::addRules(const QList<Rule> &rules)
{
    QVariantMap                args;
    QList<Rule>::ConstIterator it(rules.constBegin()),
                               end(rules.constEnd());

    args["cmd"]="addRules";
    args["count"]=rules.count();
    for(int i=0; it!=end; ++it, ++i)
    {
        if(currentRules.contains(*it))
            return false;
        args["xml"+QString().setNum(i)]=(*it).toXml();
    }

    modifyAction.setArguments(args);
    statusLabel->setText(rules.size()>1 ? i18n("Adding rules...") : i18n("Adding rule..."));
    emit status(statusLabel->fullText());
    blocker->setActive(true);
    modifyAction.execute();
    return true;
}

void Kcm::createRule(const Rule &rule)
{
    if(!addDialog)
        addDialog=new RuleDialog(this, false);

    addDialog->setRule(rule);
    addDialog->showNormal();
}

void Kcm::editRule(Rule rule)
{
    QList<QTreeWidgetItem*> items=ruleList->selectedItems();
    QTreeWidgetItem         *item=items.count() ? items.first() : 0L;

    if(item)
    {
        QVariantMap args;
        args["cmd"]="editRule";
        rule.setPosition((unsigned int)item->data(0, Qt::UserRole).toUInt());
        args["xml"]=rule.toXml();
        modifyAction.setArguments(args);
        statusLabel->setText(i18n("Updating rule..."));
        emit status(statusLabel->fullText());
        blocker->setActive(true);
        modifyAction.execute();
    }
}

void Kcm::editRuleDescr(const Rule &rule)
{
    QVariantMap args;
    args["cmd"]="editRuleDescr";
    args["xml"]=rule.toXml();
    modifyAction.setArguments(args);
    statusLabel->setText(i18n("Updating rule..."));
    emit status(statusLabel->fullText());
    blocker->setActive(true);
    modifyAction.execute();
}

void Kcm::defaults()
{
    if(KMessageBox::Yes==KMessageBox::warningYesNo(this, i18n("Reset firewall to the default settings?"), i18n("Reset")))
    {
        QVariantMap args;
        args["cmd"]="reset";
        modifyAction.setArguments(args);
        statusLabel->setText(i18n("Resetting to system default settings..."));
        blocker->setActive(true);
        modifyAction.execute();
    }
}

void Kcm::queryStatus(bool readDefaults, bool listProfiles)
{
    QVariantMap args;
    args["defaults"]=readDefaults;
    args["profiles"]=listProfiles;
    queryAction.setArguments(args);
    statusLabel->setText(i18n("Querying firewall status..."));
    blocker->setActive(true);
    queryAction.execute();
}

void Kcm::setStatus()
{
    QVariantMap args;
    args["cmd"]="setStatus";
    args["status"]=ufwEnabled->isChecked();
    modifyAction.setArguments(args);
    statusLabel->setText(ufwEnabled->isChecked() ? i18n("Enabling the firewall...") : i18n("Disabling the firewall..."));
    blocker->setActive(true);
    modifyAction.execute();
}

void Kcm::setIpV6()
{
    if(!ipv6Enabled->isChecked())
    {
        bool haveV6Rule=false;
        QList<Rule>::ConstIterator it(currentRules.constBegin()),
                                   end(currentRules.constEnd());

        for(; it!=end && !haveV6Rule; ++it)
            if((*it).getV6())
                haveV6Rule=true;
        if(haveV6Rule && KMessageBox::No==KMessageBox::warningYesNo(this, i18n("Disabling IPv6 support will remove any IPv6 rules.\nProceed?"),
                                                                    i18n("Disable IPv6 Support")))
        {
            ipv6Enabled->blockSignals(true);
            ipv6Enabled->setChecked(true);
            ipv6Enabled->blockSignals(false);
            return;
        }
    }

    QVariantMap args;
    args["cmd"]="setDefaults";
    args["ipv6"]=true;
    args["xml"]=QString("<defaults ipv6=\"")+QString(ipv6Enabled->isChecked() ? "yes" : "no")+QString("\" />");
    modifyAction.setArguments(args);
    statusLabel->setText(i18n("Setting firewall IPv6 support..."));
    blocker->setActive(true);
    modifyAction.execute();
}

void Kcm::createRules()
{
    if(!addDialog)
        addDialog=new RuleDialog(this, false);

    addDialog->reset();
    addDialog->showNormal();
}

void Kcm::editRule()
{
    QList<QTreeWidgetItem*> items=ruleList->selectedItems();
    QTreeWidgetItem         *item=items.count() ? items.first() : 0L;

    if(item)
    {
        if(!editDialog)
            editDialog=new RuleDialog(this, true);

        editDialog->setRule(currentRules.at((unsigned int)item->data(0, Qt::UserRole).toUInt()-1));
        editDialog->exec();
    }
}

void Kcm::removeRule()
{
    QList<QTreeWidgetItem*> items=ruleList->selectedItems();
    QTreeWidgetItem         *item=items.count() ? items.first() : 0L;

    if(item)
    {
        QVariantMap args;
        args["cmd"]="removeRule";
        args["index"]=QString().setNum((unsigned int)item->data(0, Qt::UserRole).toUInt())+
                      QChar(':')+
                      currentRules.at((unsigned int)item->data(0, Qt::UserRole).toUInt()-1).getHash();
        modifyAction.setArguments(args);
        statusLabel->setText(i18n("Removing rule from firewall..."));
        blocker->setActive(true);
        modifyAction.execute();
    }
}

void Kcm::moveRuleUp()
{
    moveRulePos(-1);
}

void Kcm::moveRuleDown()
{
    moveRulePos(1);
}

void Kcm::moveTo(const QTreeWidgetItem *item)
{
    if(blocker->isActive())
        return;

    QList<QTreeWidgetItem*> items=ruleList->selectedItems();

    moveRule(items.count() ? items.first()->data(0, Qt::UserRole).toUInt() : 0L,
             item ? item->data(0, Qt::UserRole).toUInt() : ruleList->topLevelItemCount()+1);
}

void Kcm::setLogLevel()
{
    QVariantMap args;
    args["cmd"]="setDefaults";
    args["xml"]=QString("<defaults loglevel=\"")+toString((Types::LogLevel)ufwLoggingLevel->currentIndex())+QString("\" />");
    modifyAction.setArguments(args);
    statusLabel->setText(i18n("Setting firewall log level..."));
    blocker->setActive(true);
    modifyAction.execute();
}

void Kcm::setDefaultIncomingPolicy()
{
    QVariantMap args;
    args["cmd"]="setDefaults";
    args["xml"]=QString("<defaults incoming=\"")+toString((Types::Policy)defaultIncomingPolicy->currentIndex())+QString("\" />");
    modifyAction.setArguments(args);
    statusLabel->setText(i18n("Setting firewall default incomming policy..."));
    blocker->setActive(true);
    modifyAction.execute();
}

void Kcm::setDefaultOutgoingPolicy()
{
    QVariantMap args;
    args["cmd"]="setDefaults";
    args["xml"]=QString("<defaults outgoing=\"")+toString((Types::Policy)defaultOutgoingPolicy->currentIndex())+QString("\" />");
    modifyAction.setArguments(args);
    statusLabel->setText(i18n("Setting firewall default outgoing policy..."));
    blocker->setActive(true);
    modifyAction.execute();
}

void Kcm::queryPerformed(ActionReply reply)
{
    QByteArray response=reply.succeeded() ? reply.data()["response"].toByteArray() : QByteArray();

    blocker->setActive(false);
    if(!response.isEmpty())
    {
        Profile profile(response);

        setStatus(profile);
        setDefaults(profile);
        setModules(profile);
        setRules(profile);
    }

    showCurrentStatus();

    if(reply.succeeded() && reply.data().contains("profiles"))
        refreshProfiles(reply.data()["profiles"].toMap());
}

void Kcm::modifyPerformed(ActionReply reply)
{
    QString cmd(reply.data()["cmd"].toString());

    blocker->setActive(false);
    emit status(QString()); // Clear add dialog status...
    if(reply.succeeded())
    {
        if("setProfile"==cmd)
        {
            QAction *currentProfile=getCurrentProfile();
            loadedProfile=currentProfile ? profileName(currentProfile) : 0L;
        }
        queryPerformed(reply);
        moveToPos=0;

        if("saveProfile"==cmd || "deleteProfile"==cmd)
            refreshProfiles(reply.data()["profiles"].toMap());
    }
    else
    {
        if("addRules"==cmd)
            emit error(QString(reply.data()["response"].toByteArray()));
        else if("removeRule"==cmd)
            KMessageBox::error(this, i18n("<p>Failed to remove rule.</p><p><i>%1</i></p>",
                                          QString(reply.data()["response"].toByteArray())));
       else if("saveProfile"==cmd)
            KMessageBox::error(this, i18n("<p>Failed to save profile.</p><p><i>%1</i></p>",
                                          QString(reply.data()["name"].toString())));
       else if("deleteProfile"==cmd)
            KMessageBox::error(this, i18n("<p>Failed to delete profile.</p><p><i>%1</i></p>",
                                          QString(reply.data()["name"].toString())));
        // Refresh list...
        moveToPos=0;
        queryStatus(true, false);
        showCurrentStatus();
    }
}

void Kcm::ruleSelectionChanged()
{
    QList<QTreeWidgetItem*> items=ruleList->selectedItems();
    bool                    enable=1==items.count();
    int                     index=enable ? items.first()->data(0, Qt::UserRole).toUInt() : 0;

    editRuleButton->setEnabled(enable);
    removeRuleButton->setEnabled(enable);
    moveRuleUpButton->setEnabled(enable && index>1);
    moveRuleDownButton->setEnabled(enable && index>0 && index<ruleList->topLevelItemCount());
}

void Kcm::ruleDoubleClicked(QTreeWidgetItem *item , int col)
{
    if(blocker->isActive())
        return;

    Q_UNUSED(col)

    if(item)
    {
        if(!editDialog)
            editDialog=new RuleDialog(this, true);

        editDialog->setRule(currentRules.at((unsigned int)item->data(0, Qt::UserRole).toUInt()-1));
        editDialog->exec();
    }
}

void Kcm::moduleClicked(QTreeWidgetItem *item, int col)
{
    // This slot get called if any part of row is clicked - even if checkbox does not change state.
    // So, to prevent bogus calls - need to check that a state change has occured.
    if(!item || MOD_COL_NAME==col ||
       (item->data(0, Qt::UserRole).toInt()==( (item->data(MOD_COL_CONN_TRACK, Qt::UserRole).toString().isEmpty() ||
                                                 Qt::Unchecked==item->checkState(MOD_COL_CONN_TRACK)
                                                    ? 0
                                                    : MOD_STATE_CONN_TRACK_ENABLED) +
                                                (item->data(MOD_COL_NAT, Qt::UserRole).toString().isEmpty() ||
                                                 Qt::Unchecked==item->checkState(MOD_COL_NAT)
                                                    ? 0
                                                    : MOD_STATE_NAT_ENABLED)) ) )
        return;

    if(blocker->isActive())
    {
        modulesList->blockSignals(true);
        item->setCheckState(col, Qt::Unchecked==item->checkState(col) ? Qt::Checked : Qt::Unchecked);
        modulesList->blockSignals(false);
    }

    QVariantMap args;
    Profile     profile(ipv6Enabled->isChecked(), (Types::LogLevel)ufwLoggingLevel->currentIndex(),
                        (Types::Policy)defaultIncomingPolicy->currentIndex(),
                        (Types::Policy)defaultOutgoingPolicy->currentIndex(), currentRules,
                        modules());

    args["cmd"]="setModules";
    args["xml"]=profile.modulesXml();
    modifyAction.setArguments(args);
    statusLabel->setText(i18n("Setting firewall modules..."));
    blocker->setActive(true);
    modifyAction.execute();
}

class ProfileNameValidator : public QValidator
{
    public:

    ProfileNameValidator(QObject *parent) : QValidator(parent) { }

    State validate(QString &input, int &) const
    {
        for(int i=0; i<input.length(); ++i)
            if(QChar('/')==input[i])
                return Invalid;

        return Acceptable;
    }
};

void Kcm::saveProfile()
{
    QString name=getNewProfileName(loadedProfile, false);

    if(!name.isEmpty())
    {
        Profile profile(ipv6Enabled->isChecked(), (Types::LogLevel)ufwLoggingLevel->currentIndex(),
                        (Types::Policy)defaultIncomingPolicy->currentIndex(),
                        (Types::Policy)defaultOutgoingPolicy->currentIndex(), currentRules,
                        modules());

        saveProfile(name, profile);
    }
}

void Kcm::loadProfile(QAction *profile)
{
    if(!loadMenuWasShown || profile==getCurrentProfile())
        return;

    Profile p=profiles[profile];

    if(!(p.hasModules() || p.hasDefaults() || p.hasRules()))
        return;

    QVariantMap args;

    args["cmd"]="setProfile";
    if(p.hasModules())
        args["modules"]=p.modulesXml();
    if(p.hasDefaults())
        args["defaults"]=p.defaultsXml();

    if(p.hasRules())
    {
        args["ruleCount"]=p.getRules().count();
        QList<Rule>::ConstIterator it(p.getRules().constBegin()),
                               end(p.getRules().constEnd());
        for(int i=0; it!=end; ++it, ++i)
            args["rule"+QString().setNum(i)]=(*it).toXml();
    }

    modifyAction.setArguments(args);
    statusLabel->setText(i18n("Activating firewall profile %1...", profileName(profile)));
    loadedProfile=QString();
    blocker->setActive(true);
    modifyAction.execute();
}

void Kcm::removeProfile(QAction *profile)
{
    if(loadMenuWasShown)
        return;

    QString name=profileName(profile);
    if(KMessageBox::Yes==KMessageBox::questionYesNo(this, i18n("<p>Remove <i>%1</i>?</p>", name), i18n("Remove Profile"),
                                                    KStandardGuiItem::yes(), KStandardGuiItem::no(),
                                                    QString(), KMessageBox::Notify|KMessageBox::Dangerous))
    {
        Profile p=profiles[profile];

        if(p.getIsSystem())
        {
            QVariantMap args;

            args["cmd"]="deleteProfile";
            args["name"]=name;
            modifyAction.setArguments(args);
            statusLabel->setText(QString("Deleting firewall profile ")+name+"...");
            blocker->setActive(true);
            modifyAction.execute();
        }
        else if(QFile::remove(p.getFileName()))
        {
            deleteProfile(profile);
            if(name==loadedProfile)
            {
                loadedProfile=QString();
                showCurrentStatus();
            }
        }
        else
            KMessageBox::error(this, i18n("<p>Failed to remove <i>%1</i></p>", name));
    }
}

void Kcm::importProfile()
{
    KUrl url=KFileDialog::getOpenUrl(KUrl(), i18n("*.ufw|Firewall Settings"), this);

    if(!url.isEmpty())
    {
        QString tempFile;

        if( KIO::NetAccess::download(url, tempFile, this))
        {
            QFile   file(tempFile);
            Profile profile(file);

            if(profile.hasRules())
            {
                QString name=getNewProfileName(url.fileName().remove(EXTENSION), true);

                if(!name.isEmpty())
                    saveProfile(name, profile);
            }
            else
                KMessageBox::error(this, i18n("<p><i>%1</i> is not a valid Firewall Settings file</p>", url.prettyUrl()));
            KIO::NetAccess::removeTempFile(tempFile);
        }
        else
            KMessageBox::error(this, KIO::NetAccess::lastErrorString());
    }
}

void Kcm::exportProfile()
{
    KUrl url=KFileDialog::getSaveUrl(KUrl(), i18n("*.ufw|Firewall Settings"), this, QString(), KFileDialog::ConfirmOverwrite);

    if(!url.isEmpty())
    {
        KTemporaryFile tempFile;

        tempFile.setAutoRemove(true);

        if(tempFile.open())
        {
            QTextStream stream(&tempFile);
            Profile     profile(ipv6Enabled->isChecked(), (Types::LogLevel)ufwLoggingLevel->currentIndex(),
                                (Types::Policy)defaultIncomingPolicy->currentIndex(),
                                (Types::Policy)defaultOutgoingPolicy->currentIndex(), currentRules,
                                modules());

            stream << profile.toXml();

            tempFile.close();
            if(!KIO::NetAccess::upload(tempFile.fileName(), url, this))
                KMessageBox::error(this, KIO::NetAccess::lastErrorString());
        }
        else
            KMessageBox::error(this, i18n("Failed to create temporary file."));
    }
}

void Kcm::loadMenuShown()
{
    // 'Load' sub-menu has been shown - so triggered actions will be loads...
    loadMenuWasShown=true;
}

void Kcm::deleteMenuShown()
{
    // 'Delete' sub-menu has been shown - so triggered actions will be deletes...
    loadMenuWasShown=false;
}

void Kcm::displayLog()
{
    if(!logViewer)
        logViewer=new LogViewer(this);
    logViewer->showNormal();
}

QString Kcm::getNewProfileName(const QString &currentName, bool isImport)
{
    QString              name=currentName;
    ProfileNameValidator validator(this);
    bool                 promptForName=!isImport;

    while(true)
    {
        if(promptForName)
        {
            name=KInputDialog::getText(i18n("Profile Name"), i18n("Please enter a name for the profile:"), name, 0, this, &validator);
            name.trimmed().simplified();
        }

        if(name.isEmpty())
            return QString();
        else
        {
            bool exists=profileExists(name);;

            if(!exists ||(!isImport && name==loadedProfile))
                return name;
            else
                switch(KMessageBox::warningYesNoCancel(this, i18n("<p>A profile named <i>%1</i> already exists.</p><p>Overwrite?</p>", name),
                                                       i18n("Overwrite Profile")))
                {
                    case KMessageBox::Cancel:
                        return QString();
                    case KMessageBox::Yes:
                        return name;
                    case KMessageBox::No:
                        promptForName=true;
                        continue; // Prompt for new name...
                }
        }
    }
}

void Kcm::listUserProfiles()
{
    QStringList                files(KGlobal::dirs()->findAllResources("data", FOLDER"/*"EXTENSION, KStandardDirs::NoDuplicates));
    QStringList::ConstIterator it(files.constBegin()),
                               end(files.constEnd());

    for(; it!=end; ++it)
    {
        QString name(QFileInfo(*it).fileName().remove(EXTENSION));

        if(!name.isEmpty() && !profileExists(name))
        {
            QFile file(*it);
            addProfile(name, Profile(file), false);
        }
    }

    if(0==loadMenu->actions().count())
    {
        loadMenu->addAction(noProfilesAction);
        deleteMenu->addAction(noProfilesAction);
    }

    sortActions();
    showCurrentStatus();
}

QAction * Kcm::getAction(const QString &name)
{
    QList<QAction *>                actions=loadMenu->actions();
    QList<QAction *>::ConstIterator it(actions.constBegin()),
                                    end(actions.constEnd());

    for(; it!=end; ++it)
        if(profileName(*it)==name)
            return *it;
    return 0L;
}


QAction * Kcm::getCurrentProfile()
{
    if(profiles.count()>0)
    {
        Profile                         current(ipv6Enabled->isChecked(),
                                                (Types::LogLevel)ufwLoggingLevel->currentIndex(),
                                                (Types::Policy)defaultIncomingPolicy->currentIndex(),
                                                (Types::Policy)defaultOutgoingPolicy->currentIndex(), currentRules,
                                                modules());
        QList<QAction *>                actions=loadMenu->actions();
        QList<QAction *>::ConstIterator it(actions.constBegin()),
                                        end(actions.constEnd());

        for(; it!=end; ++it)
        {
            Profile p();
            if(profiles[*it]==current)
                return *it;
        }
    }

    return 0L;
}

bool Kcm::profileExists(const QString &name)
{
    return getAction(name);
}

void Kcm::saveProfile(const QString &name, const Profile &profile)
{
    QVariantMap args;

    args["cmd"]="saveProfile";
    args["name"]=name;
    args["xml"]=profile.toXml();
    modifyAction.setArguments(args);
    statusLabel->setText(i18n("Saving firewall profile %1...", name));
    blocker->setActive(true);
    modifyAction.execute();
}

void Kcm::refreshProfiles(const QMap<QString, QVariant> &profileList)
{
    QSet<QString> listedProfiles=profileList.keys().toSet();
    QSet<QString> newProfiles=listedProfiles,
                  deletedProfiles=existingProfiles;

    newProfiles.subtract(existingProfiles);
    deletedProfiles.subtract(listedProfiles);
    existingProfiles.clear();

    // Remove any deleted profiles...
    QSet<QString>::ConstIterator it(deletedProfiles.constBegin()),
                                 end(deletedProfiles.constEnd());
    bool                         changed(false);

    for(; it!=end; ++it)
    {
        QString name(QFileInfo(*it).fileName().remove(EXTENSION));

        if(!name.isEmpty())
        {
            QAction *action=getAction(name);

            if(action)
            {
                Profile p=profiles[action];

                if(p.getIsSystem())
                {
                    deleteProfile(action, false);
                    changed=true;
                }
            }
        }
    }

    // Add any new profiles...
    it=newProfiles.constBegin(),
    end=newProfiles.constEnd();
    for(; it!=end; ++it)
    {
        QString name(QFileInfo(*it).fileName().remove(EXTENSION));

        if(!name.isEmpty())
        {
            QAction *action=getAction(name);

            if(action)
            {
                Profile p=profiles[action];

                if(p.getIsSystem())
                    continue;
                else if(QFile::remove(p.getFileName()))
                    deleteProfile(action, false);
                else
                    continue;
            }

            existingProfiles.insert(*it);
            changed=true;
            addProfile(name, Profile(profileList[*it].toByteArray(), true));
        }
    }

    if(changed)
    {
        if(0==loadMenu->actions().count())
        {
            loadMenu->addAction(noProfilesAction);
            deleteMenu->addAction(noProfilesAction);
        }

        sortActions();
        showCurrentStatus();
    }
}

void Kcm::addProfile(const QString &name, const Profile &profile, bool sort)
{
    QAction *action=new QAction(name, this);
    action->setData(name);
    loadMenu->addAction(action);
    deleteMenu->addAction(action);
    profiles[action]=profile;
    loadMenu->removeAction(noProfilesAction);
    deleteMenu->removeAction(noProfilesAction);
    if(sort)
    {
        sortActions();
        showCurrentStatus();
    }
}

struct ProfileAction
{
    ProfileAction(QAction *a) : action(a), name(profileName(a)) { }
    bool operator<(const ProfileAction &o) const                { return name.localeAwareCompare(o.name)<0; }
    QAction *action;
    QString name;
};

void Kcm::sortActions()
{
    if(loadMenu->actions().count()>1)
    {
        QList<QAction *>                actions=loadMenu->actions();
        QList<QAction *>::ConstIterator it(actions.constBegin()),
                                        end(actions.constEnd());
        QList<ProfileAction>            profiles;

        for(; it!=end; ++it)
        {
            profiles.append(ProfileAction(*it));
            loadMenu->removeAction(*it);
            deleteMenu->removeAction(*it);
        }

        qSort(profiles);
        QList<ProfileAction>::ConstIterator pro(profiles.constBegin()),
                                            proEnd(profiles.constEnd());

        for(; pro!=proEnd; ++pro)
        {
            loadMenu->addAction((*pro).action);
            deleteMenu->addAction((*pro).action);
        }
    }
}

void Kcm::deleteProfile(QAction *profile, bool updateState)
{
    if(profile)
    {
        QMap<QAction *, Profile>::iterator entry=profiles.find(profile);

        if(entry!=profiles.end())
            profiles.erase(entry);
        loadMenu->removeAction(profile);
        deleteMenu->removeAction(profile);
        delete profile;

        if(updateState)
        {
            if(0==loadMenu->actions().count())
            {
                loadMenu->addAction(noProfilesAction);
                deleteMenu->addAction(noProfilesAction);
            }
            showCurrentStatus();
        }
    }
}

void Kcm::deleteProfile(const QString &name)
{
    deleteProfile(getAction(name));
}

void Kcm::moveRulePos(int offset)
{
    QList<QTreeWidgetItem*> items=ruleList->selectedItems();

    if(1==items.count())
    {
        int from=items.first()->data(0, Qt::UserRole).toUInt();

        if((-1==offset && from>1) || (1==offset && from<ruleList->topLevelItemCount()))
            moveRule(from, from+offset);
    }
}

void Kcm::moveRule(int from, int to)
{
    if(blocker->isActive())
        return;

    if(from && to && from!=to)
    {
        QVariantMap args;
        args["cmd"]="moveRule";
        args["from"]=from;
        args["to"]=to;
        moveToPos=to;
        modifyAction.setArguments(args);
        statusLabel->setText(i18n("Moving rule in firewall..."));
        blocker->setActive(true);
        modifyAction.execute();
    }
}

void Kcm::showCurrentStatus()
{
    QString proName=loadedProfile;
    QAction *currentProfile=getCurrentProfile();

    if(currentProfile)
    {
        loadedProfile=profileName(currentProfile);
        proName=QString(" (")+loadedProfile+QChar(')');
    }
    else if(!proName.isEmpty())
        proName=QString(" (")+proName+QString("*)");

    if(ufwEnabled->isChecked())
        statusLabel->setText(0==ruleList->topLevelItemCount()
                                ? i18n("Firewall is enabled, and there are no rules defined.%1", proName)
                                : i18np("Firewall is enabled, and there is 1 rule defined.%2",
                                        "Firewall is enabled, and there are %1 rules defined.%2",
                                        ruleList->topLevelItemCount(), proName));
    else
        statusLabel->setText(0==ruleList->topLevelItemCount()
                                ? i18n("Firewall is currently disabled, and there are no rules defined.%1", proName)
                                : i18np("Firewall is currently disabled, and there is 1 rule defined.%2",
                                        "Firewall is currently disabled, and there are %1 rules defined.%2",
                                        ruleList->topLevelItemCount(), proName));
    ruleSelectionChanged();
}

void Kcm::setupWidgets()
{
    for(int i=Types::LOG_OFF; i<Types::LOG_COUNT; ++i)
        ufwLoggingLevel->insertItem(i, toString((Types::LogLevel)i, true));
    for(int i=Types::POLICY_ALLOW; i<Types::POLICY_COUNT_DEFAULT; ++i)
    {
        defaultOutgoingPolicy->insertItem(i, toString((Types::Policy)i, true));
        defaultIncomingPolicy->insertItem(i, toString((Types::Policy)i, true));
    }

    ufwLoggingLevel->setToolTip(Strings::logLevelInformation());
    defaultOutgoingPolicy->setToolTip(Strings::policyInformation(false));
    defaultIncomingPolicy->setToolTip(Strings::policyInformation(false));
    ruleList->setToolTip(Strings::ruleOrderInformation());
    ruleList->setColumnHidden(RulesList::COL_IPV6, true);

    connect(ufwEnabled, SIGNAL(toggled(bool)), SLOT(setStatus()));
    connect(ipv6Enabled, SIGNAL(toggled(bool)), SLOT(setIpV6()));
    connect(ufwLoggingLevel, SIGNAL(currentIndexChanged(int)), SLOT(setLogLevel()));
    connect(defaultIncomingPolicy, SIGNAL(currentIndexChanged(int)), SLOT(setDefaultIncomingPolicy()));
    connect(defaultOutgoingPolicy, SIGNAL(currentIndexChanged(int)), SLOT(setDefaultOutgoingPolicy()));
    connect(addRuleButton, SIGNAL(clicked(bool)), SLOT(createRules()));
    connect(editRuleButton, SIGNAL(clicked(bool)), SLOT(editRule()));
    connect(removeRuleButton, SIGNAL(clicked(bool)), SLOT(removeRule()));
    connect(moveRuleUpButton, SIGNAL(clicked(bool)), SLOT(moveRuleUp()));
    connect(moveRuleDownButton, SIGNAL(clicked(bool)), SLOT(moveRuleDown()));
    connect(refreshButton, SIGNAL(clicked(bool)), SLOT(queryStatus()));
    connect(logButton, SIGNAL(clicked(bool)), SLOT(displayLog()));
    connect(ruleList, SIGNAL(itemSelectionChanged()), SLOT(ruleSelectionChanged()));
    connect(ruleList, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), SLOT(ruleDoubleClicked(QTreeWidgetItem *, int)));
    connect(modulesList, SIGNAL(itemClicked(QTreeWidgetItem *, int)), SLOT(moduleClicked(QTreeWidgetItem *, int)));
    addRuleButton->setIcon(KIcon("list-add"));
    editRuleButton->setIcon(KIcon("document-edit"));
    removeRuleButton->setIcon(KIcon("list-remove"));
    moveRuleUpButton->setIcon(KIcon("arrow-up"));
    moveRuleDownButton->setIcon(KIcon("arrow-down"));
    refreshButton->setIcon(KIcon("view-refresh"));
    profilesButton->setIcon(KIcon("document-multiple"));
    logButton->setIcon(KIcon("text-x-log"));

    QMenu *profilesMenu=new QMenu(this);
    noProfilesAction=new QAction(i18n("No Saved Profiles"), this);
    noProfilesAction->setEnabled(false);
    profilesMenu->addAction(KIcon("document-save"), i18n("Save Current Settings..."), this, SLOT(saveProfile()));
    loadMenu=profilesMenu->addMenu(KIcon("document-open"), i18n("Load Profile"));
    deleteMenu=profilesMenu->addMenu(KIcon("edit-delete"), i18n("Delete Profile"));
    profilesMenu->addAction(KIcon("document-import"), i18n("Import..."), this, SLOT(importProfile()));
    profilesMenu->addAction(KIcon("document-export"), i18n("Export..."), this, SLOT(exportProfile()));
    profilesButton->setMenu(profilesMenu);
    ruleList->setDragEnabled(true);
    ruleList->viewport()->setAcceptDrops(true);
    ruleList->setDropIndicatorShown(true);
    ruleList->setDragDropMode(QAbstractItemView::InternalMove);
    listUserProfiles();
    profilesButton->setPopupMode(QToolButton::InstantPopup);

    connect(loadMenu, SIGNAL(triggered(QAction *)), SLOT(loadProfile(QAction *)));
    connect(deleteMenu, SIGNAL(triggered(QAction *)), SLOT(removeProfile(QAction *)));
    connect(loadMenu, SIGNAL(aboutToShow()), SLOT(loadMenuShown()));
    connect(deleteMenu, SIGNAL(aboutToShow()), SLOT(deleteMenuShown()));

    connect(ruleList, SIGNAL(dropped(const QTreeWidgetItem *)), SLOT(moveTo(const QTreeWidgetItem *)));
    addModules();

    blocker=new Blocker(this);
    blocker->add(ufwEnabled);
    blocker->add(ipv6Enabled);
    blocker->add(ufwLoggingLevel);
    blocker->add(defaultIncomingPolicy);
    blocker->add(defaultOutgoingPolicy);
    blocker->add(addRuleButton);
    blocker->add(editRuleButton);
    blocker->add(removeRuleButton);
    blocker->add(moveRuleUpButton);
    blocker->add(moveRuleDownButton);
    blocker->add(refreshButton);
    blocker->add(profilesButton);
    blocker->add(logButton);
}

void Kcm::setupActions()
{
    queryAction=KAuth::Action("org.kde.ufw.query");
    queryAction.setHelperID("org.kde.ufw");
#if KDE_IS_VERSION(4, 5, 90)
    queryAction.setParentWidget(this);
#endif
//     queryAction.setExecutesAsync(true);
    connect(queryAction.watcher(), SIGNAL(actionPerformed(ActionReply)), SLOT(queryPerformed(ActionReply)));

    modifyAction=KAuth::Action("org.kde.ufw.modify");
    modifyAction.setHelperID("org.kde.ufw");
#if KDE_IS_VERSION(4, 5, 90)
    modifyAction.setParentWidget(this);
#endif
//     modifyAction.setExecutesAsync(true);
    connect(modifyAction.watcher(), SIGNAL(actionPerformed(ActionReply)), SLOT(modifyPerformed(ActionReply)));
}

void Kcm::addModules()
{
    QList<KernelModule> modules;

    modules.append(KernelModule(i18nc("kernel module name", "FTP"), "nf_conntrack_ftp", "nf_nat_ftp"));
    modules.append(KernelModule(i18nc("kernel module name", "IRC"), "nf_conntrack_irc", "nf_nat_irc"));
    modules.append(KernelModule(i18nc("kernel module name", "NetBIOS"), "nf_conntrack_netbios_ns", QString(), "Samba"));
    modules.append(KernelModule(i18nc("kernel module name", "PPTP"), "nf_conntrack_pptp", "nf_nat_pptp", "VPN"));
    modules.append(KernelModule(i18nc("kernel module name", "SANE"), "nf_conntrack_sane"));
    modules.append(KernelModule(i18nc("kernel module name", "SIP"), "nf_conntrack_sip", "nf_nat_sip"));

    addOtherModules(modules);

    qSort(modules);

    QList<KernelModule>::ConstIterator it(modules.constBegin()),
                                       end(modules.constEnd());

    for(; it!=end; ++it)
        addModule(modulesList, *it);

    modulesList->header()->resizeSections(QHeaderView::ResizeToContents);
    modulesList->sortItems(MOD_COL_NAME, Qt::AscendingOrder);
    QTreeWidgetItem *headerItem=modulesList->headerItem();
    if(headerItem)
    {
        headerItem->setToolTip(MOD_COL_CONN_TRACK,
                               i18n("<p><i>Connection tracking</i> is the ability to maintain connection state "
                                    "information (such as source and destination address/port, protocol, etc.)  "
                                    "in memory.</p>"
                                    "<p>Using these modules makes the firewall more secure.</p>"));

        headerItem->setToolTip(MOD_COL_NAT,
                               i18n("<p><i>NAT (Network Address Translation)</i>. "
                                    "Where any form of NAT (SNAT, DNAT, Masquerading) on your firewall is involved, "
                                    "some commands and responses may also need to be modified by the firewall. This "
                                    "is the job of the NAT modules.</p>"
                                    "<p><b>NOTE:</b>If you are using the corresponding <i>Connection Tracking</i> "
                                    "module, then you should also enable the <i>NAT</i> module.</p>"));
    }
}

void Kcm::setStatus(const Profile &profile)
{
    if(!profile.hasStatus())
        return;

    if(ufwEnabled->isChecked()!=profile.getEnabled())
    {
        ufwEnabled->blockSignals(true);
        ufwEnabled->setChecked(profile.getEnabled());
        ufwEnabled->blockSignals(false);
    }
    configBox->setStatus(profile.getEnabled());
}

void Kcm::setDefaults(const Profile &profile)
{
    if(!profile.hasDefaults())
        return;

    if(profile.getIpv6Enabled()!=ipv6Enabled->isChecked())
    {
        ipv6Enabled->blockSignals(true);
        ipv6Enabled->setChecked(profile.getIpv6Enabled());
        ipv6Enabled->blockSignals(false);
    }
    if(ruleList->isColumnHidden(RulesList::COL_IPV6)==profile.getIpv6Enabled())
    {
        ruleList->setColumnHidden(RulesList::COL_IPV6, !profile.getIpv6Enabled());
        if(profile.getIpv6Enabled())
            ruleList->resizeColumnToContents(RulesList::COL_IPV6);
    }
    if(ufwLoggingLevel->currentIndex()!=(int)profile.getLogLevel())
    {
        ufwLoggingLevel->blockSignals(true);
        ufwLoggingLevel->setCurrentIndex(profile.getLogLevel());
        ufwLoggingLevel->blockSignals(false);
    }
    if(defaultOutgoingPolicy->currentIndex()!=(int)profile.getDefaultOutgoingPolicy())
    {
        defaultOutgoingPolicy->blockSignals(true);
        defaultOutgoingPolicy->setCurrentIndex(profile.getDefaultOutgoingPolicy());
        defaultOutgoingPolicy->blockSignals(false);
    }
    if(defaultIncomingPolicy->currentIndex()!=(int)profile.getDefaultIncomingPolicy())
    {
        defaultIncomingPolicy->blockSignals(true);
        defaultIncomingPolicy->setCurrentIndex(profile.getDefaultIncomingPolicy());
        defaultIncomingPolicy->blockSignals(false);
    }
}

void Kcm::setModules(const Profile &profile)
{
    if(!profile.hasModules())
        return;

    QSet<QString> mods=profile.getModules();

    modulesList->blockSignals(true);
    for(int i=0; i<modulesList->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item=modulesList->topLevelItem(i);

        if(item)
        {
            QString connTrack=item->data(MOD_COL_CONN_TRACK, Qt::UserRole).toString(),
                    nat=item->data(MOD_COL_NAT, Qt::UserRole).toString();
            int     state=0;

            if(!connTrack.isEmpty())
            {
                bool checked=mods.contains(connTrack);
                item->setCheckState(MOD_COL_CONN_TRACK, checked ? Qt::Checked : Qt::Unchecked);
                if(checked)
                {
                    mods.remove(connTrack);
                    state+=MOD_STATE_CONN_TRACK_ENABLED;
                }
            }
            if(!nat.isEmpty())
            {
                bool checked=mods.contains(nat);
                item->setCheckState(MOD_COL_NAT, checked ? Qt::Checked : Qt::Unchecked);
                if(checked)
                {
                    mods.remove(nat);
                    state+=MOD_STATE_NAT_ENABLED;
                }
            }
            item->setData(0, Qt::UserRole, state);
        }
    }

    // Store list of other IP tables modules that are not explicitly handled - that way if we make modifications,
    // they will also be added.
    otherModules=mods;
    modulesList->blockSignals(false);
}

void Kcm::setRules(const Profile &profile)
{
    if(!profile.hasRules())
        return;

    // First off all, save previous list state - so that this can be restored afterwards.
    // This helps to hide the fact that the list is regenerated.
    unsigned int prevCount=ruleList->topLevelItemCount(),
                 prevSelected=0,
                 prevTop=0;
    bool         hadSelectedItem=false;

    if(prevCount>0)
    {
        if(moveToPos)
        {
            prevSelected=moveToPos;
            hadSelectedItem=true;
        }
        else
        {
            QList<QTreeWidgetItem*> selectedItems=ruleList->selectedItems();

            if(1==selectedItems.count())
            {
                prevSelected=selectedItems.first()->data(0, Qt::UserRole).toUInt();
                hadSelectedItem=true;
            }
        }

        QTreeWidgetItem *topItem=ruleList->itemAt(QPoint(0, 0));

        if(topItem)
            prevTop=topItem->data(0, Qt::UserRole).toUInt();
    }

    ruleList->clear();
    currentRules=profile.getRules();

    if(currentRules.count()>0)
    {
        QTreeWidgetItem            *topItem=0L,
                                   *selectedItem=0L;
        unsigned int               index=0;
        QList<Rule>::ConstIterator it(currentRules.constBegin()),
                                   end(currentRules.constEnd());

        for(; it!=end; ++it)
        {
            QTreeWidgetItem *item=ruleList->insert(*it);

            item->setData(0, Qt::UserRole, ++index);

            if(prevTop>0 && index==prevTop)
                topItem=item;
            if(prevCount>0 && hadSelectedItem)
                if(index<=prevSelected)
                    selectedItem=item;
        }

        ruleList->resizeToContents();

        // Restore top and selected items
        if(topItem)
            ruleList->scrollToItem(topItem);

        if(selectedItem)
            selectedItem->setSelected(true);
    }
}

QSet<QString> Kcm::modules()
{
    QSet<QString> mods;

    for(int i=0; i<modulesList->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item=modulesList->topLevelItem(i);

        if(item)
        {
            QString connTrack=item->data(MOD_COL_CONN_TRACK, Qt::UserRole).toString(),
                    nat=item->data(MOD_COL_NAT, Qt::UserRole).toString();

            if(!connTrack.isEmpty() && Qt::Checked==item->checkState(MOD_COL_CONN_TRACK))
                mods.insert(connTrack);
            if(!nat.isEmpty() && Qt::Checked==item->checkState(MOD_COL_NAT))
                mods.insert(nat);
        }
    }

    // Add other modules not handled by this KCM - otherwise they will be removed!!!
    QSet<QString>::ConstIterator it(otherModules.constBegin()),
                                 end(otherModules.constEnd());

    for(; it!=end; ++it)
        mods.insert(*it);
    return mods;
}

}

#include "kcm.moc"
