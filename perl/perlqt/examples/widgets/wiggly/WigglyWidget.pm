package WigglyWidget;

use strict;
use warnings;
use blib;

use Qt4;

# [0]
use Qt4::isa qw( Qt4::Widget );
use Qt4::slots
    setText => ['QString'];

    #Qt4::BasicTimer timer;
    #Qt4::String text;
    #int step;
# [0]

my @sineTable = (
    0, 38, 71, 92, 100, 92, 71, 38,	0, -38, -71, -92, -100, -92, -71, -38
);

# [0]
sub NEW {
    my ( $class, $parent ) = @_;
    $class->SUPER::NEW( $parent );

    this->setBackgroundRole(Qt4::Palette::Midlight());
    this->setAutoFillBackground(1);

    my $newFont = this->font();
    $newFont->setPointSize($newFont->pointSize() + 20);
    this->setFont($newFont);

    this->{step} = 0;
    this->{timer} = Qt4::BasicTimer();
    this->{timer}->start(60, this);
}
# [0]

# [1]
sub paintEvent {
# [1] //! [2]
    my $metrics = Qt4::FontMetrics(this->font());
    my $x = (this->width() - $metrics->width(this->{text})) / 2;
    my $y = (this->height() + $metrics->ascent() - $metrics->descent()) / 2;
    my $color = Qt4::Color();
# [2]

# [3]
    my $painter = Qt4::Painter(this);
# [3] //! [4]
    my $text = this->{text};
    for (my $i = 0; $i < length $text; ++$i) {
        my $index = (this->{step} + $i) % 16;
        $color->setHsv((15 - $index) * 16, 255, 191);
        $painter->setPen($color);
        $painter->drawText($x, $y - (($sineTable[$index] * $metrics->height()) / 400),
                         substr( $text, $i, 1 ));
        $x += $metrics->width(substr( $text, $i, 1 ));
    }
    $painter->end();
}
# [4]

# [5]
sub timerEvent {
# [5] //! [6]
    my ( $event ) = @_;
    if ($event->timerId() == this->{timer}->timerId()) {
        ++this->{step};
        this->update();
    } else {
        this->SUPER::timerEvent($event);
    }
# [6]
}

sub setText {
    my ( $newText ) = @_;
    this->{text} = $newText;
}

1;