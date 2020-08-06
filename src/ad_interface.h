/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020 BaseALT Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AD_INTERFACE_H
#define AD_INTERFACE_H

#include <QObject>
#include <QList>
#include <QString>
#include <QMap>
#include <QHash>
#include <QDateTime>

// Interface between the GUI and AdConnection
// Stores attributes cache of objects
// Attributes cache is expanded as more objects are loaded and
// is updated on object changes
// Emits various signals for AD operation successes/failures

#define ATTRIBUTE_USER_ACCOUNT_CONTROL  "userAccountControl"
#define ATTRIBUTE_USER_PRINCIPAL_NAME   "userPrincipalName"
#define ATTRIBUTE_LOCKOUT_TIME          "lockoutTime"
#define ATTRIBUTE_ACCOUNT_EXPIRES       "accountExpires"

#define UAC_ACCOUNTDISABLE          0x0002
#define UAC_DONT_EXPIRE_PASSWORD    0x10000
#define UAC_SMARTCARD_REQUIRED      0x40000
#define UAC_NOT_DELEGATED           0x100000
#define UAC_USE_DES_KEY_ONLY        0x200000
#define UAC_DONT_REQUIRE_PREAUTH    0x400000
#define UAC_PASSWORD_EXPIRED        0x800000

#define LOCKOUT_UNLOCKED_VALUE "0"

#define AD_LARGEINTEGERTIME_NEVER_1 "0"
#define AD_LARGEINTEGERTIME_NEVER_2 "9223372036854775807"

class AdConnection;

namespace adldap
{
    class AdConnection;
};

enum NewObjectType {
    User,
    Computer,
    OU,
    Group,
    COUNT
};

enum AdInterfaceMessageType {
    AdInterfaceMessageType_Success,
    AdInterfaceMessageType_Error
};

enum EmitStatusMessage {
    EmitStatusMessage_Yes,
    EmitStatusMessage_No
};

class AdResult {
public:
    bool success;
    QString error;

    AdResult(bool success_arg);
    AdResult(bool success_arg, const QString &error_arg);
};

typedef QMap<QString, QList<QString>> Attributes;

class AdInterface final : public QObject {
Q_OBJECT

public:
    AdInterface(const AdInterface&) = delete;
    AdInterface& operator=(const AdInterface&) = delete;
    AdInterface(AdInterface&&) = delete;
    AdInterface& operator=(AdInterface&&) = delete;
    
    ~AdInterface();

    static AdInterface *instance();

    static QList<QString> get_domain_hosts(const QString &domain, const QString &site);

    AdResult login(const QString &host, const QString &domain);

    QString get_search_base();
    QString get_uri();

    QList<QString> list(const QString &dn);
    QList<QString> search(const QString &filter);

    Attributes get_all_attributes(const QString &dn);
    QList<QString> attribute_get_multi(const QString &dn, const QString &attribute);
    QString attribute_get(const QString &dn, const QString &attribute);

    AdResult attribute_replace(const QString &dn, const QString &attribute, const QString &value, EmitStatusMessage emit_message = EmitStatusMessage_Yes);
    AdResult object_create(const QString &name, const QString &dn, NewObjectType type);
    AdResult object_delete(const QString &dn);
    AdResult object_move(const QString &dn, const QString &new_container);
    AdResult object_rename(const QString &dn, const QString &new_name);
    AdResult set_pass(const QString &dn, const QString &password);
    AdResult user_set_uac_bit(const QString &dn, int bit, bool set);
    AdResult user_unlock(const QString &dn);
    void update_cache(const QList<QString> &changed_dns);
    
    QDateTime attribute_datetime_get(const QString &dn, const QString &attribute);
    AdResult attribute_datetime_replace(const QString &dn, const QString &attribute, const QDateTime &datetime);
    bool datetime_is_never(const QString &dn, const QString &attribute);

    AdResult group_add_user(const QString &group_dn, const QString &user_dn);
    AdResult group_remove_user(const QString &group_dn, const QString &user_dn);

    bool is_class(const QString &dn, const QString &object_class);
    bool is_user(const QString &dn);
    bool is_group(const QString &dn);
    bool is_container(const QString &dn);
    bool is_ou(const QString &dn);
    bool is_policy(const QString &dn);
    bool is_container_like(const QString &dn);

    bool user_get_uac_bit(const QString &dn, int bit);

    bool object_can_drop(const QString &dn, const QString &target_dn);
    void object_drop(const QString &dn, const QString &target_dn);

    void command(QStringList args);

signals:
    void modified();
    void logged_in();
    void status_message(const QString &msg, AdInterfaceMessageType type);

private:
    adldap::AdConnection *connection = nullptr;
    QHash<QString, Attributes> attributes_cache;
    bool suppress_not_found_error = false;
        
    AdInterface();

    bool should_emit_status_message(int result);
    void success_status_message(const QString &msg, EmitStatusMessage emit_message = EmitStatusMessage_Yes);
    void error_status_message(const QString &context, const QString &error, EmitStatusMessage emit_message = EmitStatusMessage_Yes);
    QString default_error_string(int ad_result) const;
}; 

QString extract_name_from_dn(const QString &dn);
QString extract_parent_dn_from_dn(const QString &dn);
QString filter_EQUALS(const QString &attribute, const QString &value);
QString filter_AND(const QString &a, const QString &b);
QString filter_OR(const QString &a, const QString &b);
QString filter_NOT(const QString &a);
QString get_uac_bit_description(int bit);

#endif /* AD_INTERFACE_H */
