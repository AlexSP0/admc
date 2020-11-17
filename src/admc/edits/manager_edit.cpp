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

#include "edits/manager_edit.h"
#include "utils.h"
#include "ad_interface.h"
#include "ad_config.h"
#include "details_dialog.h"
#include "select_dialog.h"

#include <QLineEdit>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

ManagerEdit::ManagerEdit(QObject *parent, QList<AttributeEdit *> *edits_out)
: AttributeEdit(parent)
{
    edit = new QLineEdit();

    const QString label_text = ADCONFIG()->get_attribute_display_name(ATTRIBUTE_MANAGER, CLASS_USER) + ":";
    label = new QLabel(label_text);
    connect_changed_marker(label);

    change_button = new QPushButton(tr("Change"));
    details_button = new QPushButton(tr("Details"));
    clear_button = new QPushButton(tr("Clear"));

    connect(
        change_button, &QPushButton::clicked,
        this, &ManagerEdit::on_change);
    connect(
        details_button, &QPushButton::clicked,
        this, &ManagerEdit::on_details);
    connect(
        clear_button, &QPushButton::clicked,
        this, &ManagerEdit::on_clear);

    AttributeEdit::append_to_list(edits_out);
}

void ManagerEdit::load(const AdObject &object) {
    original_value = object.get_string(ATTRIBUTE_MANAGER);
    current_value = original_value;
    
    load_current_value();

    emit edited();
}

void ManagerEdit::set_read_only(const bool read_only) {

}

void ManagerEdit::add_to_layout(QGridLayout *layout) {
    append_to_grid_layout_with_label(layout, label, edit);

    const int button_row = layout->rowCount();
    layout->addWidget(change_button, button_row, 0);
    layout->addWidget(details_button, button_row, 1);
    layout->addWidget(clear_button, button_row, 2);
}

bool ManagerEdit::verify() const {
    return true;
}

bool ManagerEdit::changed() const {
    return (current_value != original_value);
}

bool ManagerEdit::apply(const QString &dn) const {
    const bool success = AD()->attribute_replace_string(dn, ATTRIBUTE_MANAGER, current_value);

    return success;
}

void ManagerEdit::on_change() {
    const QList<QString> selected_objects = SelectDialog::open({CLASS_USER}, SelectDialogMultiSelection_No);

    if (selected_objects.size() > 0) {
        current_value = selected_objects[0];

        load_current_value();

        emit edited();
    }
}

void ManagerEdit::on_details() {
    DetailsDialog::open_for_target(current_value);
}

void ManagerEdit::on_clear() {
    current_value = QString();

    load_current_value();

    emit edited();
}

void ManagerEdit::load_current_value() {
    const QString rdn = dn_get_rdn(current_value);
    edit->setText(current_value);

    const bool have_manager = !current_value.isEmpty();
    details_button->setEnabled(have_manager);
    clear_button->setEnabled(have_manager);
}