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

#define AD_LONGTIME_NEVER_1 "0"
#define AD_LONGTIME_NEVER_2 "9223372036854775807"

class AdConnection;

namespace adldap
{
    class AdConnection;
};

// Interface between the GUI and AdConnection
// Stores attributes cache of objects
// Attributes cache is expanded as more objects are loaded and
// is updated on object changes
// Emits various signals for AD operation successes/failures

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

class AdResult {
public:
    bool success;
    QString msg;

    AdResult(bool success_arg, const QString &msg);
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

    AdResult attribute_replace(const QString &dn, const QString &attribute, const QString &value);
    AdResult object_create(const QString &name, const QString &dn, NewObjectType type);
    AdResult object_delete(const QString &dn);
    AdResult object_move(const QString &dn, const QString &new_container);
    AdResult object_rename(const QString &dn, const QString &new_name);
    AdResult set_pass(const QString &dn, const QString &password);
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
    void success_status_message(const QString &msg);
    void error_status_message(const QString &context, const QString &error);
    QString default_error_string(int ad_result) const;
}; 

QString extract_name_from_dn(const QString &dn);
QString extract_parent_dn_from_dn(const QString &dn);
QString filter_EQUALS(const QString &attribute, const QString &value);
QString filter_AND(const QString &a, const QString &b);
QString filter_OR(const QString &a, const QString &b);
QString filter_NOT(const QString &a);

#endif /* AD_INTERFACE_H */
