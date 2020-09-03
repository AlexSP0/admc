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

#include "address_tab.h"
#include "ad_interface.h"
#include "status.h"
#include "edits/attribute_edit.h"
#include "attribute_display_strings.h"
#include "utils.h"
#include "edits/country_edit.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QHash>
#include <QFile>
#include <QLabel>
#include <algorithm>

// TODO: translate country strings to Russian (qt doesn't have it)

#define COUNTRY_CODE_NONE 0

CountryEdit::CountryEdit() {
    combo = new QComboBox();

    QObject::connect(
        combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [this]() {
            emit edited();
        });

    // Load all country names into combobox
    // NOTE: temp collections to sort items for combo box
    QList<QString> all_countries;
    QHash<QString, int> string_to_code;

    QFile file(":/countries.csv");
    if (!file.open(QIODevice::ReadOnly)) {
        printf("ERROR: Failed to load countries file!\n");
    } else {
        // Load countries csv into maps
        // Map country code to country string and country abbreviation
        QHash<QString, int> country_string_to_code;
        
        // Skip header
        file.readLine();

        while (!file.atEnd()) {
            const QByteArray line_array = file.readLine();
            const QString line(line_array);
            const QList<QString> line_split = line.split(',');

            if (line_split.size() != 3) {
                continue;
            }

            const QString country_string = line_split[0];
            const QString abbreviation = line_split[1];
            const QString code_string = line_split[2];
            const int code = code_string.toInt();

            country_strings[code] = country_string;
            country_abbreviations[code] = abbreviation;

            all_countries.append(country_string);
            string_to_code[country_string] = code;
        }
    }

    // Put country strings/codes into combo box, sorted by strings
    std::sort(all_countries.begin(), all_countries.end());

    // Special case for "None" country
    // TODO: this seems really easy to break
    const QString none_string = tr("None");
    string_to_code[none_string] = COUNTRY_CODE_NONE;
    all_countries.insert(0, none_string);
    country_strings[COUNTRY_CODE_NONE] = "";
    country_abbreviations[COUNTRY_CODE_NONE] = "";

    combo->blockSignals(true);
    for (auto country_string : all_countries) {
        const int code = string_to_code[country_string];

        combo->addItem(country_string, code);
    }
    combo->blockSignals(false);
}

void CountryEdit::load(const QString &dn) {
    const QString current_code_string = AdInterface::instance()->attribute_get(dn, ATTRIBUTE_COUNTRY_CODE);
    int current_code = current_code_string.toInt();
    if (current_code_string == "") {
        current_code = COUNTRY_CODE_NONE;
    }

    original_value = current_code;

    const QVariant code_variant(current_code);
    const int index = combo->findData(code_variant);
    if (index != -1) {
        combo->setCurrentIndex(index);
    }

    emit edited();
}

void CountryEdit::add_to_layout(QGridLayout *layout) {
    const QString label_text = get_attribute_display_string(ATTRIBUTE_COUNTRY) + ":";
    const auto label = new QLabel(label_text);

    connect_changed_marker(this, label);
    append_to_grid_layout_with_label(layout, label, combo);
}

bool CountryEdit::verify_input(QWidget *parent) {
    return true;
}

bool CountryEdit::changed() const {
    const int new_value = combo->currentData().toInt();
    return (new_value != original_value);
}

bool CountryEdit::apply(const QString &dn) {
    const int code = combo->currentData().toInt();

    const QString code_string = QString::number(code);
    const QString country_string = country_strings[code];
    const QString abbreviation = country_abbreviations[code];

    bool success = true;
    success = success && AdInterface::instance()->attribute_replace(dn, ATTRIBUTE_COUNTRY_CODE, code_string);
    success = success && AdInterface::instance()->attribute_replace(dn, ATTRIBUTE_COUNTRY_ABBREVIATION, abbreviation);
    success = success && AdInterface::instance()->attribute_replace(dn, ATTRIBUTE_COUNTRY, country_string);

    return success;
}