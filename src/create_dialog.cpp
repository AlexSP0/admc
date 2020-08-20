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

#include "create_dialog.h"
#include "ad_interface.h"
#include "utils.h"
#include "status.h"
#include "attribute_edit.h"
#include "utils.h"

#include <QDialog>
#include <QLineEdit>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QList>
#include <QComboBox>
#include <QMessageBox>
#include <QCheckBox>
#include <QDialogButtonBox>

// TODO: implement cannot change pass

void autofill_edit_from_other_edit(QLineEdit *from, QLineEdit *to);
void autofill_full_name(QLineEdit *full_name_edit, QLineEdit *first_name_edit, QLineEdit *last_name_edit);
QString create_type_to_string(const CreateType &type);

CreateDialog::CreateDialog(const QString &parent_dn_arg, CreateType type_arg, QWidget *parent)
: QDialog(parent)
{
    parent_dn = parent_dn_arg;
    type = type_arg;

    setAttribute(Qt::WA_DeleteOnClose);
    resize(600, 600);

    const QString type_string = create_type_to_string(type);
    const auto title_text = QString(CreateDialog::tr("Create %1 in \"%2\"")).arg(type_string, parent_dn);
    const auto title_label = new QLabel(title_text);
    
    edits_layout = new QGridLayout();

    name_edit = new QLineEdit();
    append_to_grid_layout_with_label(edits_layout, tr("Name"), name_edit);

    switch (type) {
        case CreateType_User: {
            make_user_edits();
            break;
        }
        case CreateType_Group: {
            make_group_edits();
            break;
        }
        default: {
            break;
        }
    }

    layout_attribute_edits(all_edits, edits_layout, this);

    auto button_box = new QDialogButtonBox(QDialogButtonBox::Ok |  QDialogButtonBox::Cancel, this);

    const auto top_layout = new QVBoxLayout();
    setLayout(top_layout);
    top_layout->addWidget(title_label);
    top_layout->addLayout(edits_layout);
    top_layout->addWidget(button_box);

    connect(
        button_box, &QDialogButtonBox::accepted,
        this, &CreateDialog::accept);
    connect(
        button_box, &QDialogButtonBox::rejected,
        this, &QDialog::reject);
}

void CreateDialog::accept() {
    const QString name = name_edit->text();

    auto get_suffix =
    [](CreateType type_arg) {
        switch (type_arg) {
            case CreateType_User: return "CN";
            case CreateType_Computer: return "CN";
            case CreateType_OU: return "OU";
            case CreateType_Group: return "CN";
            case CreateType_COUNT: return "COUNT";
        }
        return "";
    };
    const QString suffix = get_suffix(type);

    const QString dn = suffix + "=" + name + "," + parent_dn;

    const bool verify_success = verify_attribute_edits(all_edits, this);
    if (!verify_success) {
        return;
    }

    auto get_classes =
    [](CreateType type_arg) {
        static const char *classes_user[] = {CLASS_USER, NULL};
        static const char *classes_group[] = {CLASS_GROUP, NULL};
        static const char *classes_ou[] = {CLASS_OU, NULL};
        static const char *classes_computer[] = {CLASS_TOP, CLASS_PERSON, CLASS_ORG_PERSON, CLASS_USER, CLASS_COMPUTER, NULL};

        switch (type_arg) {
            case CreateType_User: return classes_user;
            case CreateType_Computer: return classes_computer;
            case CreateType_OU: return classes_ou;
            case CreateType_Group: return classes_group;
            case CreateType_COUNT: return classes_user;
        }
        return classes_user;
    };
    const char **classes = get_classes(type);

    AdInterface::instance()->start_batch();
    {
        const AdResult result_add = AdInterface::instance()->object_add(dn, classes);

        bool result_apply = false;
        if (result_add.success) {
            result_apply = apply_attribute_edits(all_edits, dn, ApplyAttributeEditBatch_No, this);
        }

        const QString type_string = create_type_to_string(type);

        if (result_add.success & result_apply) {
            const QString message = QString(CreateDialog::tr("Created %1 - \"%2\"")).arg(type_string, name);

            Status::instance()->message(message, StatusType_Success);

            QDialog::accept();
        } else {
            if (result_add.success) {
                AdInterface::instance()->object_delete(dn);
            } else {
                QMessageBox::critical(this, QObject::tr("Error"), result_add.error_with_context);
            }

            const QString message = QString(CreateDialog::tr("Failed to create %1 - \"%2\"")).arg(type_string, name);
            Status::instance()->message(message, StatusType_Error);
        }
    }
    AdInterface::instance()->end_batch();
}

void CreateDialog::make_group_edits() {
    const auto sama_name = new StringEdit(ATTRIBUTE_SAMACCOUNT_NAME);
    autofill_edit_from_other_edit(name_edit, sama_name->edit);

    const auto group_scope = new GroupScopeEdit();
    const auto group_type = new GroupTypeEdit();

    all_edits = {
        sama_name,
        group_scope,
        group_type
    };
}

void CreateDialog::make_user_edits() {
    // TODO: do password, make it share code with password dialog
    // make_edit(tr("Password"), &pass_edit);
    // make_edit(tr("Confirm password"), &pass_confirm_edit);

    const QList<QString> string_attributes = {
        ATTRIBUTE_FIRST_NAME,
        ATTRIBUTE_LAST_NAME,
        ATTRIBUTE_DISPLAY_NAME,
        ATTRIBUTE_INITIALS,
        ATTRIBUTE_USER_PRINCIPAL_NAME,
        ATTRIBUTE_SAMACCOUNT_NAME,
    };
    QMap<QString, StringEdit *> string_edits;
    make_string_edits(string_attributes, &string_edits);

    QLineEdit *sama_name_edit = string_edits[ATTRIBUTE_SAMACCOUNT_NAME]->edit;
    autofill_edit_from_other_edit(name_edit, sama_name_edit);

    const QList<AccountOption> options = {
        AccountOption_PasswordExpired,
        AccountOption_DontExpirePassword,
        AccountOption_Disabled
        // TODO: AccountOption_CannotChangePass
    };
    QMap<AccountOption, AccountOptionEdit *> option_edits;
    make_accout_option_edits(options, &option_edits);

    // NOTE: use keys from lists to get correct order
    for (auto attribute : string_attributes) {
        all_edits.append(string_edits[attribute]);
    }
    for (auto option : options) {
        all_edits.append(option_edits[option]);
    }

    // When PasswordExpired is set, you can't set CannotChange and DontExpirePassword
    // Prevent the conflicting checks from being set when PasswordExpired is set already and show a message about it
    const QCheckBox *pass_expired_check = option_edits[AccountOption_PasswordExpired]->check;
    auto connect_never_expire_conflict =
    [this, pass_expired_check, option_edits](AccountOption option) {
        QCheckBox *conflict = option_edits[option]->check; 
        
        connect(conflict, &QCheckBox::stateChanged,
            [this, conflict, option, pass_expired_check]() {
                if (checkbox_is_checked(pass_expired_check) && checkbox_is_checked(conflict)) {
                    conflict->setCheckState(Qt::Checked);

                    const QString pass_expired_text = get_account_option_description(AccountOption_PasswordExpired);
                    const QString conflict_text = get_account_option_description(option);
                    const QString error = QString(tr("Can't set \"%1\" when \"%2\" is set already.")).arg(conflict_text, pass_expired_text);

                    QMessageBox::warning(this, "Error", error);
                }
            }
            );
    };

    QLineEdit *full_name_edit = string_edits[ATTRIBUTE_DISPLAY_NAME]->edit;
    QLineEdit *first_name_edit = string_edits[ATTRIBUTE_FIRST_NAME]->edit;
    QLineEdit *last_name_edit = string_edits[ATTRIBUTE_LAST_NAME]->edit;
    autofill_full_name(full_name_edit, first_name_edit, last_name_edit);

    connect_never_expire_conflict(AccountOption_PasswordExpired);
    // TODO: connect_never_expire_conflict(AccountOption_CannotChange);
}

// When "from" edit is edited, the text is copied to "to" edit
// But "to" can still be edited separately if needed
void autofill_edit_from_other_edit(QLineEdit *from, QLineEdit *to) {
    QObject::connect(
        from, &QLineEdit::textChanged,
        [=] () {
            to->setText(from->text());
        });
}

void autofill_full_name(QLineEdit *full_name_edit, QLineEdit *first_name_edit, QLineEdit *last_name_edit) {
    auto autofill =
    [=]() {
        const QString first_name = first_name_edit->text(); 
        const QString last_name = last_name_edit->text();
        const QString full_name = first_name + " " + last_name; 

        full_name_edit->setText(full_name);
    };

    QObject::connect(
        first_name_edit, &QLineEdit::textChanged,
        autofill);
    QObject::connect(
        last_name_edit, &QLineEdit::textChanged,
        autofill);
}

QString create_type_to_string(const CreateType &type) {
    switch (type) {
        case CreateType_User: return CreateDialog::tr("User");
        case CreateType_Computer: return CreateDialog::tr("Computer");
        case CreateType_OU: return CreateDialog::tr("Organization Unit");
        case CreateType_Group: return CreateDialog::tr("Group");
        case CreateType_COUNT: return "COUNT";
    }
    return "";
}
