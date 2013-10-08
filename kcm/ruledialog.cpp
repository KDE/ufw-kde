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

#include "ruledialog.h"
#include "appprofiles.h"
#include "types.h"
#include "strings.h"
#include "combobox.h"
#include <limits.h>
#include <KDE/KConfigGroup>
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KIcon>
#include <KDE/KMessageBox>
#include <QtGui/QValidator>
#include <QtGui/QButtonGroup>
#include <QtNetwork/QNetworkInterface>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

namespace UFW
{

static const int constPartStart=1<<16;

enum RuleType
{
    RT_SIMPLE,
    RT_ADVANCED
};

enum Direction
{
    DIR_IN,
    DIR_OUT
};

struct PredefinedPort
{
    PredefinedPort(Types::PredefinedPort v) : val(v), str(Types::toString(v, true)) { }
    bool operator<(const PredefinedPort &o) const                                   { return str.localeAwareCompare(o.str)<0; }
    Types::PredefinedPort val;
    QString               str;
};

static void addRuleTypes(QComboBox *combo)
{
    combo->insertItem(RT_SIMPLE, i18n("Simple"));
    combo->insertItem(RT_ADVANCED, i18n("Advanced"));
}

static void addPolicies(QComboBox *combo)
{
    for(int i=0; i<Types::POLICY_COUNT; ++i)
        combo->insertItem(i, Types::toString((Types::Policy)i, true));
}

static void addDirections(QComboBox *combo)
{
    combo->insertItem(DIR_IN, i18n("Incoming"));
    combo->insertItem(DIR_OUT, i18n("Outgoing"));
}

static void addLogging(QComboBox *combo)
{
    for(int i=0; i<Types::LOGGING_COUNT; ++i)
        combo->insertItem(i, Types::toString((Types::Logging)i, true));
}

static void addProtocols(QComboBox *combo)
{
    for(int i=0; i<Types::PROTO_COUNT; ++i)
        combo->insertItem(i, Types::toString((Types::Protocol)i, true));
}

static void addPredefinedPorts(QComboBox *combo, QList<PredefinedPort> &sortedPp, QMap<int, int> &map, bool splitMulti)
{
    map.clear();
    QList<PredefinedPort>::ConstIterator it(sortedPp.constBegin()), 
                                         end(sortedPp.constEnd());

    for(int index=combo->count(); it!=end; ++it)
    {
        if(!AppProfiles::get().contains((*it).str) && !AppProfiles::get().contains((*it).str.toUpper()) && !AppProfiles::get().contains((*it).str.toLower()))
        {
            QString ports=Types::toString((*it).val, false);
            bool    isMulti=ports.contains(" ");
            
            if(isMulti && splitMulti)
            {
                QStringList                split(ports.split(" "));
                QStringList::ConstIterator sIt(split.constBegin()),
                                           sEnd(split.constEnd());
                                           
                for(int part=1; sIt!=sEnd; ++sIt, ++part)
                {
                    combo->insertItem(index, (*it).str+QLatin1String(" (")+(*sIt)+QChar(')'));
                    map[index++]=(*it).val+(part<<16);
                }
            }
            else
            {
                combo->insertItem(index, (*it).str+QLatin1String(" (")+ports+QChar(')'));
                map[index++]=(*it).val;
            }
        }
    }
}

static void getPredefinedPortAndProtocol(QMap<int, int> &map, int index, QString &port, Types::Protocol &prot)
{
    int                        value=map[index],
                               part=(value&0xFFFF0000)>>16,
                               entry=value&0xFFFF;
    QStringList                ports(Types::toString((Types::PredefinedPort)entry).split(" "));
    QStringList::ConstIterator it(ports.begin()),
                               end(ports.end());

    for(int p=1; it!=end; ++it, ++p)
        if(0==part || p==part)
        {
            port=*it;
            prot=Types::PROTO_BOTH;

            for(int i=0; i<Types::PROTO_COUNT; ++i)
            {
                QString p(QChar('/')+toString((Types::Protocol)i));
                if(port.endsWith(p))
                {
                    prot=(Types::Protocol)i;
                    port.replace(p, "");
                    break;
                }
            }
        }
}

static void addProfiles(QComboBox *combo)
{
    int index=0;

    QList<AppProfiles::Entry>                profiles(AppProfiles::get());
    QList<AppProfiles::Entry>::ConstIterator it(profiles.constBegin()),
                                             end(profiles.constEnd());

    for(; it!=end; ++it)
        combo->insertItem(index++, (*it).name+QLatin1String(" (")+(*it).ports+QChar(')'));
}

static QString getProfileName(const QString &text)
{
    int index=text.indexOf(" (");
    return -1==index ? text : text.left(index);
}

static QString getProfilesPorts(const QString &text)
{
    int start=text.indexOf('('),
        end=text.indexOf(')');

    return -1==start || -1==end ? text : text.mid(start+1, (end-(start+1)));
}

static void setRulePort(const QString &port, Types::Protocol prot, QRadioButton *portRadio, QRadioButton *profileRadio, 
                        QRadioButton *anyRadio, QComboBox *profileCombo)
{
    if(anyRadio && port.isEmpty())
        anyRadio->setChecked(true);
    else
        portRadio->setChecked(true);

    if(port.isEmpty())
        return;
    
    QString portAndProt=port+Rule::protocolSuffix(prot);
    int     index=0,
            numItems=profileCombo->count();

    for(; index<numItems; ++index)
        if(getProfilesPorts(profileCombo->itemText(index))==portAndProt)
        {
            profileRadio->setChecked(true);
            profileCombo->setCurrentIndex(index);
            return;
        }
}

static void addInterfaces(QComboBox *combo)
{
    QList<QNetworkInterface>                interfaces(QNetworkInterface::allInterfaces());
    QList<QNetworkInterface>::ConstIterator it(interfaces.constBegin()),
                                            end(interfaces.constEnd());

    combo->insertItem(0, i18n("Any interface"));
    for (; it!=end; ++it)
        combo->insertItem(combo->count(), it->name());
}

static QString portInformation()
{
    return i18n("<p>Enter one or more port numbers (e.g. <i>20,22</i>), "
                "a service name (e.g. <i>ssh</i>), "
                "or a port range (e.g. use <i>20:30</i> for ports 20 to 30)</p>");
}

static QString hostInformation()
{
    return i18n("Enter an IP address (e.g. 192.168.1.100), or subnet (e.g. 192.168.1.0/24)");
}

static QString ifaceInformation()
{
    return i18n("Enter a network interface (e.g. eth0), or leave blank to apply to all interfaces");
}

#define CFG_GROUP     isEdit ? "KCM_UFW_EditRuleDialog" : "KCM_UFW_RuleDialog"
#define CFG_RULE_TYPE "RuleType"
#define CFG_SIZE      "Size"

class PortValidator : public QValidator
{
    public:

    PortValidator(QObject *parent) : QValidator(parent) { }

    State validate(QString &input, int &) const
    {
        int colonCount(0);

        for(int i=0; i<input.length(); ++i)
            if(!input[i].isLetterOrNumber() && QChar(':')!=input[i] && QChar(',')!=input[i])
                return Invalid;
            else if(QChar(':')==input[i])
                if(++colonCount>1)
                    return Invalid;
        if(input.contains(",,"))
            return Invalid;
        if(input.endsWith(':'))
            return Intermediate;
        return Acceptable;
    }
};

class IpAddressValidator : public QValidator
{
    public:

    IpAddressValidator(RuleDialog *parent)
        : QValidator(parent)
        , dlg(parent)
    {
        validV4Chars << '0' << '1' << '2' << '3' << '4' << '5' << '6' << '7' << '8' << '9';
        validV6Chars=validV4Chars;
        validV6Chars << 'A' << 'a' << 'B' << 'b' << 'C' << 'c' << 'D' << 'd' << 'E' << 'e' << 'F' << 'f'
                     /*<< '.' << ':' << '/'*/;
    }

    State validate(QString &input, int &) const
    {
        int  dotCount(0),
             colonCount(0),
             slashPos(-1);
        bool allowV6(dlg->ipV6Enabled());

        for(int i=0; i<input.length(); ++i)
            if(QChar('.')==input[i])
            {
                // Cant have '.' have '/', or have more than 3 periods!
                if(slashPos>-1 || ++dotCount>3)
                    return Invalid;
            }
            else if(allowV6 && QChar(':')==input[i])
            {
                // Can't have ':' after '/', or more than 7 colons!
                if(slashPos>-1 || ++colonCount>7)
                    return Invalid;
            }
            else if(QChar('/')==input[i])
            {
                // Can't have two or more slashes!
                if(slashPos>-1)
                    return Invalid;
                slashPos=i;
            }
            else if((allowV6 && !validV6Chars.contains(input[i])) ||
                    (!allowV6 && !validV4Chars.contains(input[i])))
                return Invalid;

        // IPv4 cant have just dots
        if(input.contains(".."))
             return Invalid;
        // Can't mix IPv4 and IPv6
        if(input.contains(".") && input.contains(":"))
            return Invalid;

        if(input.endsWith('.') || input.endsWith('/'))
            return Intermediate;

        return Acceptable;
    }

    private:

    QSet<QChar> validV4Chars,
                validV6Chars;
    RuleDialog  *dlg;
};

class IfaceValidator : public QValidator
{
    public:

    IfaceValidator(QObject *parent) : QValidator(parent) { }

    State validate(QString &input, int &) const
    {
        for(int i=0; i<input.length(); ++i)
            if(!input[i].isLetterOrNumber())
                return Invalid;

        return Acceptable;
    }
};

static bool isV6Address(const QString &str)
{
    return str.contains(':');
}

static bool checkAddress(const QString &str)
{
    if(str.isEmpty())
        return true;

    if(str.startsWith('/') || str.startsWith('.') || str.endsWith('/') || str.endsWith('.'))
        return false;

    int numDots=str.count(QChar('.')),
        numColons=str.count(QChar(':'));

    if(numDots>0 && (3!=numDots || numColons>0))
        return false;

    QByteArray    addr(str.contains('/')
                        ? str.split('/').first().toLatin1()
                        : str.toLatin1());
    unsigned char dest[16];

    return inet_pton(numColons ? AF_INET6 : AF_INET, addr.constData(), dest)>0;
}

static bool checkPort(const QString &str)
{
    if(str.isEmpty())
        return true;

    if(str.startsWith(':') || str.startsWith(',') || str.endsWith(':') || str.endsWith(','))
        return false;

    bool                       range(-1!=str.indexOf(':'));
    QStringList                parts(str.split(QRegExp("(:|,)")));
    QStringList::ConstIterator it(parts.constBegin()),
                               end(parts.constEnd());

    for(; it!=end; ++it)
    {
        bool ok;

        (*it).toUShort(&ok);

        if(!ok && (range || 0==Rule::getServicePort(*it)))
            return false;
    }

    return true;
}

static void setProfileIndex(QComboBox *combo, const QString &str)
{
    if(str.isEmpty())
        return;

    int  index=0,
         numItems=combo->count();
    bool haveSep=false;

    for(; index<numItems; ++index)
        if(getProfileName(combo->itemText(index))==str)
        {
            combo->setCurrentIndex(index);
            return;
        }
        else if(combo->itemText(index).isEmpty())
            haveSep=true;
        
    if(!haveSep)
        combo->insertSeparator(index++);
    combo->insertItem(index, str);
    combo->setCurrentIndex(index);
}

RuleDialog::RuleDialog(Kcm *parent, bool isEditDlg)
          : KDialog(parent)
          , kcm(parent)
          , isEdit(isEditDlg)
{
    QWidget *mainWidet=new QWidget(this);
    if(isEdit)
    {
        setButtons(Help|Ok|Cancel);
        setCaption(i18n("Edit Rule"));
        setHelp("add_and_edit_rules", "ufw");
    }
    else
    {
        setButtons(Help|Apply|Close);
        setButtonText(Apply, i18n("Add"));
        setButtonIcon(Apply, KIcon("list-add"));
        setCaption(i18n("Add Rule"));
        setHelp("add_and_edit_rules", "ufw");
    }
    setupUi(mainWidet);
    setMainWidget(mainWidet);

    addRuleTypes(ruleType);
    addPolicies(simplePolicy);
    addDirections(simpleDirection);
    addLogging(simpleLogging);
    addProtocols(simpleProtocol);
    addPolicies(advancedPolicy);
    addDirections(advancedDirection);
    addLogging(advancedLogging);
    addProtocols(advancedProtocol);
    addInterfaces(advancedInterface);
    if(AppProfiles::get().count())
    {
        addProfiles(simpleProfile);
        simpleProfile->insertSeparator(simpleProfile->count());
        addProfiles(advancedSrcProfile);
        advancedSrcProfile->insertSeparator(advancedSrcProfile->count());
        addProfiles(advancedDestProfile);
        advancedDestProfile->insertSeparator(advancedDestProfile->count());
    }
    else
    {
        simpleProfileRadio->setVisible(false);
        simpleProfile->setVisible(false);
        simplePortRadio->setVisible(false);
        advancedSrcProfileRadio->setVisible(false);
        advancedSrcProfile->setVisible(false);
        advancedSrcPortRadio->setVisible(false);
        advancedDestProfileRadio->setVisible(false);
        advancedDestProfile->setVisible(false);
        advancedDestPortRadio->setVisible(false);
        advancedSrcProfileRadio->setEnabled(false);
        advancedDestProfileRadio->setEnabled(false);
    }

    QList<PredefinedPort> sortedPorts;
    
    for(int i=0; i<Types::PP_COUNT; ++i)
        sortedPorts.append(PredefinedPort((Types::PredefinedPort)i));
    qSort(sortedPorts);

    addPredefinedPorts(simpleProfile, sortedPorts, simpleIndexToPredefinedPort, isEdit);
    addPredefinedPorts(advancedDestProfile, sortedPorts, advancedIndexToPredefinedPort, true);
    addPredefinedPorts(advancedSrcProfile, sortedPorts, advancedIndexToPredefinedPort, true);
    
    simpleProtocol->setCurrentIndex(Types::PROTO_BOTH);
    advancedProtocol->setCurrentIndex(Types::PROTO_BOTH);

    controlSimpleProtocol();

    KConfigGroup grp(KGlobal::config(), CFG_GROUP);
    int          rt=grp.readEntry(CFG_RULE_TYPE, (int)0);

    ruleType->setCurrentIndex(RT_SIMPLE==rt || RT_ADVANCED==rt ? rt : RT_SIMPLE);
    setRuleType();

    simplePort->setValidator(new PortValidator(this));
    advancedDestPort->setValidator(new PortValidator(this));
    advancedSrcPort->setValidator(new PortValidator(this));
    advancedDestHost->setValidator(new IpAddressValidator(this));
    advancedSrcHost->setValidator(new IpAddressValidator(this));
    advancedInterface->setEditable(true);
    advancedInterface->setEditText(advancedInterface->itemText(0));
    advancedInterface->setValidator(new IfaceValidator(this));

    simplePolicy->setToolTip(Strings::policyInformation());
    advancedPolicy->setToolTip(Strings::policyInformation());

    simpleLogging->setToolTip(Strings::loggingInformation());
    advancedLogging->setToolTip(Strings::loggingInformation());

    simplePort->setToolTip(portInformation());
    advancedSrcPort->setToolTip(portInformation());
    advancedDestPort->setToolTip(portInformation());

    advancedSrcHost->setToolTip(hostInformation());
    advancedDestHost->setToolTip(hostInformation());

    advancedInterface->setToolTip(ifaceInformation());

    simplePort->setRadio(simplePortRadio);
    simpleProfile->setRadio(simpleProfileRadio);
    advancedSrcHost->setRadio(advancedSrcHostRadio);
    advancedSrcPort->setRadio(advancedSrcPortRadio);
    advancedSrcProfile->setRadio(advancedSrcProfileRadio);
    advancedDestHost->setRadio(advancedDestHostRadio);
    advancedDestPort->setRadio(advancedDestPortRadio);
    advancedDestProfile->setRadio(advancedDestProfileRadio);

    QButtonGroup *advancedSrcHostGroup=new QButtonGroup(this),
                 *advancedSrcPortGroup=new QButtonGroup(this),
                 *advancedDestHostGroup=new QButtonGroup(this),
                 *advancedDestPortGroup=new QButtonGroup(this);
                 
    advancedSrcHostGroup->addButton(advancedSrcHostRadio);
    advancedSrcHostGroup->addButton(advancedSrcAnyHostRadio);
    advancedSrcPortGroup->addButton(advancedSrcPortRadio);
    advancedSrcPortGroup->addButton(advancedSrcProfileRadio);
    advancedSrcPortGroup->addButton(advancedSrcAnyPortRadio);
    
    advancedDestHostGroup->addButton(advancedDestHostRadio);
    advancedDestHostGroup->addButton(advancedDestAnyHostRadio);
    advancedDestPortGroup->addButton(advancedDestPortRadio);
    advancedDestPortGroup->addButton(advancedDestProfileRadio);
    advancedDestPortGroup->addButton(advancedDestAnyPortRadio);

    connect(simplePolicy, SIGNAL(currentIndexChanged(int)), advancedPolicy, SLOT(setCurrentIndex(int)));
    connect(advancedPolicy, SIGNAL(currentIndexChanged(int)), simplePolicy, SLOT(setCurrentIndex(int)));

    connect(simpleDirection, SIGNAL(currentIndexChanged(int)), advancedDirection, SLOT(setCurrentIndex(int)));
    connect(advancedDirection, SIGNAL(currentIndexChanged(int)), simpleDirection, SLOT(setCurrentIndex(int)));

    connect(simpleLogging, SIGNAL(currentIndexChanged(int)), advancedLogging, SLOT(setCurrentIndex(int)));
    connect(advancedLogging, SIGNAL(currentIndexChanged(int)), simpleLogging, SLOT(setCurrentIndex(int)));

    connect(simpleDescription, SIGNAL(textEdited(const QString &)), advancedDescription, SLOT(setText(const QString &)));
    connect(advancedDescription, SIGNAL(textEdited(const QString &)), simpleDescription, SLOT(setText(const QString &)));
    
    connect(simpleProfileRadio, SIGNAL(toggled(bool)), SLOT(controlSimpleProtocol()));
    connect(advancedSrcAnyPortRadio, SIGNAL(toggled(bool)), SLOT(controlAdvancedProtocol()));
    connect(advancedDestAnyPortRadio, SIGNAL(toggled(bool)), SLOT(controlAdvancedProtocol()));
    connect(advancedSrcProfileRadio, SIGNAL(toggled(bool)), SLOT(controlAdvancedProtocol()));
    connect(advancedDestProfileRadio, SIGNAL(toggled(bool)), SLOT(controlAdvancedProtocol()));
//
//     connect(simplePort, SIGNAL(textChanged(const QString &)), advancedDestPort, SLOT(setText(const QString &)));
//     connect(advancedDestPort, SIGNAL(textChanged(const QString &)), simplePort, SLOT(setText(const QString &)));
    connect(ruleType, SIGNAL(currentIndexChanged(int)), SLOT(setRuleType()));
    if(isEdit)
        connect(this, SIGNAL(okClicked()), SLOT(update()));
    else
        connect(this, SIGNAL(applyClicked()), SLOT(update()));

    connect(kcm, SIGNAL(error(const QString &)), SLOT(showError(const QString &)));
    connect(kcm, SIGNAL(status(const QString &)), statusLabel, SLOT(setText(const QString &)));

    QSize sz=grp.readEntry(CFG_SIZE, QSize(160, 240));

    if(sz.isValid())
        resize(sz);

    advancedPolicy->adjustSize();
    simplePolicy->adjustSize();
    int width=advancedPolicy->size().width()<simplePolicy->size().width()
                ? simplePolicy->size().width() : advancedPolicy->size().width();

    simplePolicy->setMinimumWidth(width);
    simplePolicy->setMaximumWidth(width);
    advancedPolicy->setMinimumWidth(width);
    advancedPolicy->setMaximumWidth(width);
    
    simpleProfile->adjustSize();
    simplePort->setMinimumWidth(simpleProfile->size().width());
}

RuleDialog::~RuleDialog()
{
    KConfigGroup grp(KGlobal::config(), CFG_GROUP);

    grp.writeEntry(CFG_RULE_TYPE, ruleType->currentIndex());
    grp.writeEntry(CFG_SIZE, size());
}

void RuleDialog::reset()
{
    simpleDescription->setText(QString());
    advancedDescription->setText(QString());
    simpleDescription->setText(QString());
    simplePolicy->setCurrentIndex(Types::POLICY_DENY);
    simpleDirection->setCurrentIndex(DIR_IN);
    simplePort->setText(QString());
    simpleProtocol->setCurrentIndex(Types::PROTO_BOTH);
    simpleLogging->setCurrentIndex(Types::LOGGING_OFF);
    advancedPolicy->setCurrentIndex(Types::POLICY_DENY);
    advancedDirection->setCurrentIndex(DIR_IN);
    advancedSrcHost->setText(QString());
    advancedSrcPort->setText(QString());
    advancedDestHost->setText(QString());
    advancedDestPort->setText(QString());
    advancedProtocol->setCurrentIndex(Types::PROTO_BOTH);
    advancedLogging->setCurrentIndex(Types::LOGGING_OFF);
    advancedInterface->setEditText(advancedInterface->itemText(0));
    simpleProfileRadio->setChecked(true);
    advancedSrcAnyPortRadio->setChecked(true);
    advancedDestAnyPortRadio->setChecked(true);
    advancedSrcAnyHostRadio->setChecked(true);
    advancedDestAnyHostRadio->setChecked(true);
    controlSimpleProtocol();
    controlAdvancedProtocol();
    
    if(RT_SIMPLE==ruleType->currentIndex())
        simplePolicy->setFocus();
    else
        advancedPolicy->setFocus();
}

void RuleDialog::setRule(const Rule &rule)
{
    editingRule=rule;
    simpleDescription->setText(rule.getDescription());
    advancedDescription->setText(rule.getDescription());
    simpleDescription->setText(rule.getDescription());
    simplePolicy->setCurrentIndex(rule.getAction());
    simpleDirection->setCurrentIndex(rule.getIncoming() ? DIR_IN : DIR_OUT);
    simplePort->setText(rule.getDestPort());
    simpleProtocol->setCurrentIndex(rule.getProtocol());
    simpleLogging->setCurrentIndex(rule.getLogging());

    advancedPolicy->setCurrentIndex(rule.getAction());
    advancedDirection->setCurrentIndex(rule.getIncoming() ? DIR_IN : DIR_OUT);
    advancedSrcHost->setText(rule.getSourceAddress());
    advancedSrcPort->setText(rule.getSourcePort());
    advancedDestHost->setText(rule.getDestAddress());
    advancedDestPort->setText(rule.getDestPort());
    advancedProtocol->setCurrentIndex(rule.getProtocol());
    advancedLogging->setCurrentIndex(rule.getLogging());
    
    QString iface(rule.getIncoming() ? rule.getInterfaceIn() : rule.getInterfaceOut());
    advancedInterface->setEditText(iface.isEmpty() ? advancedInterface->itemText(0) : iface);

    if(rule.getDestApplication().isEmpty())
    {
        setRulePort(rule.getDestPort(), rule.getProtocol(), simplePortRadio, simpleProfileRadio, NULL, simpleProfile);
        setRulePort(rule.getDestPort(), rule.getProtocol(), advancedDestPortRadio, advancedDestProfileRadio,
                    advancedDestAnyPortRadio, advancedDestProfile);
    }
    else
    {
        simpleProfileRadio->setChecked(true);
        setProfileIndex(simpleProfile, rule.getDestApplication());
        advancedDestProfileRadio->setChecked(true);
        setProfileIndex(advancedDestProfile, rule.getDestApplication());
    }

    if(rule.getSourceApplication().isEmpty())
    {
        setRulePort(rule.getSourcePort(), rule.getProtocol(), advancedSrcPortRadio, advancedSrcProfileRadio,
                    advancedSrcAnyPortRadio, advancedSrcProfile);
    }
    else
    {
        advancedSrcProfileRadio->setChecked(true);
        setProfileIndex(advancedSrcProfile, rule.getSourceApplication());
    }

    ruleType->setCurrentIndex(!rule.getDestAddress().isEmpty() || !rule.getSourceAddress().isEmpty() ||
                              !rule.getSourceApplication().isEmpty() ||
                              !rule.getSourcePort().isEmpty() || !rule.getInterfaceIn().isEmpty() ||
                              !rule.getInterfaceOut().isEmpty() ? RT_ADVANCED : RT_SIMPLE);
    
    simpleProfileRadio->setVisible(simpleProfile->count());
    simpleProfile->setVisible(simpleProfile->count());
    simplePortRadio->setVisible(simpleProfile->count());
    advancedSrcProfileRadio->setVisible(advancedSrcProfile->count());
    advancedSrcProfile->setVisible(advancedSrcProfile->count());
    advancedSrcPortRadio->setVisible(advancedSrcProfile->count());
    advancedDestProfileRadio->setVisible(advancedDestProfile->count());
    advancedDestProfile->setVisible(advancedDestProfile->count());
    advancedDestPortRadio->setVisible(advancedDestProfile->count());
    
    advancedSrcAnyHostRadio->setChecked(rule.getSourceAddress().isEmpty());
    advancedDestAnyHostRadio->setChecked(rule.getDestAddress().isEmpty());
    advancedSrcHostRadio->setChecked(!rule.getSourceAddress().isEmpty());
    advancedDestHostRadio->setChecked(!rule.getDestAddress().isEmpty());

    controlAdvancedProtocol();
    controlSimpleProtocol();
        
    setRuleType();
    
    if(RT_SIMPLE==ruleType->currentIndex())
        simplePolicy->setFocus();
    else
        advancedPolicy->setFocus();
}

void RuleDialog::update()
{
    if(kcm->isActive())
        return;

    QList<Rule> rules;

    // Set blank fields to 'Any'...
    if(Types::PROTO_BOTH!=simpleProtocol->currentIndex() && simpleProfileRadio->isChecked())
        simpleProtocol->setCurrentIndex(Types::PROTO_BOTH);
    if(advancedSrcHostRadio->isChecked() && advancedSrcHost->text().isEmpty())
        advancedSrcAnyHostRadio->setChecked(true);
    if(advancedDestHostRadio->isChecked() && advancedDestHost->text().isEmpty())
        advancedDestAnyHostRadio->setChecked(true);
    if(advancedSrcPortRadio->isChecked() && advancedSrcPort->text().isEmpty())
        advancedSrcAnyPortRadio->setChecked(true);
    if(advancedDestPortRadio->isChecked() && advancedDestPort->text().isEmpty())
        advancedDestAnyPortRadio->setChecked(true);
    if(Types::PROTO_BOTH!=advancedProtocol->currentIndex() && !advancedDestPortRadio->isChecked() &&
       !advancedSrcPortRadio->isChecked())
        advancedProtocol->setCurrentIndex(Types::PROTO_BOTH);
    if(!advancedInterface->currentText().isEmpty() && advancedInterface->currentText()!=advancedInterface->itemText(0))
    {
        // For some reason, the Validator seems to stop working occasionaly!!! So, just check the contents here, and
        // remove invalid characters...
        QString iface(advancedInterface->currentText());
        
        for(int i=0; i<iface.length(); ++i)
            if(!iface[i].isLetterOrNumber())
                iface[i]=' ';
        advancedInterface->setEditText(iface.replace(" ", ""));
    }
    if(advancedInterface->currentText().isEmpty())
        advancedInterface->setEditText(advancedInterface->itemText(0));

    switch(ruleType->currentIndex())
    {
        default:
        case RT_SIMPLE:
            if(simpleProfileRadio->isChecked() && simpleIndexToPredefinedPort.contains(simpleProfile->currentIndex()))
            {
                if(simpleIndexToPredefinedPort[simpleProfile->currentIndex()]>=constPartStart)
                {
                    QString         port;
                    Types::Protocol protocol;

                    getPredefinedPortAndProtocol(simpleIndexToPredefinedPort, simpleProfile->currentIndex(), port, protocol);
                    rules.append(Rule((Types::Policy)simplePolicy->currentIndex(),
                                      DIR_IN==simpleDirection->currentIndex(),
                                      (Types::Logging)simpleLogging->currentIndex(),
                                      protocol, simpleDescription->text(), editingRule.getHash(),
                                      QString(), QString(), QString(), port));
                }
                else
                {
                    QStringList                ports(Types::toString((Types::PredefinedPort)simpleIndexToPredefinedPort[simpleProfile->currentIndex()])
                                                                    .split(" "));
                    QStringList::ConstIterator it(ports.begin()),
                                               end(ports.end());

                    for(int p=1; it!=end; ++it, ++p)
                    {
                        QString         port(*it);
                        Types::Protocol protocol=Types::PROTO_BOTH;

                        for(int i=0; i<Types::PROTO_COUNT; ++i)
                        {
                            QString p(QChar('/')+toString((Types::Protocol)i));
                            if(port.endsWith(p))
                            {
                                protocol=(Types::Protocol)i;
                                port.replace(p, "");
                                break;
                            }
                        }
                        rules.append(Rule((Types::Policy)simplePolicy->currentIndex(),
                                        DIR_IN==simpleDirection->currentIndex(),
                                        (Types::Logging)simpleLogging->currentIndex(),
                                        protocol, simpleDescription->text(), editingRule.getHash(),
                                        QString(), QString(), QString(), port));
                    }
                }
            }
            else
            {
                QString port=simplePortRadio->isChecked() ? simplePort->text() : QString(),
                        app=simpleProfileRadio->isChecked() ? getProfileName(simpleProfile->currentText()) : QString();

                if(port.isEmpty() && app.isEmpty())
                    KMessageBox::error(this, i18n("No port defined."));
                else if(!port.isEmpty() && !checkPort(port))
                    KMessageBox::error(this, i18n("Invalid port."));
                else if(!port.isEmpty() && (port.contains(":") || port.contains(",")) && Types::PROTO_BOTH==simpleProtocol->currentIndex())
                    KMessageBox::error(this, i18n("Port ranges can only be used when either TCP or UDP are explicitly selected."));
                else
                    rules.append(Rule((Types::Policy)simplePolicy->currentIndex(),
                                    DIR_IN==simpleDirection->currentIndex(),
                                    (Types::Logging)simpleLogging->currentIndex(),
                                    (Types::Protocol)simpleProtocol->currentIndex(),
                                    simpleDescription->text(), editingRule.getHash(),
                                    QString(), QString(), QString(), port,
                                    QString(), QString(), QString(), app));
            }
            break;
        case RT_ADVANCED:
        {
            bool    srcIsPreDefined=advancedSrcProfileRadio->isChecked() && 
                                        advancedIndexToPredefinedPort.contains(advancedSrcProfile->currentIndex()),
                    destIsPreDefined=advancedDestProfileRadio->isChecked() && 
                                        advancedIndexToPredefinedPort.contains(advancedDestProfile->currentIndex());
            QString srcApp=!srcIsPreDefined && advancedSrcProfileRadio->isChecked() ? getProfileName(advancedSrcProfile->currentText()) : QString(),
                    destApp=!destIsPreDefined && advancedDestProfileRadio->isChecked() ? getProfileName(advancedDestProfile->currentText()) : QString(),
                    srcHost=advancedSrcAnyHostRadio->isChecked() ? QString() : advancedSrcHost->text(),
                    srcPort=!srcIsPreDefined && advancedSrcPortRadio->isChecked() ? advancedSrcPort->text() : QString(),
                    destHost=advancedDestAnyHostRadio->isChecked() ? QString() : advancedDestHost->text(),
                    destPort=!destIsPreDefined && advancedDestPortRadio->isChecked() ? advancedDestPort->text() : QString();
            Types::Protocol prot=(Types::Protocol)advancedProtocol->currentIndex(),
                            srcProt=Types::PROTO_BOTH,
                            destProt=Types::PROTO_BOTH;

            if(srcIsPreDefined)
            {
                getPredefinedPortAndProtocol(advancedIndexToPredefinedPort, advancedSrcProfile->currentIndex(), srcPort, srcProt);
                prot=srcProt;
            }
            if(destIsPreDefined)
            {
                getPredefinedPortAndProtocol(advancedIndexToPredefinedPort, advancedDestProfile->currentIndex(), destPort, destProt);
                prot=destProt;
            }
            if(!kcm->ipV6Enabled() && ((!destHost.isEmpty() && isV6Address(destHost)) || (!srcHost.isEmpty() && isV6Address(srcHost))))
                KMessageBox::error(this, i18n("You can only use IPv6 addresses, if IPv6 support is enabled."), i18n("IPv6 Support Disabled"));
            else if(destIsPreDefined && srcIsPreDefined && srcProt!=destProt)
                KMessageBox::error(this, i18n("Selected pre-defined ports for 'Source' and 'Destination' use different protocols."), i18n("Mixed Protocols"));
            else if(srcHost.isEmpty() && srcPort.isEmpty() && destHost.isEmpty() && destPort.isEmpty() &&
               srcApp.isEmpty() && destApp.isEmpty())
                KMessageBox::error(this, i18n("No hosts or ports defined."));
            else if(!checkAddress(srcHost))
                KMessageBox::error(this, i18n("Invalid 'Source' address."));
            else if(!checkAddress(destHost))
                KMessageBox::error(this, i18n("Invalid 'Destination' address."));
            else if(!srcIsPreDefined && !checkPort(srcPort))
                KMessageBox::error(this, i18n("Invalid 'Source' port."));
            else if(!destIsPreDefined && !checkPort(destPort))
                KMessageBox::error(this, i18n("Invalid 'Destination' port."));
            else if(Types::PROTO_BOTH==prot &&
                    (srcPort.contains(":") || destPort.contains(":") || srcPort.contains(",") || destPort.contains(",")))
                KMessageBox::error(this, i18n("Port ranges can only be used when either TCP or UDP are explicitly selected."));
            else
                rules.append(Rule((Types::Policy)advancedPolicy->currentIndex(),
                                  DIR_IN==advancedDirection->currentIndex(),
                                  (Types::Logging)advancedLogging->currentIndex(),
                                  prot,
                                  advancedDescription->text(), editingRule.getHash(),
                                  srcHost, srcPort, destHost, destPort,
                                  DIR_IN==advancedDirection->currentIndex() && 
                                  advancedInterface->currentText()!=advancedInterface->itemText(0)
                                        ? advancedInterface->currentText() : QString(),
                                  DIR_IN!=advancedDirection->currentIndex() && 
                                  advancedInterface->currentText()!=advancedInterface->itemText(0)
                                        ? advancedInterface->currentText() : QString(),
                                  srcApp, destApp));
        }
    }

    if(rules.count())
    {
        if(isEdit)
        {
            if(rules.first().onlyDescrChanged(editingRule))
                kcm->editRuleDescr(rules.first());
            else if(rules.first().different(editingRule))
                kcm->editRule(rules.first());
        }
        else if(!kcm->addRules(rules))
            KMessageBox::error(this, i18n("Rule already exists!"));
    }
}

void RuleDialog::setRuleType()
{
    switch(ruleType->currentIndex())
    {
        default:
        case RT_SIMPLE:
            stackedWidget->setCurrentIndex(RT_SIMPLE);
            simplePort->setText(advancedDestPort->text());
            break;
        case RT_ADVANCED:
            stackedWidget->setCurrentIndex(RT_ADVANCED);
            advancedDestPort->setText(simplePort->text());
            break;
    }
}

void RuleDialog::showError(const QString &err)
{
    KMessageBox::error(this, i18n("<p>Failed to insert rule.</p><p><i>%1</i></p>", err));
}

void RuleDialog::controlSimpleProtocol()
{
    bool fixedProto=simpleProfileRadio->isChecked();
    
    simpleProtocol->setEnabled(!fixedProto);
    if(fixedProto)
        simpleProtocol->setCurrentIndex(Types::PROTO_BOTH);
}

void RuleDialog::controlAdvancedProtocol()
{
    bool selectableProto=(advancedSrcPortRadio->isChecked() || advancedDestPortRadio->isChecked()) &&
                         !(advancedSrcProfileRadio->isChecked() || advancedDestProfileRadio->isChecked());
    
    advancedProtocol->setEnabled(selectableProto);
    if(!selectableProto)
        advancedProtocol->setCurrentIndex(Types::PROTO_BOTH);
}

}

#include "ruledialog.moc"
