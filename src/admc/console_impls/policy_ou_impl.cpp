/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020-2022 BaseALT Ltd.
 * Copyright (C) 2020-2022 Dmitry Degtyarev
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

#include "console_impls/policy_ou_impl.h"

#include "adldap.h"
#include "console_impls/all_policies_folder_impl.h"
#include "console_impls/item_type.h"
#include "console_impls/object_impl.h"
#include "console_impls/policy_impl.h"
#include "create_policy_dialog.h"
#include "find_policy_dialog.h"
#include "globals.h"
#include "gplink.h"
#include "policy_ou_results_widget.h"
#include "select_policy_dialog.h"
#include "status.h"
#include "utils.h"
#include "console_widget/console_tree_item_icons.h"

#include <QDebug>
#include <QMenu>
#include <QStandardItem>

bool index_is_domain(const QModelIndex &index) {
    const QString dn = index.data(PolicyOURole_DN).toString();
    const QString domain_dn = g_adconfig->domain_dn();
    const bool out = (dn == domain_dn);

    return out;
}

PolicyOUImpl::PolicyOUImpl(ConsoleWidget *console_arg)
: ConsoleImpl(console_arg) {
    policy_ou_results_widget = new PolicyOUResultsWidget(console_arg);
    set_results_widget(policy_ou_results_widget);

    create_ou_action = new QAction(tr("Create OU"), this);
    create_and_link_gpo_action = new QAction(tr("Create a GPO and link to this OU"), this);
    link_gpo_action = new QAction(tr("Link existing GPO"), this);
    find_gpo_action = new QAction(tr("Find GPO"), this);
    change_gp_options_action = new QAction(tr("Block inheritance"), this);
    change_gp_options_action->setCheckable(true);
    update_gp_options_check_state();

    connect(
        create_ou_action, &QAction::triggered,
        this, &PolicyOUImpl::create_ou);
    connect(
        create_and_link_gpo_action, &QAction::triggered,
        this, &PolicyOUImpl::create_and_link_gpo);
    connect(
        link_gpo_action, &QAction::triggered,
        this, &PolicyOUImpl::link_gpo);
    connect(
        find_gpo_action, &QAction::triggered,
        this, &PolicyOUImpl::find_gpo);
    connect(
        change_gp_options_action, &QAction::triggered,
        this, &PolicyOUImpl::change_gp_options);
}

void PolicyOUImpl::selected_as_scope(const QModelIndex &index) {
    policy_ou_results_widget->update(index);
}

// TODO: perform searches in separate threads
void PolicyOUImpl::fetch(const QModelIndex &index) {
    AdInterface ad;
    if (ad_failed(ad, console)) {
        return;
    }

    const QString dn = index.data(PolicyOURole_DN).toString();

    // Add child OU's
    {
        const QString base = dn;
        const SearchScope scope = SearchScope_Children;
        const QString filter = filter_CONDITION(Condition_Equals, ATTRIBUTE_OBJECT_CLASS, CLASS_OU);
        QList<QString> attributes = console_object_search_attributes();

        const QHash<QString, AdObject> results = ad.search(base, scope, filter, attributes);

        policy_ou_impl_add_objects_to_console(console, results.values(), index);
    }

    const bool is_domain = index_is_domain(index);

    // Add "All policies" folder if this is domain
    if (is_domain) {
        const QList<QStandardItem *> all_policies_row = console->add_scope_item(ItemType_AllPoliciesFolder, index);
        QStandardItem *all_policies_item = all_policies_row[0];
        all_policies_item->setText(tr("All policies"));
        all_policies_item->setIcon(QIcon::fromTheme("folder"));
        // Set sort index for "All policies" to 1 so it's always
        // at the bottom of the policy tree
        console->set_item_sort_index(all_policies_item->index(), 2);
    }

    // Add policies linked to this OU
    const AdObject parent_object = ad.search_object(dn);
    const QString gplink_string = parent_object.get_string(ATTRIBUTE_GPLINK);
    const Gplink gplink = Gplink(gplink_string);
    const QList<QString> gpo_list = gplink.get_gpo_list();
    update_ou_enforced_and_disabled_policies(gplink, index);

    policy_ou_impl_add_objects_from_dns(console, ad, gpo_list, index);
}

bool PolicyOUImpl::can_drop(const QList<QPersistentModelIndex> &dropped_list, const QSet<int> &dropped_type_list, const QPersistentModelIndex &target, const int target_type) {
    UNUSED_ARG(target);
    UNUSED_ARG(target_type);
    UNUSED_ARG(dropped_list);

    const bool dropped_are_policy = (dropped_type_list == QSet<int>({ItemType_Policy}));

    return dropped_are_policy;
}

void PolicyOUImpl::drop(const QList<QPersistentModelIndex> &dropped_list, const QSet<int> &dropped_type_list, const QPersistentModelIndex &target, const int target_type) {
    UNUSED_ARG(target_type);
    UNUSED_ARG(dropped_type_list);

    const QString ou_dn = target.data(PolicyOURole_DN).toString();

    const QList<QString> gpo_list = [&]() {
        QList<QString> out;

        for (const QPersistentModelIndex &index : dropped_list) {
            const QString gpo = index.data(PolicyRole_DN).toString();
            out.append(gpo);
        }

        return out;
    }();

    link_gpo_to_ou(target, ou_dn, gpo_list);

    // Need to refresh so that results widget is updated
    // because linking a gpo changes contents of results.
    const QModelIndex current_scope = console->get_current_scope_item();
    console->refresh_scope(current_scope);
}

void PolicyOUImpl::refresh(const QList<QModelIndex> &index_list) {
    const QModelIndex index = index_list[0];

    console->delete_children(index);
    fetch(index);

    policy_ou_results_widget->update(index);
}

void PolicyOUImpl::activate(const QModelIndex &index) {
    properties({index});
}

QList<QAction *> PolicyOUImpl::get_all_custom_actions() const {
    QList<QAction *> out;

    update_gp_options_check_state();
    out.append(create_ou_action);
    out.append(create_and_link_gpo_action);
    out.append(link_gpo_action);
    out.append(find_gpo_action);
    out.append(change_gp_options_action);

    return out;
}

QSet<QAction *> PolicyOUImpl::get_custom_actions(const QModelIndex &index, const bool single_selection) const {
    UNUSED_ARG(index);

    QSet<QAction *> out;

    update_gp_options_check_state();
    if (single_selection) {
        out.insert(create_ou_action);
        out.insert(create_and_link_gpo_action);
        out.insert(link_gpo_action);
        out.insert(change_gp_options_action);

        const bool is_domain = [&]() {
            const QString dn = index.data(PolicyOURole_DN).toString();
            const QString domain_dn = g_adconfig->domain_dn();
            const bool is_domain_out = (dn == domain_dn);

            return is_domain_out;
        }();

        if (is_domain) {
            out.insert(find_gpo_action);
        }
    }

    return out;
}

QSet<StandardAction> PolicyOUImpl::get_standard_actions(const QModelIndex &index, const bool single_selection) const {
    UNUSED_ARG(index);
    UNUSED_ARG(single_selection);

    QSet<StandardAction> out;

    out.insert(StandardAction_Properties);

    const bool can_refresh = console_item_get_was_fetched(index);
    if (can_refresh) {
        out.insert(StandardAction_Refresh);
    }

    const bool is_domain = index_is_domain(index);

    if (!is_domain) {
        out.insert(StandardAction_Rename);
        out.insert(StandardAction_Delete);
    }

    return out;
}

QList<QString> PolicyOUImpl::column_labels() const {
    return {tr("Name")};
}

QList<int> PolicyOUImpl::default_columns() const {
    return {0};
}

void PolicyOUImpl::create_ou() {
    const QString parent_dn = get_selected_target_dn(console, ItemType_PolicyOU, PolicyOURole_DN);
    console_object_create({console}, CLASS_OU, parent_dn);
}

void PolicyOUImpl::create_and_link_gpo() {
    AdInterface ad;
    if (ad_failed(ad, console)) {
        return;
    }

    const QList<QModelIndex> selected_list = console->get_selected_items(ItemType_PolicyOU);

    if (selected_list.isEmpty()) {
        return;
    }

    const QModelIndex target_index = selected_list[0];
    const QString target_dn = target_index.data(PolicyOURole_DN).toString();

    auto dialog = new CreatePolicyDialog(ad, console);
    dialog->open();

    connect(
        dialog, &QDialog::accepted,
        this,
        [this, dialog, target_index, target_dn]() {
            AdInterface ad2;
            if (ad_failed(ad2, console)) {
                return;
            }

            const QString gpo_dn = dialog->get_created_dn();

            link_gpo_to_ou(target_index, target_dn, {gpo_dn});

            const QModelIndex current_scope = console->get_current_scope_item();
            policy_ou_results_widget->update(current_scope);

            // Add policy to "all policies" folder
            const AdObject gpo_object = ad2.search_object(gpo_dn);
            const QModelIndex all_policies_index = get_all_policies_folder_index(console);
            all_policies_folder_impl_add_objects(console, {gpo_object}, all_policies_index);
        });
}

void PolicyOUImpl::link_gpo() {
    AdInterface ad;
    if (ad_failed(ad, console)) {
        return;
    }

    auto dialog = new SelectPolicyDialog(ad, console);
    dialog->open();

    connect(
        dialog, &QDialog::accepted,
        this,
        [this, dialog]() {
            const QList<QModelIndex> selected_list = console->get_selected_items(ItemType_PolicyOU);
            const QModelIndex selected_index = selected_list[0];
            const QString target_dn = get_selected_target_dn(console, ItemType_PolicyOU, PolicyOURole_DN);
            const QList<QString> gpo_list = dialog->get_selected_dns();

            link_gpo_to_ou(selected_index, target_dn, gpo_list);
        });
}

void PolicyOUImpl::properties(const QList<QModelIndex> &index_list) {
    console_object_properties({console}, index_list, PolicyOURole_DN, {CLASS_OU});
}

void PolicyOUImpl::rename(const QList<QModelIndex> &index_list) {
    console_object_rename({console}, index_list, PolicyOURole_DN, CLASS_OU);
}

void PolicyOUImpl::delete_action(const QList<QModelIndex> &index_list) {
    console_object_delete({console}, index_list, PolicyOURole_DN);
}

void policy_ou_impl_add_objects_from_dns(ConsoleWidget *console, AdInterface &ad, const QList<QString> &dn_list, const QModelIndex &parent) {
    const QList<AdObject> object_list = [&]() {
        QList<AdObject> out;

        for (const QString &dn : dn_list) {
            const AdObject object = ad.search_object(dn);
            out.append(object);
        }

        return out;
    }();

    policy_ou_impl_add_objects_to_console(console, object_list, parent);
}

void policy_ou_impl_add_objects_to_console(ConsoleWidget *console, const QList<AdObject> &object_list, const QModelIndex &parent) {
    if (!parent.isValid()) {
        return;
    }

    const bool parent_was_fetched = console_item_get_was_fetched(parent);
    if (!parent_was_fetched) {
        return;
    }

    for (const AdObject &object : object_list) {
        const bool is_ou = object.is_class(CLASS_OU);
        const bool is_gpc = object.is_class(CLASS_GP_CONTAINER);

        if (is_ou) {
            const QList<QStandardItem *> row = console->add_scope_item(ItemType_PolicyOU, parent);

            policy_ou_impl_load_row(row, object);

            console->set_item_sort_index(row[0]->index(), 1);
        } else if (is_gpc) {
            const QList<QStandardItem *> row = console->add_scope_item(ItemType_Policy, parent);

            console_policy_load(row, object);
        }
    }
}

void policy_ou_impl_load_row(const QList<QStandardItem *> row, const AdObject &object) {
    QStandardItem *item = row[0];

    policy_ou_impl_load_item_data(item, object);

    const QString name = object.get_string(ATTRIBUTE_NAME);
    item->setText(name);
}

void PolicyOUImpl::link_gpo_to_ou(const QModelIndex &ou_index, const QString &ou_dn, const QList<QString> &gpo_list) {
    AdInterface ad;
    if (ad_failed(ad, console)) {
        return;
    }

    const Gplink original_gplink = [&]() {
        const AdObject target_object = ad.search_object(ou_dn);
        const QString gplink_string = target_object.get_string(ATTRIBUTE_GPLINK);
        const Gplink out = Gplink(gplink_string);

        return out;
    }();

    const Gplink new_gplink = [&]() {
        Gplink out = original_gplink;

        for (const QString &gpo : gpo_list) {
            out.add(gpo);
        }

        return out;
    }();

    const QString new_gplink_string = new_gplink.to_string();

    ad.attribute_replace_string(ou_dn, ATTRIBUTE_GPLINK, new_gplink_string);

    g_status->display_ad_messages(ad, console);

    const QList<QString> added_gpo_list = [&]() {
        QList<QString> out;

        const QList<QString> new_gpo_list = new_gplink.get_gpo_list();

        for (const QString &gpo : new_gpo_list) {
            const bool added = !original_gplink.contains(gpo);
            if (added) {
                out.append(gpo);
            }
        }

        return out;
    }();

    policy_ou_impl_add_objects_from_dns(console, ad, added_gpo_list, ou_index);

    const QModelIndex current_scope = console->get_current_scope_item();
    policy_ou_results_widget->update(current_scope);
}

void PolicyOUImpl::find_gpo() {
    auto dialog = new FindPolicyDialog(console, console);
    dialog->open();
}

void PolicyOUImpl::change_gp_options() {
    AdInterface ad;
    if (ad_failed(ad, console)) {
        return;
    }

    QStandardItem *currentItem = console->get_item(console->get_current_scope_item());
    const QString dn = currentItem->data(PolicyOURole_DN).toString();

    bool checked = change_gp_options_action->isChecked();
    bool res;

    QIcon icon_to_set;
    bool is_domain = index_is_domain(currentItem->index());

    if (checked)
    {
        res = ad.attribute_replace_string(dn, ATTRIBUTE_GPOPTIONS, GPOPTIONS_BLOCK_INHERITANCE);
        icon_to_set = is_domain ? get_console_tree_item_icon(ItemIconType_Domain_InheritanceBlocked) :
                get_console_tree_item_icon(ItemIconType_OU_InheritanceBlocked);
    }
    else
    {
        res = ad.attribute_replace_string(dn, ATTRIBUTE_GPOPTIONS, GPOPTIONS_INHERIT);
        icon_to_set = is_domain ? get_console_tree_item_icon(ItemIconType_Domain_Clean) :
                get_console_tree_item_icon(ItemIconType_OU_Clean);
    }

    if (!res) {
        g_status->display_ad_messages(ad, console);
        change_gp_options_action->toggle();
        return;
    }
    currentItem->setData(checked, PolicyOURole_Inheritance_Block);
    currentItem->setIcon(icon_to_set);
}


void PolicyOUImpl::update_gp_options_check_state() const {
    QVariant checked = console->get_current_scope_item().data(PolicyOURole_Inheritance_Block);

    if (checked.isValid()) {
        change_gp_options_action->setEnabled(true);
        change_gp_options_action->setChecked(checked.toBool());
    }
    else
        change_gp_options_action->setDisabled(true);
}

void PolicyOUImpl::update_ou_enforced_and_disabled_policies(const Gplink &gplink, const QModelIndex &ou_index) {
    QStandardItem *ou_scope_item = console->get_item(ou_index);
    QStringList enforced_gpo_dn_list;
    QStringList disabled_gpo_dn_list;
    for (QString gpo_dn : gplink.get_gpo_list()) {
        if (gplink.get_option(gpo_dn, GplinkOption_Enforced))
            enforced_gpo_dn_list.append(gpo_dn);
        if (gplink.get_option(gpo_dn, GplinkOption_Disabled))
            disabled_gpo_dn_list.append(gpo_dn);
    }

    ou_scope_item->setData(enforced_gpo_dn_list, PolicyOURole_Enforced_GPO_List);
    ou_scope_item->setData(disabled_gpo_dn_list, PolicyOURole_Disabled_GPO_List);
}

void policy_ou_impl_load_item_data(QStandardItem *item, const AdObject &object) {
    const QString dn = object.get_dn();
    item->setData(dn, PolicyOURole_DN);

    if (object.get_string(ATTRIBUTE_GPOPTIONS) == GPOPTIONS_BLOCK_INHERITANCE) {
        if (index_is_domain(item->index()))
            item->setIcon(get_console_tree_item_icon(ItemIconType_Domain_InheritanceBlocked));
        else
            item->setIcon(get_console_tree_item_icon(ItemIconType_OU_InheritanceBlocked));
    } else {
        item->setIcon(get_object_icon(object));
    }

    bool inheritance_is_blocked = object.get_int(ATTRIBUTE_GPOPTIONS);
    item->setData(inheritance_is_blocked, PolicyOURole_Inheritance_Block);
}

QModelIndex get_ou_child_policy_item(ConsoleWidget *console, const QModelIndex &ou_index, const QString &policy_dn) {
    QList<QModelIndex> found_policy_indexes = console->search_items(ou_index,
                                                                     PolicyRole_DN,
                                                                     policy_dn,
                                                                     {ItemType_Policy});
    QModelIndex policy_index = QModelIndex();
    for (QModelIndex index : found_policy_indexes)
    {
        if (index.parent().data(ConsoleRole_Type) == ItemType_PolicyOU)
        {
            policy_index = index;
            break;
        }
    }

    return policy_index;
}
