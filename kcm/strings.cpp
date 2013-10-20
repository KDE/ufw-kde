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

/**
 * NOTE: *Most* of the information/strings in this file come from the UFW manpage.
 */

#include "strings.h"
#include <KDE/KLocale>

namespace UFW
{

namespace Strings
{

QString policyInformation(bool withLimit)
{
    return QString("<p><ul>")+
           i18n("<li><i><b>Allow</b></i> accepts access to the specified ports/services.</li>"
                "<li><i><b>Deny</b></i> blocks access to the specified ports/services.</li>"
                "<li><i><b>Reject</b></i> is similar to <i>Deny</i>, but lets the sender know when traffic "
                "is being denied, rather than simply ignoring it.</li>")
           +(withLimit
                ? i18n("<li><i><b>Limit</b></i> enables connection rate limiting. This is useful for protecting "
                       "against brute-force login attacks. The firewall will deny connections if an "
                       "IP address has attempted to initiate 6 or more connections in the last 30 seconds.</li>")
                : QString())
           +QString("</ul></p>");
}

QString logLevelInformation()
{
    return i18n("<p><ul>"
                "<li><i><b>Off</b></i> disables logging.</li>"
                "<li><i><b>Low</b></i> logs all blocked packets not matching the default policy (with rate limiting), "
                "as well as packets matching logged rules.</li>"
                "<li><i><b>Medium</b></i> as per <i>Low</i>, plus all allowed packets not matching the default policy, "
                "all <i>invalid</i> packets, and all new connections. All logging is done with rate limiting.</li>"
                "<li><i><b>High</b></i> as per <i>Medium</i> (without rate limiting), plus all packets with rate "
                "limiting.</li>"
                "<li><i><b>Full</b></i> log everything, without rate limiting.</li>"
                "</ul></p>"
                "<p>Levels above <i>Medium</i> generate a lot of logging output, and may quickly fill up your disk. "
                "<i>Medium</i> may generate a lot of logging output on a busy system.</p>");
}

QString loggingInformation()
{
    return i18n("<p>Per rule logging.</ul>"
                "<li><i><b>None</b></i> no logging is performed when a packet matches a rule.</li>"
                "<li><i><b>New connections</b></i> will log all new connections matching a rule.</li>"
                "<li><i><b>All packets</b></i> will log all packets matching a rule.</li>"
                "</ul></p>");
}

QString ruleOrderInformation()
{
    return i18n("<p>Rule ordering is important and the first match wins. Therefore when adding rules, add "
                "the more specific rules first with more general rules later.</p>");
}

}

}
