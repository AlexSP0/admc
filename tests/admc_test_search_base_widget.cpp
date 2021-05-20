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

#include "admc_test_search_base_widget.h"

#include "filter_widget/search_base_widget.h"
#include "console_types/console_object.h"
#include "select_container_dialog.h"
#include "globals.h"

#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QTreeView>

void ADMCTestSearchBaseWidget::init() {
    ADMCTest::init();

    search_base_widget = new SearchBaseWidget();

    auto layout = new QVBoxLayout();
    parent_widget->setLayout(layout);
    layout->addWidget(search_base_widget);

    parent_widget->show();
    QVERIFY(QTest::qWaitForWindowExposed(parent_widget, 1000));

    combo = search_base_widget->findChild<QComboBox *>();
    QVERIFY(combo != nullptr);

    browse_button = search_base_widget->findChild<QPushButton *>();
    QVERIFY(browse_button != nullptr);

    // Create test OU's
    const QList<QString> ou_name_list = {
        "test-ou-alpha",
        "test-ou-beta",
        "test-ou-gamma",
    };
    dn_list.clear();
    for (const QString &ou_name : ou_name_list) {
        const QString dn = test_object_dn(ou_name, CLASS_OU);
        
        dn_list.append(dn);

        const bool create_success = ad.object_add(dn, CLASS_OU);
        QVERIFY(create_success);
    }
}

// By default, domain head should be selected
void ADMCTestSearchBaseWidget::default_to_domain_head() {
    const QString domain_head = g_adconfig->domain_head();

    const QString search_base = search_base_widget->get_search_base();
    QVERIFY(search_base == domain_head);
}

// After selecting a search base, the widget should return
// the DN of selected search base
void ADMCTestSearchBaseWidget::select_search_base() {
    const QString select_dn = dn_list[0];
    add_search_base(select_dn);

    const QString search_base = search_base_widget->get_search_base();
    QVERIFY(search_base == select_dn);
}

// Adding multiple search bases to combo box, then selecting
// one of them in the combobox should make the widget return
// that search base.
void ADMCTestSearchBaseWidget::select_search_base_multiple() {
    for (const QString &dn : dn_list) {
        add_search_base(dn);
    }

    // Alpha is at index 1 in the combo (0 is domain)
    combo->setCurrentIndex(1);
    const QString search_base = search_base_widget->get_search_base();
    QVERIFY(search_base == dn_list[0]);
}

void ADMCTestSearchBaseWidget::add_search_base(const QString &dn) {
    browse_button->click();
    auto select_container_dialog = search_base_widget->findChild<SelectContainerDialog *>();
    QVERIFY(select_container_dialog != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(select_container_dialog, 1000));

    auto select_container_view = select_container_dialog->findChild<QTreeView *>();
    QVERIFY(select_container_view != nullptr);
    navigate_until_object(select_container_view, dn, ContainerRole_DN);

    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Enter);
    QVERIFY(QTest::qWaitForWindowExposed(search_base_widget, 1000));

    // NOTE: have to delete manually, dialog deletes itself
    // on close a bit late which causes consecutive add_search_base()
    // calls to get the dialog that should've been destroyed
    delete select_container_dialog;
}

void ADMCTestSearchBaseWidget::serialize() {
    // Setup some state
    for (const QString &dn : dn_list) {
        add_search_base(dn);
    }

    combo->setCurrentIndex(1);
    const QString search_base_original = search_base_widget->get_search_base();

    // Serialize
    QByteArray stream_bytes;
    QDataStream stream_in(&stream_bytes, QIODevice::WriteOnly);
    stream_in << search_base_widget;

    // Change state
    combo->setCurrentIndex(2);
    
    // Deserialize
    QDataStream stream_out(stream_bytes);
    stream_out >> search_base_widget;

    // Check that deserialization successfully restored
    // original state
    const QString search_base_deserialized = search_base_widget->get_search_base();
    QVERIFY(search_base_original == search_base_deserialized);
}

QTEST_MAIN(ADMCTestSearchBaseWidget)