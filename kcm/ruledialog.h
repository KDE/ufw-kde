#ifndef UFW_RULE_DIALOG_H
#define UFW_RULE_DIALOG_H

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

#include <KDE/KDialog>
#include <QtCore/QMap>
#include "rule.h"
#include "kcm.h"
#include "ui_rulewidget.h"

namespace UFW
{

class RuleDialog : public KDialog, public Ui::RuleWidget
{
    Q_OBJECT

    public:

    RuleDialog(Kcm *parent, bool isEditDlg);
    virtual ~RuleDialog();

    void reset();
    void setRule(const Rule &rule);
    bool ipV6Enabled() const { return kcm->ipV6Enabled(); }

    private Q_SLOTS:

    void update();
    void setRuleType();
    void showError(const QString &err);
    void controlSimpleProtocol();
    void controlAdvancedProtocol();

    private:

    Kcm            *kcm;
    bool           isEdit;
    Rule           editingRule;
    QMap<int, int> simpleIndexToPredefinedPort,
                   advancedIndexToPredefinedPort;
};

}

#endif
