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

#include "central_widget.h"

#include "object_model.h"
#include "policy_model.h"
#include "utils.h"
#include "adldap.h"
#include "properties_dialog.h"
#include "globals.h"
#include "settings.h"
#include "filter_dialog.h"
#include "filter_widget/filter_widget.h"
#include "object_actions.h"
#include "status.h"
#include "rename_dialog.h"
#include "create_dialog.h"
#include "create_policy_dialog.h"
#include "move_dialog.h"
#include "find_dialog.h"
#include "password_dialog.h"
#include "rename_policy_dialog.h"
#include "select_dialog.h"
#include "console_widget/console_widget.h"
#include "console_widget/results_view.h"
#include "editors/multi_editor.h"
#include "gplink.h"
#include "policy_results_widget.h"

#include <QDebug>
#include <QAbstractItemView>
#include <QVBoxLayout>
#include <QSplitter>
#include <QDebug>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QApplication>
#include <QTreeWidget>
#include <QStack>
#include <QMenu>
#include <QLabel>
#include <QSortFilterProxyModel>

enum DropType {
    DropType_Move,
    DropType_AddToGroup,
    DropType_None
};

DropType get_object_drop_type(const QModelIndex &dropped, const QModelIndex &target);
bool object_should_be_in_scope(const AdObject &object);

CentralWidget::CentralWidget()
: QWidget()
{
    object_actions = new ObjectActions(this);

    create_policy_action = new QAction(tr("New policy"), this);
    auto add_link_action = new QAction(tr("Add link"), this);
    auto rename_policy_action = new QAction(tr("Rename"), this);
    auto delete_policy_action = new QAction(tr("Delete"), this);

    // NOTE: create policy action is not added to list
    // because it is shown for the policies container, not
    // for gpo's themselves.
    
    // NOTE: add policy actions to this list so that they
    // are processed
    policy_actions = {
        add_link_action,
        rename_policy_action,
        delete_policy_action,
    };

    open_filter_action = new QAction(tr("&Filter objects"), this);
    dev_mode_action = new QAction(tr("Dev mode"), this);
    show_noncontainers_action = new QAction(tr("&Show non-container objects in Console tree"), this);

    filter_dialog = nullptr;
    open_filter_action->setEnabled(false);

    console_widget = new ConsoleWidget();

    object_results = new ResultsView(this);
    // TODO: not sure how to do this. View headers dont even
    // have sections until their models are loaded.
    // g_settings->setup_header_state(object_results->header(),
    // VariantSetting_ResultsHeader);

    auto policies_results = new ResultsView(this);
    policies_results->detail_view()->header()->setDefaultSectionSize(200);
    policies_results_id = console_widget->register_results(policies_results, policy_model_header_labels(), policy_model_default_columns());
    
    policy_results_widget = new PolicyResultsWidget();
    policy_links_results_id = console_widget->register_results(policy_results_widget);
    
    auto layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);
    layout->addWidget(console_widget);

    // Refresh head when settings affecting the filter
    // change. This reloads the model with an updated filter
    const BoolSettingSignal *advanced_features = g_settings->get_bool_signal(BoolSetting_AdvancedFeatures);
    connect(
        advanced_features, &BoolSettingSignal::changed,
        this, &CentralWidget::refresh_head);

    const BoolSettingSignal *show_non_containers = g_settings->get_bool_signal(BoolSetting_ShowNonContainersInConsoleTree);
    connect(
        show_non_containers, &BoolSettingSignal::changed,
        this, &CentralWidget::refresh_head);

    const BoolSettingSignal *dev_mode_signal = g_settings->get_bool_signal(BoolSetting_DevMode);
    connect(
        dev_mode_signal, &BoolSettingSignal::changed,
        this, &CentralWidget::refresh_head);

    g_settings->connect_toggle_widget(console_widget->get_scope_view(), BoolSetting_ShowConsoleTree);
    g_settings->connect_toggle_widget(console_widget->get_description_bar(), BoolSetting_ShowResultsHeader);

    g_settings->connect_action_to_bool_setting(dev_mode_action, BoolSetting_DevMode);
    g_settings->connect_action_to_bool_setting(show_noncontainers_action, BoolSetting_ShowNonContainersInConsoleTree);

    connect(
        open_filter_action, &QAction::triggered,
        this, &CentralWidget::open_filter);

    connect(
        object_actions->get(ObjectAction_NewUser), &QAction::triggered,
        this, &CentralWidget::create_user);
    connect(
        object_actions->get(ObjectAction_NewComputer), &QAction::triggered,
        this, &CentralWidget::create_computer);
    connect(
        object_actions->get(ObjectAction_NewOU), &QAction::triggered,
        this, &CentralWidget::create_ou);
    connect(
        object_actions->get(ObjectAction_NewGroup), &QAction::triggered,
        this, &CentralWidget::create_group);
    connect(
        object_actions->get(ObjectAction_Delete), &QAction::triggered,
        this, &CentralWidget::delete_objects);
    connect(
        object_actions->get(ObjectAction_Rename), &QAction::triggered,
        this, &CentralWidget::rename);
    connect(
        object_actions->get(ObjectAction_Move), &QAction::triggered,
        this, &CentralWidget::move);
    connect(
        object_actions->get(ObjectAction_AddToGroup), &QAction::triggered,
        this, &CentralWidget::add_to_group);
    connect(
        object_actions->get(ObjectAction_Enable), &QAction::triggered,
        this, &CentralWidget::enable);
    connect(
        object_actions->get(ObjectAction_Disable), &QAction::triggered,
        this, &CentralWidget::disable);
    connect(
        object_actions->get(ObjectAction_ResetPassword), &QAction::triggered,
        this, &CentralWidget::reset_password);
    connect(
        object_actions->get(ObjectAction_Find), &QAction::triggered,
        this, &CentralWidget::find);
    connect(
        object_actions->get(ObjectAction_EditUpnSuffixes), &QAction::triggered,
        this, &CentralWidget::edit_upn_suffixes);

    connect(
        create_policy_action, &QAction::triggered,
        this, &CentralWidget::create_policy);
    connect(
        add_link_action, &QAction::triggered,
        this, &CentralWidget::add_link);
    connect(
        rename_policy_action, &QAction::triggered,
        this, &CentralWidget::rename_policy);
    connect(
        delete_policy_action, &QAction::triggered,
        this, &CentralWidget::delete_policy);

    connect(
        console_widget, &ConsoleWidget::current_scope_item_changed,
        this, &CentralWidget::on_current_scope_changed);
    connect(
        console_widget, &ConsoleWidget::results_count_changed,
        this, &CentralWidget::update_description_bar);
    connect(
        console_widget, &ConsoleWidget::item_fetched,
        this, &CentralWidget::fetch_scope_node);
    connect(
        console_widget, &ConsoleWidget::items_can_drop,
        this, &CentralWidget::on_items_can_drop);
    connect(
        console_widget, &ConsoleWidget::items_dropped,
        this, &CentralWidget::on_items_dropped);
    connect(
        console_widget, &ConsoleWidget::properties_requested,
        this, &CentralWidget::on_properties_requested);
    connect(
        console_widget, &ConsoleWidget::selection_changed,
        this, &CentralWidget::update_actions_visibility);
    connect(
        console_widget, &ConsoleWidget::context_menu,
        this, &CentralWidget::context_menu);

    update_actions_visibility();
}

void CentralWidget::go_online(AdInterface &ad) {
    // NOTE: filter dialog requires a connection to load
    // display strings from adconfig so create it here
    filter_dialog = new FilterDialog(this);
    connect(
        filter_dialog, &QDialog::accepted,
        this, &CentralWidget::refresh_head);
    open_filter_action->setEnabled(true);

    // NOTE: Header labels are from ADCONFIG, so have to get them
    // after going online
    object_results_id = console_widget->register_results(object_results, object_model_header_labels(), object_model_default_columns());

    // Add top domain item
    const QString head_dn = g_adconfig->domain_head();
    const AdObject head_object = ad.search_object(head_dn);

    QStandardItem *item = console_widget->add_scope_item(object_results_id, ScopeNodeType_Dynamic, QModelIndex());

    scope_head_index = QPersistentModelIndex(item->index());

    setup_object_scope_item(item, head_object);

    console_widget->set_current_scope(item->index());

    // Add top policies item
    QStandardItem *policies_item = console_widget->add_scope_item(policies_results_id, ScopeNodeType_Static, QModelIndex());
    policies_item->setText(tr("Group Policy Objects"));
    policies_index = QPersistentModelIndex(policies_item->index());
    policies_item->setDragEnabled(false);
    policies_item->setIcon(QIcon::fromTheme("folder"));

    // Load policies items
    const QList<QString> search_attributes = policy_model_search_attributes();
    const QString filter = filter_CONDITION(Condition_Equals, ATTRIBUTE_OBJECT_CLASS, CLASS_GP_CONTAINER);

    const QHash<QString, AdObject> search_results = ad.search(filter, search_attributes, SearchScope_All);

    for (const AdObject &object : search_results.values()) {
        add_policy_to_console(object);
    }

    console_widget->sort_scope();
}

void CentralWidget::open_filter() {
    if (filter_dialog != nullptr) {
        filter_dialog->open();
    }
}

void CentralWidget::delete_objects() {
    const QHash<QString, QPersistentModelIndex> selected = get_selected_dns_and_indexes();
    const QList<QString> deleted_objects = object_delete(selected.keys(), this);

    for (const QString &dn : deleted_objects) {
        const QModelIndex index = selected[dn];
        console_widget->delete_item(index);
    }
}

void CentralWidget::on_properties_requested() {
    const QHash<QString, QPersistentModelIndex> targets = get_selected_dns_and_indexes();
    if (targets.size() != 1) {
        return;
    }

    const QString target = targets.keys()[0];

    PropertiesDialog *dialog = PropertiesDialog::open_for_target(target);

    connect(
        dialog, &PropertiesDialog::applied,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const AdObject updated_object = ad.search_object(target);
            const QModelIndex index = targets.values()[0];
            update_console_item(index, updated_object);

            update_actions_visibility();
        });
}

void CentralWidget::rename() {
    const QHash<QString, QPersistentModelIndex> targets = get_selected_dns_and_indexes();

    auto dialog = new RenameDialog(targets.keys(), this);
    dialog->open();

    connect(
        dialog, &RenameDialog::accepted,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const QString new_dn = dialog->get_new_dn();
            const QModelIndex index = targets.values()[0];
            const AdObject updated_object = ad.search_object(new_dn);
            update_console_item(index, updated_object);

            console_widget->sort_scope();
        });
}

void CentralWidget::create_helper(const QString &object_class) {
    const QHash<QString, QPersistentModelIndex> targets = get_selected_dns_and_indexes();

    auto dialog = new CreateDialog(targets.keys(), object_class, this);
    dialog->open();

    connect(
        dialog, &CreateDialog::accepted,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const QString created_dn = dialog->get_created_dn();
            const AdObject new_object = ad.search_object(created_dn);
            const QModelIndex parent_index = targets.values()[0];
            add_object_to_console(new_object, parent_index);

            console_widget->sort_scope();
        });
}

void CentralWidget::move() {
    const QHash<QString, QPersistentModelIndex> targets = get_selected_dns_and_indexes();

    auto dialog = new MoveDialog(targets.keys(), this);
    dialog->open();

    connect(
        dialog, &QDialog::accepted,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const QString new_parent_dn = dialog->get_selected();
            const QModelIndex new_parent_index =
            [=]() {
                const QList<QModelIndex> search_results = console_widget->search_scope_by_role(ObjectRole_DN, new_parent_dn, ItemType_DomainObject);

                if (search_results.size() == 1) {
                    return search_results[0];
                } else {
                    return QModelIndex();
                }
            }();

            const QList<QString> moved_objects = dialog->get_moved_objects();

            for (const QString &dn : moved_objects) {
                const QModelIndex index = targets[dn];
                move_object_in_console(ad, index, new_parent_dn, new_parent_index);
            }

            console_widget->sort_scope();
        });
}

void CentralWidget::add_to_group() {
    const QList<QString> targets = get_selected_dns();
    object_add_to_group(targets, this);
}

void CentralWidget::enable() {
    enable_disable_helper(false);
}

void CentralWidget::disable() {
    enable_disable_helper(true);
}

void CentralWidget::find() {
    const QList<QString> targets = get_selected_dns();

    if (targets.size() != 1) {
        return;
    }

    const QString target = targets[0];

    auto find_dialog = new FindDialog(filter_classes, target, this);
    find_dialog->open();
}

void CentralWidget::reset_password() {
    const QList<QString> targets = get_selected_dns();
    const auto password_dialog = new PasswordDialog(targets, this);
    password_dialog->open();
}

void CentralWidget::create_user() {
    create_helper(CLASS_USER);
}

void CentralWidget::create_computer() {
    create_helper(CLASS_COMPUTER);
}

void CentralWidget::create_ou() {
    create_helper(CLASS_OU);
}

void CentralWidget::create_group() {
    create_helper(CLASS_GROUP);
}

void CentralWidget::edit_upn_suffixes() {
    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    // Open editor for upn suffixes attribute of partitions
    // object
    const QString partitions_dn = g_adconfig->partitions_dn();
    const AdObject partitions_object = ad.search_object(partitions_dn);
    const QList<QByteArray> current_values = partitions_object.get_values(ATTRIBUTE_UPN_SUFFIXES);

    auto editor = new MultiEditor(ATTRIBUTE_UPN_SUFFIXES, current_values, this);
    editor->open();

    // When editor is accepted, update values of upn
    // suffixes
    connect(editor, &QDialog::accepted,
        [this, editor, partitions_dn]() {
            AdInterface ad2;
            if (ad_failed(ad2)) {
                return;
            }

            const QList<QByteArray> new_values = editor->get_new_values();

            ad2.attribute_replace_values(partitions_dn, ATTRIBUTE_UPN_SUFFIXES, new_values);
            g_status()->display_ad_messages(ad2, this);
        });
}

void CentralWidget::create_policy() {
    // TODO: implement using ad.create_gpo() (which is
    // unfinished)

    auto dialog = new CreatePolicyDialog(this);

    connect(
        dialog, &QDialog::accepted,
        [this, dialog]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const QString dn = dialog->get_created_dn();

            const QList<QString> search_attributes = policy_model_search_attributes();
            const QHash<QString, AdObject> search_results = ad.search(QString(), search_attributes, SearchScope_Object, dn);
            const AdObject object = search_results[dn];

            add_policy_to_console(object);

            // NOTE: not adding policy object to the domain
            // tree, but i think it's ok?
        });

    dialog->open();
}

void CentralWidget::add_link() {
    const QList<QModelIndex> selected = console_widget->get_selected_items();
    if (selected.size() == 0) {
        return;
    }

    auto dialog = new SelectDialog({CLASS_OU}, SelectDialogMultiSelection_Yes, this);

    QObject::connect(
        dialog, &SelectDialog::accepted,
        [=]() {           
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            show_busy_indicator();

            const QList<QString> gpos =
            [selected]() {
                QList<QString> out;

                for (const QModelIndex &index : selected) {
                    const QString gpo = index.data(PolicyRole_DN).toString();
                    out.append(gpo);
                }

                return out;
            }();

            const QList<QString> ou_list = dialog->get_selected();

            for (const QString &ou_dn : ou_list) {
                const QHash<QString, AdObject> results = ad.search(QString(), {ATTRIBUTE_GPLINK}, SearchScope_Object, ou_dn);
                const AdObject ou_object = results[ou_dn];
                const QString gplink_string = ou_object.get_string(ATTRIBUTE_GPLINK);
                Gplink gplink = Gplink(gplink_string);

                for (const QString &gpo : gpos) {
                    gplink.add(gpo);
                }

                ad.attribute_replace_string(ou_dn, ATTRIBUTE_GPLINK, gplink.to_string());
            }

            const QModelIndex current_scope = console_widget->get_current_scope_item();
            policy_results_widget->update(current_scope);

            hide_busy_indicator();

            g_status()->display_ad_messages(ad, this);
        });

    dialog->open();
}

void CentralWidget::rename_policy() {
    const QList<QModelIndex> indexes = console_widget->get_selected_items();
    if (indexes.size() != 1) {
        return;
    }

    const QModelIndex index = indexes[0];
    const QString dn = index.data(PolicyRole_DN).toString();

    auto dialog = new RenamePolicyDialog(dn, this);
    dialog->open();

    connect(
        dialog, &RenamePolicyDialog::accepted,
        [=]() {
            AdInterface ad;
            if (ad_failed(ad)) {
                return;
            }

            const AdObject updated_object = ad.search_object(dn);
            update_policy_item(index, updated_object);

            console_widget->sort_scope();
        });
}

void CentralWidget::delete_policy() {
    const QHash<QString, QPersistentModelIndex> selected = get_selected_dns_and_indexes();

    if (selected.size() == 0) {
        return;
    }

    const bool confirmed = confirmation_dialog(tr("Are you sure you want to delete this policy and all of it's links?"), this);
    if (!confirmed) {
        return;
    }

    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    show_busy_indicator();

    for (const QModelIndex &index : selected) {
        const QString dn = index.data(PolicyRole_DN).toString();
        const bool success = ad.object_delete(dn);

        if (success) {
            // Remove deleted policy from console
            console_widget->delete_item(index);

            // Remove links to delete policy
            const QString filter = filter_CONDITION(Condition_Contains, ATTRIBUTE_GPLINK, dn);
            const QList<QString> search_attributes = {
                ATTRIBUTE_GPLINK,
            };
            const QHash<QString, AdObject> search_results = ad.search(filter, search_attributes, SearchScope_All);
            for (const AdObject &object : search_results.values()) {
                const QString gplink_string = object.get_string(ATTRIBUTE_GPLINK);
                Gplink gplink = Gplink(gplink_string);
                gplink.remove(dn);

                const QString updated_gplink_string = gplink.to_string();
                ad.attribute_replace_string(object.get_dn(), ATTRIBUTE_GPLINK, updated_gplink_string);
            }
        }
    }

    hide_busy_indicator();

    g_status()->display_ad_messages(ad, this);
}

// NOTE: only check if object can be dropped if dropping a
// single object, because when dropping multiple objects it
// is ok for some objects to successfully drop and some to
// fail. For example, if you drop users together with OU's
// onto a group, users will be added to that group while OU
// will fail to drop.
void CentralWidget::on_items_can_drop(const QList<QModelIndex> &dropped_list, const QModelIndex &target, bool *ok) {
    if (dropped_list.size() != 1) {
        *ok = true;
        return;
    } else {
        const QModelIndex dropped = dropped_list[0];

        const DropType drop_type = get_object_drop_type(dropped, target);
        const bool can_drop = (drop_type != DropType_None);

        *ok = can_drop;
    }
}

void CentralWidget::on_items_dropped(const QList<QModelIndex> &dropped_list, const QModelIndex &target) {
    const QString target_dn = target.data(ObjectRole_DN).toString();

    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    show_busy_indicator();

    for (const QModelIndex &dropped : dropped_list) {
        const QString dropped_dn = dropped.data(ObjectRole_DN).toString();
        const DropType drop_type = get_object_drop_type(dropped, target);

        switch (drop_type) {
            case DropType_Move: {
                const bool move_success = ad.object_move(dropped_dn, 
                    target_dn);

                if (move_success) {
                    move_object_in_console(ad, dropped, target_dn, target);
                }

                break;
            }
            case DropType_AddToGroup: {
                ad.group_add_member(target_dn, dropped_dn);

                break;
            }
            case DropType_None: {
                break;
            }
        }
    }

    console_widget->sort_scope();

    hide_busy_indicator();

    g_status()->display_ad_messages(ad, nullptr);
}

void CentralWidget::on_current_scope_changed() {
    const QModelIndex current_scope = console_widget->get_current_scope_item();
    policy_results_widget->update(current_scope);

    update_description_bar();
}

void CentralWidget::refresh_head() {
    show_busy_indicator();

    console_widget->refresh_scope(scope_head_index);

    hide_busy_indicator();
}

// TODO: currently calling this when current scope changes,
// but also need to call this when items are added/deleted
void CentralWidget::update_description_bar() {
    const QString text =
    [this]() {
        const QModelIndex current_scope = console_widget->get_current_scope_item();
        const ItemType type = (ItemType) current_scope.data(ConsoleRole_Type).toInt();

        if (type == ItemType_DomainObject) {
            const int results_count = console_widget->get_current_results_count();
            const QString out = tr("%n object(s)", "", results_count);

            return out;
        } else {
            return QString();
        }
    }();

    console_widget->set_description_bar_text(text);
}

void CentralWidget::add_actions_to_action_menu(QMenu *menu) {
    object_actions->add_to_menu(menu);

    menu->addAction(create_policy_action);

    for (QAction *action : policy_actions) {
        menu->addAction(action);
    }

    menu->addSeparator();

    console_widget->add_actions_to_action_menu(menu);
}

void CentralWidget::add_actions_to_navigation_menu(QMenu *menu) {
    console_widget->add_actions_to_navigation_menu(menu);
}

void CentralWidget::add_actions_to_view_menu(QMenu *menu) {
    console_widget->add_actions_to_view_menu(menu);

    menu->addSeparator();

    menu->addAction(open_filter_action);

    menu->addAction(show_noncontainers_action);

    #ifdef QT_DEBUG
    menu->addAction(dev_mode_action);
    #endif
}

// Load children of this item in scope tree
// and load results linked to this scope item
void CentralWidget::fetch_scope_node(const QModelIndex &index) {
    show_busy_indicator();

    const bool dev_mode = g_settings->get_bool(BoolSetting_DevMode);

    //
    // Search object's children
    //
    const QString filter =
    [=]() {
        const QString user_filter = filter_dialog->filter_widget->get_filter();

        const QString is_container = is_container_filter();

        // NOTE: OR user filter with containers filter so that container objects are always shown, even if they are filtered out by user filter
        QString out = filter_OR({user_filter, is_container});

        // Hide advanced view only" objects if advanced view
        // setting is off
        const bool advanced_features_OFF = !g_settings->get_bool(BoolSetting_AdvancedFeatures);
        if (advanced_features_OFF) {
            const QString advanced_features = filter_CONDITION(Condition_NotEquals, ATTRIBUTE_SHOW_IN_ADVANCED_VIEW_ONLY, "true");
            out = filter_OR({out, advanced_features});
        }

        // OR filter with some dev mode object classes, so that they show up no matter what when dev mode is on
        if (dev_mode) {
            const QList<QString> schema_classes = {
                "classSchema",
                "attributeSchema",
                "displaySpecifier",
            };

            QList<QString> class_filters;
            for (const QString &object_class : schema_classes) {
                const QString class_filter = filter_CONDITION(Condition_Equals, ATTRIBUTE_OBJECT_CLASS, object_class);
                class_filters.append(class_filter);
            }

            out = filter_OR({out, filter_OR(class_filters)});
        }

        return out;
    }();

    const QList<QString> search_attributes = object_model_search_attributes();

    const QString dn = index.data(ObjectRole_DN).toString();

    // TODO: handle connect/search failure
    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    QHash<QString, AdObject> search_results = ad.search(filter, search_attributes, SearchScope_Children, dn);

    // Dev mode
    // NOTE: configuration and schema objects are hidden so that they don't show up in regular searches. Have to use search_object() and manually add them to search results.
    if (dev_mode) {
        const QString search_base = g_adconfig->domain_head();
        const QString configuration_dn = g_adconfig->configuration_dn();
        const QString schema_dn = g_adconfig->schema_dn();

        if (dn == search_base) {
            search_results[configuration_dn] = ad.search_object(configuration_dn);
        } else if (dn == configuration_dn) {
            search_results[schema_dn] = ad.search_object(schema_dn);
        }
    }

    //
    // Add items to scope and results
    //

    for (const AdObject &object : search_results.values()) {
        add_object_to_console(object, index);
    }

    // TODO: make sure to sort scope everywhere it should be
    // sorted
    console_widget->sort_scope();

    hide_busy_indicator();
}

// NOTE: "containers" referenced here don't mean objects
// with "container" object class. Instead it means all the
// objects that can have children(some of which are not
// "container" class).
bool object_should_be_in_scope(const AdObject &object) {
    const bool is_container =
    [=]() {
        const QList<QString> filter_containers = g_adconfig->get_filter_containers();
        const QString object_class = object.get_string(ATTRIBUTE_OBJECT_CLASS);

        return filter_containers.contains(object_class);
    }();

    const bool show_non_containers_ON = g_settings->get_bool(BoolSetting_ShowNonContainersInConsoleTree);

    return (is_container || show_non_containers_ON);
}

void CentralWidget::add_object_to_console(const AdObject &object, const QModelIndex &parent) {
    // NOTE: don't add if parent wasn't fetched yet. If that
    // is the case then the object will be added naturally
    // when parent is fetched.
    const bool parent_was_fetched = console_widget->item_was_fetched(parent);
    if (!parent_was_fetched) {
        return;
    }

    const bool should_be_in_scope = object_should_be_in_scope(object);

    if (should_be_in_scope) {
        QStandardItem *scope_item;
        QList<QStandardItem *> results_row;
        console_widget->add_buddy_scope_and_results(object_results_id, ScopeNodeType_Dynamic, parent, &scope_item, &results_row);

        setup_object_scope_item(scope_item, object);
        setup_object_results_row(results_row, object);
    } else {
        const QList<QStandardItem *> results_row = console_widget->add_results_row(parent);
        setup_object_results_row(results_row, object);
    }
}

// Updates console widget to reflect an object being moved
// on the server. Note that both new parent dn and index are
// passed because new_parent_index may be invalid. For
// example if it's not loaded into console yet, it can still
// be selected in move dialog. In that case we only delete
// object from it's old location and do nothing for new
// location. The object will be loaded at new location when
// it's parent is loaded.
void CentralWidget::move_object_in_console(AdInterface &ad, const QPersistentModelIndex &old_index, const QString &new_parent_dn, const QPersistentModelIndex &new_parent_index) {
    // TODO: look for a way to clear up this "have to delete
    // after adding" thing. Try to express it through
    // console widget's API. Failing that, at least write a
    // comment in console_widget.h

    // NOTE: delete old item AFTER adding new item because.
    // 1) Need to get old dn from old index. 2) If old item
    // is deleted first, then it's possible for new parent
    // to get selected (if they are next to each other in
    // scope tree). Then what happens is that due to new
    // parent being selected, it gets fetched and loads new
    // object. End result is that new object is duplicated.
    if (new_parent_index.isValid()) {
        const QString old_dn = old_index.data(ObjectRole_DN).toString();
        const QString new_dn = dn_move(old_dn, new_parent_dn);
        const AdObject updated_object = ad.search_object(new_dn);
        add_object_to_console(updated_object, new_parent_index);
    }

    console_widget->delete_item(old_index);
}

void CentralWidget::update_console_item(const QModelIndex &index, const AdObject &object) {
    auto update_helper =
    [this, object](const QModelIndex &the_index) {
        const bool is_scope = console_widget->is_scope_item(the_index);

        if (is_scope) {
            QStandardItem *scope_item = console_widget->get_scope_item(the_index);

            const QString old_dn = scope_item->data(ObjectRole_DN).toString();
            const bool dn_changed = (old_dn != object.get_dn());

            setup_object_scope_item(scope_item, object);

            // NOTE: if dn changed, then this change affects
            // this item's children, so have to refresh to
            // reload children.
            if (dn_changed) {
                console_widget->refresh_scope(the_index);
            }
        } else {
            QList<QStandardItem *> results_row = console_widget->get_results_row(the_index);
            load_object_row(results_row, object);
        }
    };

    update_helper(index);

    const QModelIndex buddy = console_widget->get_buddy(index);
    if (buddy.isValid()) {
        update_helper(buddy);
    }
}

void CentralWidget::update_policy_item(const QModelIndex &index, const AdObject &object) {
    auto update_helper =
    [this, object](const QModelIndex &the_index) {
        const bool is_scope = console_widget->is_scope_item(the_index);

        if (is_scope) {
            QStandardItem *scope_item = console_widget->get_scope_item(the_index);
            setup_policy_scope_item(scope_item, object);
        } else {
            QList<QStandardItem *> results_row = console_widget->get_results_row(the_index);
            setup_policy_results_row(results_row, object);
        }
    };

    update_helper(index);

    const QModelIndex buddy = console_widget->get_buddy(index);
    if (buddy.isValid()) {
        update_helper(buddy);
    }
}

// Determine what kind of drop type is dropping this object
// onto target. If drop type is none, then can't drop this
// object on this target.
DropType get_object_drop_type(const QModelIndex &dropped, const QModelIndex &target) {
    const bool dropped_is_target =
    [&]() {
        const QString dropped_dn = dropped.data(ObjectRole_DN).toString();
        const QString target_dn = target.data(ObjectRole_DN).toString();

        return (dropped_dn == target_dn);
    }();

    const QList<QString> dropped_classes = dropped.data(ObjectRole_ObjectClasses).toStringList();
    const QList<QString> target_classes = target.data(ObjectRole_ObjectClasses).toStringList();

    const bool dropped_is_user = dropped_classes.contains(CLASS_USER);
    const bool dropped_is_group = dropped_classes.contains(CLASS_GROUP);
    const bool target_is_group = target_classes.contains(CLASS_GROUP);

    if (dropped_is_target) {
        return DropType_None;
    } else if (dropped_is_user && target_is_group) {
        return DropType_AddToGroup;
    } else if (dropped_is_group && target_is_group) {
        return DropType_AddToGroup;
    } else {
        const QList<QString> dropped_superiors = g_adconfig->get_possible_superiors(dropped_classes);

        const bool target_is_valid_superior =
        [&]() {
            for (const auto &object_class : dropped_superiors) {
                if (target_classes.contains(object_class)) {
                    return true;
                }
            }

            return false;
        }();

        if (target_is_valid_superior) {
            return DropType_Move;
        } else {
            return DropType_None;
        }
    }
}

void CentralWidget::enable_disable_helper(const bool disabled) {
    const QHash<QString, QPersistentModelIndex> targets = get_selected_dns_and_indexes();

    show_busy_indicator();

    const QList<QString> changed_objects = object_enable_disable(targets.keys(), disabled, this);

    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    for (const QString &dn : changed_objects) {
        const QPersistentModelIndex index = targets[dn];

        auto update_helper =
        [=](const QPersistentModelIndex &the_index) {
            const bool is_scope = console_widget->is_scope_item(the_index);

            if (is_scope) {
                QStandardItem *scope_item = console_widget->get_scope_item(the_index);

                scope_item->setData(disabled, ObjectRole_AccountDisabled);
            } else {
                QList<QStandardItem *> results_row = console_widget->get_results_row(the_index);
                results_row[0]->setData(disabled, ObjectRole_AccountDisabled);
            }
        };

        update_helper(index);

        const QPersistentModelIndex buddy = console_widget->get_buddy(index);
        if (buddy.isValid()) {
            update_helper(buddy);
        }
    }

    update_actions_visibility();

    hide_busy_indicator();
}

// First, hide all actions, then show whichever actions are
// appropriate for current console selection
void CentralWidget::update_actions_visibility() {
    const QList<QModelIndex> selected_indexes = console_widget->get_selected_items();

    // Show create policy action if clicked on "Policies"
    // scope item
    const bool create_policy_action_is_visible =
    [&]() {
        if (selected_indexes.size() != 1) {
            return false;
        }

        const QModelIndex index = selected_indexes[0];
        const bool is_policies_item = (index == policies_index);

        return is_policies_item;
    }();;
    create_policy_action->setVisible(create_policy_action_is_visible);

    for (QAction *action : policy_actions) {
        action->setVisible(false);
    }

    if (indexes_are_of_type(selected_indexes, ItemType_Policy)) {
        for (QAction *action : policy_actions) {
            action->setVisible(true);
        }
    }

    object_actions->update_actions_visibility(selected_indexes);
}

// Get selected indexes mapped to their DN's
QHash<QString, QPersistentModelIndex> CentralWidget::get_selected_dns_and_indexes() {
    QHash<QString, QPersistentModelIndex> out;

    const QList<QModelIndex> indexes = console_widget->get_selected_items();
    for (const QModelIndex &index : indexes) {
        const QString dn = index.data(ObjectRole_DN).toString();
        out[dn] = QPersistentModelIndex(index);
    }

    return out;
}

QList<QString> CentralWidget::get_selected_dns() {
    const QHash<QString, QPersistentModelIndex> selected = get_selected_dns_and_indexes();

    return selected.keys();
}

void CentralWidget::add_policy_to_console(const AdObject &object) {
    QStandardItem *scope_item;
    QList<QStandardItem *> results_row;
    console_widget->add_buddy_scope_and_results(policy_links_results_id, ScopeNodeType_Static, policies_index, &scope_item, &results_row);

    setup_policy_scope_item(scope_item, object);
    setup_policy_results_row(results_row, object);
}