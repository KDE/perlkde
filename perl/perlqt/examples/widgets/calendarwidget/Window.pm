package Window;

use strict;
use warnings;
use blib;

use Qt4;
use Qt4::isa qw( Qt4::Widget );

# [0]
use Qt4::slots
    localeChanged => ['int'],
    firstDayChanged => ['int'],
    selectionModeChanged => ['int'],
    horizontalHeaderChanged => ['int'],
    verticalHeaderChanged => ['int'],
    selectedDateChanged => [],
    minimumDateChanged => ['QDate'],
    maximumDateChanged => ['QDate'],
    weekdayFormatChanged => [],
    weekendFormatChanged => [],
    reformatHeaders => [],
    reformatCalendarPage => [];

sub previewGroupBox() {
    return this->{previewGroupBox};
}

sub previewLayout() {
    return this->{previewLayout};
}

sub calendar() {
    return this->{calendar};
}

sub generalOptionsGroupBox() {
    return this->{generalOptionsGroupBox};
}

sub localeLabel() {
    return this->{localeLabel};
}

sub firstDayLabel() {
    return this->{firstDayLabel};
}

# [0]
sub selectionModeLabel() {
    return this->{selectionModeLabel};
}

sub horizontalHeaderLabel() {
    return this->{horizontalHeaderLabel};
}

sub verticalHeaderLabel() {
    return this->{verticalHeaderLabel};
}

sub localeCombo() {
    return this->{localeCombo};
}

sub firstDayCombo() {
    return this->{firstDayCombo};
}

sub selectionModeCombo() {
    return this->{selectionModeCombo};
}

sub gridCheckBox() {
    return this->{gridCheckBox};
}

sub navigationCheckBox() {
    return this->{navigationCheckBox};
}

sub horizontalHeaderCombo() {
    return this->{horizontalHeaderCombo};
}

sub verticalHeaderCombo() {
    return this->{verticalHeaderCombo};
}

sub datesGroupBox() {
    return this->{datesGroupBox};
}

sub currentDateLabel() {
    return this->{currentDateLabel};
}

sub minimumDateLabel() {
    return this->{minimumDateLabel};
}

sub maximumDateLabel() {
    return this->{maximumDateLabel};
}

sub currentDateEdit() {
    return this->{currentDateEdit};
}

sub minimumDateEdit() {
    return this->{minimumDateEdit};
}

sub maximumDateEdit() {
    return this->{maximumDateEdit};
}

sub textFormatsGroupBox() {
    return this->{textFormatsGroupBox};
}

sub weekdayColorLabel() {
    return this->{weekdayColorLabel};
}

sub weekendColorLabel() {
    return this->{weekendColorLabel};
}

sub headerTextFormatLabel() {
    return this->{headerTextFormatLabel};
}

sub weekdayColorCombo() {
    return this->{weekdayColorCombo};
}

sub weekendColorCombo() {
    return this->{weekendColorCombo};
}

sub headerTextFormatCombo() {
    return this->{headerTextFormatCombo};
}

sub firstFridayCheckBox() {
    return this->{firstFridayCheckBox};
}

# [1]
sub mayFirstCheckBox() {
    return this->{mayFirstCheckBox};
}

# [1]

# [0]
sub NEW {
    my ( $class, $parent ) = @_;
    $class->SUPER::NEW( $parent );
    this->createPreviewGroupBox();
    this->createGeneralOptionsGroupBox();
    this->createDatesGroupBox();
    this->createTextFormatsGroupBox();

    my $layout = Qt4::GridLayout();
    $layout->addWidget(this->previewGroupBox, 0, 0);
    $layout->addWidget(this->generalOptionsGroupBox, 0, 1);
    $layout->addWidget(this->datesGroupBox, 1, 0);
    $layout->addWidget(this->textFormatsGroupBox, 1, 1);
    $layout->setSizeConstraint(Qt4::Layout::SetFixedSize());
    this->setLayout($layout);

    this->previewLayout->setRowMinimumHeight(0, this->calendar->sizeHint()->height());
    this->previewLayout->setColumnMinimumWidth(0, this->calendar->sizeHint()->width());

    this->setWindowTitle(this->tr('Calendar Widget'));
}
# [0]

sub localeChanged {
    my ($index) = @_;
    this->calendar->setLocale(this->localeCombo->itemData($index)->toLocale());
}

# [1]
sub firstDayChanged {
    my ($index) = @_;
    this->calendar->setFirstDayOfWeek(
                                this->firstDayCombo->itemData($index)->toInt());
}
# [1]

sub selectionModeChanged {
    my ($index) = @_;
    this->calendar->setSelectionMode(
                               this->selectionModeCombo->itemData($index)->toInt());
}

sub horizontalHeaderChanged {
    my ($index) = @_;
    this->calendar->setHorizontalHeaderFormat(
        this->horizontalHeaderCombo->itemData($index)->toInt());
}

sub verticalHeaderChanged {
    my ($index) = @_;
    this->calendar->setVerticalHeaderFormat(
        this->verticalHeaderCombo->itemData($index)->toInt());
}

# [2]
sub selectedDateChanged {
    this->currentDateEdit->setDate(this->calendar->selectedDate());
}
# [2]

# [3]
sub minimumDateChanged {
    my ($date) = @_;
    this->calendar->setMinimumDate($date);
    this->maximumDateEdit->setDate(this->calendar->maximumDate());
}
# [3]

# [4]
sub maximumDateChanged {
    my ($date) = @_;
    this->calendar->setMaximumDate($date);
    this->minimumDateEdit->setDate(this->calendar->minimumDate());
}
# [4]

# [5]
sub weekdayFormatChanged {
    my $format = Qt4::TextCharFormat();

    $format->setForeground( Qt4::Brush(
        Qt4::qVariantValue(
            this->weekdayColorCombo->itemData(this->weekdayColorCombo->currentIndex()),
            'Qt4::Color')
        )
    );
    this->calendar->setWeekdayTextFormat(Qt4::Monday(), $format);
    this->calendar->setWeekdayTextFormat(Qt4::Tuesday(), $format);
    this->calendar->setWeekdayTextFormat(Qt4::Wednesday(), $format);
    this->calendar->setWeekdayTextFormat(Qt4::Thursday(), $format);
    this->calendar->setWeekdayTextFormat(Qt4::Friday(), $format);
}
# [5]

# [6]
sub weekendFormatChanged {
    my $format = Qt4::TextCharFormat();

    $format->setForeground( Qt4::Brush(
        Qt4::qVariantValue(
            this->weekendColorCombo->itemData(this->weekendColorCombo->currentIndex()),
            'Qt4::Color')
        )
    );
    this->calendar->setWeekdayTextFormat(Qt4::Saturday(), $format);
    this->calendar->setWeekdayTextFormat(Qt4::Sunday(), $format);
}
# [6]

# [7]
sub reformatHeaders {
    my $text = this->headerTextFormatCombo->currentText();
    my $format = Qt4::TextCharFormat();

    if ($text eq this->tr('Bold')) {
        $format->setFontWeight(Qt4::Font::Bold());
    } elsif ($text eq this->tr('Italic')) {
        $format->setFontItalic(1);
    } elsif ($text eq this->tr('Green')) {
        $format->setForeground(Qt4::Color(Qt4::green()));
    }
    this->calendar->setHeaderTextFormat($format);
}
# [7]

# [8]
sub reformatCalendarPage {
    my $mayFirstFormat = Qt4::TextCharFormat();
    this->{mayFirstFormat} = $mayFirstFormat;
    if (this->mayFirstCheckBox->isChecked()) {
        $mayFirstFormat->setForeground(Qt4::Brush(Qt4::red()));
    }

    my $firstFridayFormat = Qt4::TextCharFormat();
    this->{firstFridayFormat} = $firstFridayFormat;
    if (this->firstFridayCheckBox->isChecked()) {
        $firstFridayFormat->setForeground(Qt4::Brush(Qt4::blue()));
    }

    my $date = Qt4::Date(this->calendar->yearShown(), this->calendar->monthShown(), 1); 

    this->calendar->setDateTextFormat(Qt4::Date($date->year(), 5, 1), $mayFirstFormat);

    $date->setDate($date->year(), $date->month(), 1);
    while ($date->dayOfWeek() != Qt4::Friday()) {
        $date = $date->addDays(1);
    }
    this->calendar->setDateTextFormat($date, $firstFridayFormat);
}
# [8]

# [9]
sub createPreviewGroupBox {
    my $previewGroupBox = Qt4::GroupBox(this->tr('Preview'));
    this->{previewGroupBox} = $previewGroupBox;

    my $calendar = Qt4::CalendarWidget();
    this->{calendar} = $calendar;
    $calendar->setMinimumDate(Qt4::Date(1900, 1, 1));
    $calendar->setMaximumDate(Qt4::Date(3000, 1, 1));
    $calendar->setGridVisible(1);

    this->connect($calendar, SIGNAL 'currentPageChanged(int, int)',
            this, SLOT 'reformatCalendarPage()');

    my $previewLayout = Qt4::GridLayout();
    this->{previewLayout} = $previewLayout;
    $previewLayout->addWidget($calendar, 0, 0, Qt4::AlignCenter());
    $previewGroupBox->setLayout($previewLayout);
}
# [9]

# [10]
sub createGeneralOptionsGroupBox {
    my $generalOptionsGroupBox = Qt4::GroupBox(this->tr('General Options'));
    this->{generalOptionsGroupBox} = $generalOptionsGroupBox;

    my $localeCombo = Qt4::ComboBox();
    this->{localeCombo} = $localeCombo;
    my $curLocaleIndex = -1;
    my $index = 0;
    for (my $_lang = Qt4::Locale::C(); $_lang <= Qt4::Locale::LastLanguage(); ++$_lang) {
        #Qt4::Locale::Language lang = static_cast<Qt4::Locale::Language>(_lang);
        my $lang = $_lang;
        my $countries = Qt4::Locale::countriesForLanguage($lang);
        next unless $countries && ref $countries eq 'ARRAY';
        for (my $i = 0; $i < scalar @{$countries}; ++$i) {
            my $country = $countries->[$i];
            my $label = Qt4::Locale::languageToString($lang);
            $label .= '/';
            $label .= Qt4::Locale::countryToString($country);
            my $locale = Qt4::Locale($lang, $country);
            if (this->locale()->language() == $lang && this->locale()->country() == $country) {
                $curLocaleIndex = $index;
            }
            $localeCombo->addItem($label, Qt4::Variant($locale));
            ++$index;
        }
    }
    if ($curLocaleIndex != -1) {
        $localeCombo->setCurrentIndex($curLocaleIndex);
    }
    my $localeLabel = Qt4::Label(this->tr('&Locale'));
    this->{localeLabel} = $localeLabel;
    $localeLabel->setBuddy($localeCombo);

    my $firstDayCombo = Qt4::ComboBox();
    this->{firstDayCombo} = $firstDayCombo;
    $firstDayCombo->addItem(this->tr('Sunday'), Qt4::Variant(Qt4::Int(${Qt4::Sunday()})));
    $firstDayCombo->addItem(this->tr('Monday'), Qt4::Variant(Qt4::Int(${Qt4::Monday()})));
    $firstDayCombo->addItem(this->tr('Tuesday'), Qt4::Variant(Qt4::Int(${Qt4::Tuesday()})));
    $firstDayCombo->addItem(this->tr('Wednesday'), Qt4::Variant(Qt4::Int(${Qt4::Wednesday()})));
    $firstDayCombo->addItem(this->tr('Thursday'), Qt4::Variant(Qt4::Int(${Qt4::Thursday()})));
    $firstDayCombo->addItem(this->tr('Friday'), Qt4::Variant(Qt4::Int(${Qt4::Friday()})));
    $firstDayCombo->addItem(this->tr('Saturday'), Qt4::Variant(Qt4::Int(${Qt4::Saturday()})));

    my $firstDayLabel = Qt4::Label(this->tr('Wee&k starts on:'));
    this->{firstDayLabel} = $firstDayLabel;
    $firstDayLabel->setBuddy($firstDayCombo);
# [10]

    my $selectionModeCombo = Qt4::ComboBox();
    this->{selectionModeCombo} = $selectionModeCombo;
    $selectionModeCombo->addItem(this->tr('Single selection'),
                                Qt4::Variant(Qt4::Int(${Qt4::CalendarWidget::SingleSelection()})));
    $selectionModeCombo->addItem(this->tr('None'), Qt4::Variant(Qt4::Int(${Qt4::CalendarWidget::NoSelection()})));

    my $selectionModeLabel = Qt4::Label(this->tr('&Selection mode:'));
    this->{selectionModeLabel} = $selectionModeLabel;
    $selectionModeLabel->setBuddy($selectionModeCombo);

    my $gridCheckBox = Qt4::CheckBox(this->tr('&Grid'));
    this->{gridCheckBox} = $gridCheckBox;
    $gridCheckBox->setChecked(this->calendar->isGridVisible());

    my $navigationCheckBox = Qt4::CheckBox(this->tr('&Navigation bar'));
    this->{navigationCheckBox} = $navigationCheckBox;
    $navigationCheckBox->setChecked(1);

    my $horizontalHeaderCombo = Qt4::ComboBox();
    this->{horizontalHeaderCombo} = $horizontalHeaderCombo;
    $horizontalHeaderCombo->addItem(this->tr('Single letter day names'),
                                   Qt4::Variant(Qt4::Int(${Qt4::CalendarWidget::SingleLetterDayNames()})));
    $horizontalHeaderCombo->addItem(this->tr('Short day names'),
                                   Qt4::Variant(Qt4::Int(${Qt4::CalendarWidget::ShortDayNames()})));
    $horizontalHeaderCombo->addItem(this->tr('None'),
                                   Qt4::Variant(Qt4::Int(${Qt4::CalendarWidget::NoHorizontalHeader()})));
    $horizontalHeaderCombo->setCurrentIndex(1);

    my $horizontalHeaderLabel = Qt4::Label(this->tr('&Horizontal header:'));
    this->{horizontalHeaderLabel} = $horizontalHeaderLabel;
    $horizontalHeaderLabel->setBuddy($horizontalHeaderCombo);

    my $verticalHeaderCombo = Qt4::ComboBox();
    this->{verticalHeaderCombo} = $verticalHeaderCombo;
    $verticalHeaderCombo->addItem(this->tr('ISO week numbers'),
                                 Qt4::Variant(Qt4::Int(${Qt4::CalendarWidget::ISOWeekNumbers()})));
    $verticalHeaderCombo->addItem(this->tr('None'), Qt4::Variant(Qt4::Int(${Qt4::CalendarWidget::NoVerticalHeader()})));

    my $verticalHeaderLabel = Qt4::Label(this->tr('&Vertical header:'));
    this->{verticalHeaderLabel} = $verticalHeaderLabel;
    $verticalHeaderLabel->setBuddy($verticalHeaderCombo);

# [11]
    this->connect($localeCombo, SIGNAL 'currentIndexChanged(int)',
            this, SLOT 'localeChanged(int)');
    this->connect($firstDayCombo, SIGNAL 'currentIndexChanged(int)',
            this, SLOT 'firstDayChanged(int)');
    this->connect($selectionModeCombo, SIGNAL 'currentIndexChanged(int)',
            this, SLOT 'selectionModeChanged(int)');
    this->connect($gridCheckBox, SIGNAL 'toggled(bool)',
            calendar, SLOT 'setGridVisible(bool)');
    this->connect($navigationCheckBox, SIGNAL 'toggled(bool)',
            calendar, SLOT 'setNavigationBarVisible(bool)');
    this->connect($horizontalHeaderCombo, SIGNAL 'currentIndexChanged(int)',
            this, SLOT 'horizontalHeaderChanged(int)');
    this->connect($verticalHeaderCombo, SIGNAL 'currentIndexChanged(int)',
            this, SLOT 'verticalHeaderChanged(int)');
# [11]

    my $checkBoxLayout = Qt4::HBoxLayout();
    $checkBoxLayout->addWidget($gridCheckBox);
    $checkBoxLayout->addStretch();
    $checkBoxLayout->addWidget($navigationCheckBox);

    my $outerLayout = Qt4::GridLayout();
    $outerLayout->addWidget($localeLabel, 0, 0);
    $outerLayout->addWidget($localeCombo, 0, 1);
    $outerLayout->addWidget($firstDayLabel, 1, 0);
    $outerLayout->addWidget($firstDayCombo, 1, 1);
    $outerLayout->addWidget($selectionModeLabel, 2, 0);
    $outerLayout->addWidget($selectionModeCombo, 2, 1);
    $outerLayout->addLayout($checkBoxLayout, 3, 0, 1, 2);
    $outerLayout->addWidget($horizontalHeaderLabel, 4, 0);
    $outerLayout->addWidget($horizontalHeaderCombo, 4, 1);
    $outerLayout->addWidget($verticalHeaderLabel, 5, 0);
    $outerLayout->addWidget($verticalHeaderCombo, 5, 1);
    $generalOptionsGroupBox->setLayout($outerLayout);

# [12]
    this->firstDayChanged(this->firstDayCombo->currentIndex());
    this->selectionModeChanged(this->selectionModeCombo->currentIndex());
    this->horizontalHeaderChanged(this->horizontalHeaderCombo->currentIndex());
    this->verticalHeaderChanged(this->verticalHeaderCombo->currentIndex());
}
# [12]

# [13]
sub createDatesGroupBox {
    my $datesGroupBox = Qt4::GroupBox(this->tr('Dates'));
    this->{datesGroupBox} = $datesGroupBox;

    my $minimumDateEdit = Qt4::DateEdit();
    this->{minimumDateEdit} = $minimumDateEdit;
    $minimumDateEdit->setDisplayFormat('MMM d yyyy');
    $minimumDateEdit->setDateRange(this->calendar->minimumDate(),
                                  this->calendar->maximumDate());
    $minimumDateEdit->setDate(this->calendar->minimumDate());

    my $minimumDateLabel = Qt4::Label(this->tr('&Minimum Date:'));
    this->{minimumDateLabel} = $minimumDateLabel;
    $minimumDateLabel->setBuddy($minimumDateEdit);

    my $currentDateEdit = Qt4::DateEdit();
    this->{currentDateEdit} = $currentDateEdit;
    $currentDateEdit->setDisplayFormat('MMM d yyyy');
    $currentDateEdit->setDate(this->calendar->selectedDate());
    $currentDateEdit->setDateRange(this->calendar->minimumDate(),
                                  this->calendar->maximumDate());

    my $currentDateLabel = Qt4::Label(this->tr('&Current Date:'));
    this->{currentDateLabel} = $currentDateLabel;
    $currentDateLabel->setBuddy($currentDateEdit);

    my $maximumDateEdit = Qt4::DateEdit();
    this->{maximumDateEdit} = $maximumDateEdit;
    $maximumDateEdit->setDisplayFormat('MMM d yyyy');
    $maximumDateEdit->setDateRange(this->calendar->minimumDate(),
                                  this->calendar->maximumDate());
    $maximumDateEdit->setDate(this->calendar->maximumDate());

    my $maximumDateLabel = Qt4::Label(this->tr('Ma&ximum Date:'));
    this->{maximumDateLabel} = $maximumDateLabel;
    $maximumDateLabel->setBuddy($maximumDateEdit);

# [13] //! [14]
    this->connect(currentDateEdit, SIGNAL 'dateChanged(const QDate &)',
            calendar, SLOT 'setSelectedDate(const QDate &)');
    this->connect(calendar, SIGNAL 'selectionChanged()',
            this, SLOT 'selectedDateChanged()');
    this->connect(minimumDateEdit, SIGNAL 'dateChanged(const QDate &)',
            this, SLOT 'minimumDateChanged(const QDate &)');
    this->connect(maximumDateEdit, SIGNAL 'dateChanged(const QDate &)',
            this, SLOT 'maximumDateChanged(const QDate &)');

# [14]
    my $dateBoxLayout = Qt4::GridLayout();
    $dateBoxLayout->addWidget($currentDateLabel, 1, 0);
    $dateBoxLayout->addWidget($currentDateEdit, 1, 1);
    $dateBoxLayout->addWidget($minimumDateLabel, 0, 0);
    $dateBoxLayout->addWidget($minimumDateEdit, 0, 1);
    $dateBoxLayout->addWidget($maximumDateLabel, 2, 0);
    $dateBoxLayout->addWidget($maximumDateEdit, 2, 1);
    $dateBoxLayout->setRowStretch(3, 1);

    $datesGroupBox->setLayout($dateBoxLayout);
# [15]
}
# [15]

# [16]
sub createTextFormatsGroupBox {
    my $textFormatsGroupBox = Qt4::GroupBox(this->tr('Text Formats'));
    this->{textFormatsGroupBox} = $textFormatsGroupBox;

    my $weekdayColorCombo = this->createColorComboBox();
    this->{weekdayColorCombo} = $weekdayColorCombo;
    $weekdayColorCombo->setCurrentIndex(
            $weekdayColorCombo->findText(this->tr('Black')));

    my $weekdayColorLabel = Qt4::Label(this->tr('&Weekday color:'));
    this->{weekdayColorLabel} = $weekdayColorLabel;
    $weekdayColorLabel->setBuddy($weekdayColorCombo);

    my $weekendColorCombo = this->createColorComboBox();
    this->{weekendColorCombo} = $weekendColorCombo;
    $weekendColorCombo->setCurrentIndex(
            $weekendColorCombo->findText(this->tr('Red')));

    my $weekendColorLabel = Qt4::Label(this->tr('Week&end color:'));
    this->{weekendColorLabel} = $weekendColorLabel;
    $weekendColorLabel->setBuddy($weekendColorCombo);

# [16] //! [17]
    my $headerTextFormatCombo = Qt4::ComboBox();
    this->{headerTextFormatCombo} = $headerTextFormatCombo;
    $headerTextFormatCombo->addItem(this->tr('Bold'));
    $headerTextFormatCombo->addItem(this->tr('Italic'));
    $headerTextFormatCombo->addItem(this->tr('Plain'));

    my $headerTextFormatLabel = Qt4::Label(this->tr('&Header text:'));
    this->{headerTextFormatLabel} = $headerTextFormatLabel;
    $headerTextFormatLabel->setBuddy($headerTextFormatCombo);

    my $firstFridayCheckBox = Qt4::CheckBox(this->tr('&First Friday in blue'));
    this->{firstFridayCheckBox} = $firstFridayCheckBox;

    my $mayFirstCheckBox = Qt4::CheckBox(this->tr('May &1 in red'));
    this->{mayFirstCheckBox} = $mayFirstCheckBox;

# [17] //! [18]
    this->connect($weekdayColorCombo, SIGNAL 'currentIndexChanged(int)',
            this, SLOT 'weekdayFormatChanged()');
    this->connect($weekendColorCombo, SIGNAL 'currentIndexChanged(int)',
            this, SLOT 'weekendFormatChanged()');
    this->connect($headerTextFormatCombo, SIGNAL 'currentIndexChanged(const QString &)',
            this, SLOT 'reformatHeaders()');
    this->connect($firstFridayCheckBox, SIGNAL 'toggled(bool)',
            this, SLOT 'reformatCalendarPage()');
    this->connect($mayFirstCheckBox, SIGNAL 'toggled(bool)',
            this, SLOT 'reformatCalendarPage()');

# [18]
    my $checkBoxLayout = Qt4::HBoxLayout();
    $checkBoxLayout->addWidget($firstFridayCheckBox);
    $checkBoxLayout->addStretch();
    $checkBoxLayout->addWidget($mayFirstCheckBox);

    my $outerLayout = Qt4::GridLayout();
    $outerLayout->addWidget($weekdayColorLabel, 0, 0);
    $outerLayout->addWidget($weekdayColorCombo, 0, 1);
    $outerLayout->addWidget($weekendColorLabel, 1, 0);
    $outerLayout->addWidget($weekendColorCombo, 1, 1);
    $outerLayout->addWidget($headerTextFormatLabel, 2, 0);
    $outerLayout->addWidget($headerTextFormatCombo, 2, 1);
    $outerLayout->addLayout($checkBoxLayout, 3, 0, 1, 2);
    $textFormatsGroupBox->setLayout($outerLayout);

    this->weekdayFormatChanged();
    this->weekendFormatChanged();
# [19]
    this->reformatHeaders();
    this->reformatCalendarPage();
}
# [19]

# [20]
sub createColorComboBox {
    my $comboBox = Qt4::ComboBox();
    $comboBox->addItem(this->tr('Red'), Qt4::qVariantFromValue(Qt4::Color(Qt4::red())));
    $comboBox->addItem(this->tr('Blue'), Qt4::qVariantFromValue(Qt4::Color(Qt4::blue())));
    $comboBox->addItem(this->tr('Black'), Qt4::qVariantFromValue(Qt4::Color(Qt4::black())));
    $comboBox->addItem(this->tr('Magenta'), Qt4::qVariantFromValue(Qt4::Color(Qt4::magenta())));
    return $comboBox;
}
# [20]

1;
