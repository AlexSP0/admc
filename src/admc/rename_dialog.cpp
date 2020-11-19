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

#include "rename_dialog.h"
#include "ad_interface.h"
#include "edits/attribute_edit.h"
#include "edits/string_edit.h"
#include "status.h"
#include "utils.h"
#include "config.h"

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>

// TODO: figure out what can and can't be renamed and disable renaming for exceptions (computers can't for example)

RenameDialog::RenameDialog(const QString &target_arg)
: QDialog()
{
    target = target_arg;
    const AdObject object = AD()->search_object(target);

    setAttribute(Qt::WA_DeleteOnClose);

    const QString object_as_folder = dn_as_folder(object.get_dn());
    const auto title = QString(tr("Rename %1 - %2")).arg(object_as_folder, ADMC_APPLICATION_NAME);
    setWindowTitle(title);

    const QString object_class = object.get_string(ATTRIBUTE_OBJECT_CLASS);

    name_edit = new QLineEdit();

    if (object.is_class(CLASS_USER)) {
        const QList<QString> attributes = {
            ATTRIBUTE_FIRST_NAME,
            ATTRIBUTE_LAST_NAME,
            ATTRIBUTE_DISPLAY_NAME,
            ATTRIBUTE_USER_PRINCIPAL_NAME,
            ATTRIBUTE_SAMACCOUNT_NAME
        };
        make_string_edits(attributes, object_class, this, &all_edits);
    } else if (object.is_class(CLASS_GROUP)) {
        new StringEdit(ATTRIBUTE_SAMACCOUNT_NAME, object_class, this, &all_edits);
    }

    auto button_box = new QDialogButtonBox();
    ok_button = button_box->addButton(QDialogButtonBox::Ok);
    reset_button = button_box->addButton(QDialogButtonBox::Reset);
    auto cancel_button = button_box->addButton(QDialogButtonBox::Cancel);
    connect(
        ok_button, &QPushButton::clicked,
        this, &QDialog::accept);
    connect(
        reset_button, &QPushButton::clicked,
        this, &RenameDialog::reset);
    connect(
        cancel_button, &QPushButton::clicked,
        this, &RenameDialog::reject);

    const auto edits_layout = new QFormLayout();
    edits_layout->addRow(tr("Name:"), name_edit);
    edits_add_to_layout(all_edits, edits_layout);
    
    const auto top_layout = new QVBoxLayout();
    setLayout(top_layout);
    top_layout->addLayout(edits_layout);
    top_layout->addWidget(button_box);

    for (auto edit : all_edits) {
        connect(
            edit, &AttributeEdit::edited,
            this, &RenameDialog::on_edited);
    }
    connect(
        name_edit, &QLineEdit::textChanged,
        this, &RenameDialog::on_edited);
    on_edited();

    reset();
}

void RenameDialog::accept() {
    const QString old_name = dn_get_rdn(target);

    auto fail_msg =
    [=]() {
        const QString message = QString(tr("Failed to rename object - \"%1\"")).arg(old_name);
        Status::instance()->message(message, StatusType_Error);
    };

    const int errors_index = Status::instance()->get_errors_size();
    AD()->start_batch();
    {
        const QString new_name = name_edit->text();
        const bool rename_success = AD()->object_rename(target, new_name);

        if (rename_success) {
            const QString new_dn = dn_rename(target, new_name);
            const bool apply_success = edits_apply(all_edits, new_dn);

            if (apply_success) {
                const QString message = QString(tr("Renamed object - \"%1\"")).arg(old_name);
                Status::instance()->message(message, StatusType_Success);
            } else {
                fail_msg();
            }
        } else {
            fail_msg();
        }
    }
    AD()->end_batch();

    QDialog::close();
    Status::instance()->show_errors_popup(errors_index);
}

void RenameDialog::on_edited() {
    reset_button->setEnabled(true);

    const bool name_filled = !name_edit->text().isEmpty();
    ok_button->setEnabled(name_filled);
}

void RenameDialog::reset() {
    const QString name = dn_get_rdn(target);
    name_edit->setText(name);

    const AdObject object = AD()->search_object(target);
    edits_load(all_edits, object);

    reset_button->setEnabled(false);
    ok_button->setEnabled(false);
}
